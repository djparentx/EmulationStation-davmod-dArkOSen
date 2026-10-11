#include "animations/StoryboardAnimator.h"

// Ported from AmberELEC ES (es-core/src/anim/StoryboardAnimator.cpp)

#include "GuiComponent.h"
#include "PowerSaver.h"
#include <cmath>

typedef ThemeData::ThemeElement::Property::PropertyType PropType;

/////////////////////////////////////////////////////////////////////////////////////////////
// StoryAnimation
/////////////////////////////////////////////////////////////////////////////////////////////

StoryAnimation::StoryAnimation(ThemeAnimation* anim)
{
	animation = anim;

	mRepeatCount = 0;
	mCurrentTime = 0;
	mIsReversed = false;
}

bool StoryAnimation::update(int elapsed)
{
	if (animation == nullptr)
		return false;

	if (mIsReversed)
		mCurrentTime -= elapsed;
	else
		mCurrentTime += elapsed;

	bool ended = false;
	bool pseudoEnd = false;

	if (!mIsReversed && mCurrentTime > animation->duration)
	{
		if (animation->autoReverse)
		{
			mIsReversed = true;
			mCurrentTime = animation->duration;
		}
		else
		{
			pseudoEnd = true;
			mCurrentTime = 0;
		}
	}
	else if (mIsReversed && mCurrentTime < 0)
	{
		mCurrentTime = 0;
		mIsReversed = false;
		pseudoEnd = true;
	}

	if (pseudoEnd)
	{
		if (animation->repeat == 1)
			ended = true;
		else if (animation->repeat > 1)
		{
			mRepeatCount++;
			if (mRepeatCount >= animation->repeat)
				ended = true;
			else
				mCurrentTime = 0;
		}
	}

	if (ended || animation->duration == 0)
	{
		currentValue = animation->computeValue(animation->autoReverse ? 0.0f : 1.0f);
		return false;
	}

	float value = 0;

	if (mCurrentTime >= animation->duration)
		value = 1;
	else
	{
		const float c = 1;

		float t = mCurrentTime / (float)animation->duration;
		if (t > 1)
			t = 1;

		switch (animation->easingMode)
		{
		case ThemeAnimation::EasingMode::EaseIn:
			value = t * t;
			break;

		case ThemeAnimation::EasingMode::EaseInCubic:
			value = t * t * t;
			break;

		case ThemeAnimation::EasingMode::EaseInQuint:
			value = t * t * t * t * t;
			break;

		case ThemeAnimation::EasingMode::EaseOut:
			value = (-c * t * (t - 2.0f));
			break;

		case ThemeAnimation::EasingMode::EaseOutCubic:
			t = t - 1.0f;
			value = t * t * t + 1.0f;
			break;

		case ThemeAnimation::EasingMode::EaseOutQuint:
			t = t - 1.0f;
			value = t * t * t * t * t + 1.0f;
			break;

		case ThemeAnimation::EasingMode::EaseInOut:
			t = mCurrentTime / ((float)animation->duration / 2.0f);
			if (t < 1)
				value = t * t / 2.0f;
			else
			{
				t--;
				value = (-c / 2.0f * (t * (t - 2.0f) - 1.0f));
			}
			break;

		case ThemeAnimation::EasingMode::Bump:
			value = (float)(sin((M_PI / 2.0) * t) + sin(M_PI * t) / 2.0);
			break;

		default:
			value = t;
			break;
		}
	}

	currentValue = animation->computeValue(value);

	return !ended;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// StoryboardAnimator
/////////////////////////////////////////////////////////////////////////////////////////////

StoryboardAnimator::StoryboardAnimator(GuiComponent* comp, const std::shared_ptr<ThemeStoryboard>& storyboard,
	const std::function<bool(const std::string&)>& enabledFn)
{
	mHasInitialProperties = false;
	mPaused = true;
	mComponent = comp;

	mStoryBoard = new ThemeStoryboard(*storyboard);
	mRepeatCount = 0;
	mCurrentTime = 0;

	evaluateEnabled(enabledFn);
}

StoryboardAnimator::~StoryboardAnimator()
{
	pause();
	clearStories();

	delete mStoryBoard;
}

void StoryboardAnimator::evaluateEnabled(const std::function<bool(const std::string&)>& enabledFn)
{
	for (auto anim : mStoryBoard->animations)
		if (!anim->enabledExpression.empty())
			anim->enabled = enabledFn ? enabledFn(anim->enabledExpression) : true;
}

void StoryboardAnimator::clearStories()
{
	for (auto story : mFinishedStories)
		delete story;

	for (auto story : mCurrentStories)
		delete story;

	mFinishedStories.clear();
	mCurrentStories.clear();
}

void StoryboardAnimator::reset(int atTime, bool resetInitialProperties)
{
	if (mPaused)
	{
		PowerSaver::pause();
		mPaused = false;
	}

	mCurrentTime = atTime;

	clearStories();

	if (atTime > 0)
	{
		for (auto anim : mStoryBoard->animations)
			if (anim->enabled && (anim->begin + anim->duration <= atTime))
				mFinishedStories.push_back(new StoryAnimation(anim));

		addNewAnimations();
	}
	else
	{
		mComponent->setProperty("sound", std::string()); // make sure sound is always reset

		if (mHasInitialProperties && resetInitialProperties)
		{
			for (auto prop : mInitialProperties)
			{
				if (mDisabledProperties.find(prop.first) != mDisabledProperties.cend())
					continue;

				bool hasAssignationAtZero = false;

				for (auto anim : mStoryBoard->animations)
					if (anim->enabled && anim->begin == 0 && anim->propertyName == prop.first)
						hasAssignationAtZero = true;

				if (!hasAssignationAtZero)
					mComponent->setProperty(prop.first, prop.second);
			}
		}

		for (auto anim : mStoryBoard->animations)
			if (anim->enabled && anim->begin == 0)
				mCurrentStories.push_back(new StoryAnimation(anim));
	}
}

void StoryboardAnimator::clearInitialProperties()
{
	mInitialProperties.clear();
}

void StoryboardAnimator::stop()
{
	pause();

	for (auto prop : mInitialProperties)
		if (mDisabledProperties.find(prop.first) == mDisabledProperties.cend())
			mComponent->setProperty(prop.first, prop.second);

	clearStories();
}

void StoryboardAnimator::pause()
{
	if (!mPaused)
	{
		PowerSaver::resume();
		mPaused = true;
	}
}

void StoryboardAnimator::addNewAnimations()
{
	for (auto anim : mStoryBoard->animations)
	{
		if (!anim->enabled)
			continue;

		bool exists = false;

		for (auto story : mCurrentStories)
			if (story->animation == anim) { exists = true; break; }

		if (!exists)
		{
			for (auto story : mFinishedStories)
				if (story->animation == anim) { exists = true; break; }
		}

		if (exists)
			continue;

		if (mCurrentTime >= anim->begin)
		{
			anim->ensureInitialValue(mComponent->getProperty(anim->propertyName));
			mCurrentStories.push_back(new StoryAnimation(anim));
		}
	}
}

bool StoryboardAnimator::update(int elapsed)
{
	if (mPaused || elapsed > 500)
		return true;

	if (!mHasInitialProperties)
	{
		mHasInitialProperties = true;

		for (auto anim : mStoryBoard->animations)
			if (anim->enabled && anim->propertyName != "sound") // sound is always initially empty
				mInitialProperties[anim->propertyName] = mComponent->getProperty(anim->propertyName);

		for (auto anim : mStoryBoard->animations)
		{
			if (anim->begin == 0 && anim->enabled)
			{
				if (anim->to.type == PropType::Unknown)
					anim->to = anim->propertyName == "sound" ? ThemeData::ThemeElement::Property(std::string()) : mComponent->getProperty(anim->propertyName);

				if (anim->from.type == PropType::Unknown)
					anim->from = anim->propertyName == "sound" ? ThemeData::ThemeElement::Property(std::string()) : mComponent->getProperty(anim->propertyName);
				else if (mDisabledProperties.find(anim->propertyName) == mDisabledProperties.cend())
					mComponent->setProperty(anim->propertyName, anim->from);
			}
		}
	}

	mCurrentTime += elapsed;

	addNewAnimations();

	for (int i = (int)mCurrentStories.size() - 1; i >= 0; i--)
	{
		auto story = mCurrentStories[i];
		bool ended = !story->update(elapsed);

		if (mDisabledProperties.find(story->animation->propertyName) == mDisabledProperties.cend())
			mComponent->setProperty(story->animation->propertyName, story->currentValue);

		if (ended)
		{
			if (story->animation->propertyName == "sound")
				mComponent->setProperty("sound", std::string());

			mFinishedStories.push_back(story);
			mCurrentStories.erase(mCurrentStories.begin() + i);
		}
	}

	// AmberELEC / Batocera: a storyboard holding a disabled animation never completes (and never repeats)
	if (mFinishedStories.size() == mStoryBoard->animations.size())
	{
		if (mStoryBoard->repeat == 1)
		{
			clearStories();
			pause();
			return false;
		}
		else if (mStoryBoard->repeat > 0)
		{
			mRepeatCount++;
			if (mRepeatCount >= mStoryBoard->repeat)
			{
				clearStories();
				pause();
				return false;
			}
		}

		reset(mStoryBoard->repeatAt, mStoryBoard->repeatAt == 0); // it's a repeat
		return true;
	}

	return true;
}

const std::string StoryboardAnimator::getName() const
{
	if (mStoryBoard != nullptr)
		return mStoryBoard->eventName;

	return "";
}

void StoryboardAnimator::enableProperty(const std::string& name, bool enable)
{
	mDisabledProperties.erase(name);

	if (!enable)
		mDisabledProperties.insert(name);
}
