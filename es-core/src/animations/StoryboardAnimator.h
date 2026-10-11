#pragma once
#ifndef ES_CORE_ANIMATIONS_STORYBOARD_ANIMATOR_H
#define ES_CORE_ANIMATIONS_STORYBOARD_ANIMATOR_H

// Ported from AmberELEC ES (es-core/src/anim/StoryboardAnimator.h)

#include "animations/ThemeStoryboard.h"
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class GuiComponent;

class StoryAnimation
{
public:
	StoryAnimation(ThemeAnimation* anim);

	bool update(int elapsed);

	ThemeData::ThemeElement::Property currentValue;
	ThemeAnimation* animation;

private:
	int  mRepeatCount;
	int  mCurrentTime;
	bool mIsReversed;
};

class StoryboardAnimator
{
public:
	// the storyboard is copied; enabledFn resolves <animation enabled="expression"> (nullptr = enabled)
	StoryboardAnimator(GuiComponent* comp, const std::shared_ptr<ThemeStoryboard>& storyboard,
		const std::function<bool(const std::string&)>& enabledFn = nullptr);
	~StoryboardAnimator();

	void reset(int atTime = 0, bool resetInitialProperties = true);
	void stop();
	bool update(int elapsed);
	void pause();

	bool isRunning() { return !mPaused; }
	void clearInitialProperties();
	void evaluateEnabled(const std::function<bool(const std::string&)>& enabledFn);

	const std::string getName();

	void enableProperty(const std::string& name, bool enable);

private:
	void addNewAnimations();
	void clearStories();

	GuiComponent*    mComponent;
	ThemeStoryboard* mStoryBoard;

	int  mRepeatCount;
	int  mCurrentTime;
	bool mPaused;

	std::vector<StoryAnimation*> mCurrentStories;
	std::vector<StoryAnimation*> mFinishedStories;
	std::map<std::string, ThemeData::ThemeElement::Property> mInitialProperties;
	std::set<std::string> mDisabledProperties;

	bool mHasInitialProperties;
};

#endif // ES_CORE_ANIMATIONS_STORYBOARD_ANIMATOR_H
