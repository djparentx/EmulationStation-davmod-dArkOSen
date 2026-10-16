#include <string>
#include "GuiComponent.h"

#include "animations/Animation.h"
#include "animations/AnimationController.h"
#include "animations/LambdaAnimation.h"
#include "animations/StoryboardAnimator.h"
#include "animations/ThemeStoryboard.h"
#include "Log.h"
#include "Sound.h"
#include "renderers/Renderer.h"
#include "math/Vector2i.h"
#include "ThemeData.h"
#include "Window.h"
#include "utils/StringUtil.h"
#include <algorithm>
#include <cmath>

bool GuiComponent::ALLOWANIMATIONS = true;

GuiComponent::GuiComponent(Window* window) : mWindow(window), mParent(NULL), mOpacity(255),
	mPosition(Vector3f::Zero()), mOrigin(Vector2f::Zero()), mRotationOrigin(0.5, 0.5),
	mSize(Vector2f::Zero()), mTransform(Transform4x4f::Identity()), mIsProcessing(false), mVisible(true),
	mStaticExtra(false)
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		mAnimationMap[i] = NULL;
}

GuiComponent::~GuiComponent()
{
	mWindow->removeGui(this);

	if (mStoryboardAnimator != nullptr)
	{
		delete mStoryboardAnimator;
		mStoryboardAnimator = nullptr;
	}

	cancelAllAnimations();

	if(mParent)
		mParent->removeChild(this);

	// children created from nested theme elements are owned by this component (AmberELEC)
	for (int i = (int)getChildCount() - 1; i >= 0; i--)
	{
		GuiComponent* child = getChild(i);
		if (child->mThemedChild)
			delete child; // its destructor removes it from mChildren
		else
			child->setParent(NULL);
	}
}

bool GuiComponent::input(InputConfig* config, Input input)
{
	for(unsigned int i = 0; i < getChildCount(); i++)
	{
		if(getChild(i)->input(config, input))
			return true;
	}

	return false;
}

void GuiComponent::updateSelf(int deltaTime)
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		advanceAnimation(i, deltaTime);
}

void GuiComponent::updateChildren(int deltaTime)
{
	for(unsigned int i = 0; i < getChildCount(); i++)
	{
		getChild(i)->update(deltaTime);
	}
}

void GuiComponent::update(int deltaTime)
{
	if (mStoryboardAnimator != nullptr)
		mStoryboardAnimator->update(deltaTime);

	updateSelf(deltaTime);
	updateChildren(deltaTime);
}

void GuiComponent::setStoryboards(const std::map<std::string, std::shared_ptr<ThemeStoryboard>>& storyboards)
{
	if (mStoryboards.empty() && storyboards.empty())
		return;

	stopStoryboard();

	mStoryboards = storyboards;

	// no-event storyboard plays when the theme is applied, and again on every onShow().
	// Started without an immediate update (AmberELEC): the values it restores are captured on the
	// first frame, after the owner finished setting the component up (e.g. SystemView's systemInfo opacity 0)
	if (hasStoryboard(""))
	{
		mStoryboardAnimator = new StoryboardAnimator(this, mStoryboards[""]);
		mStoryboardAnimator->reset();
	}
}

bool GuiComponent::startStoryboard(const std::string& event, const std::function<bool(const std::string&)>& enabledFn, bool restoreInitialProperties)
{
	auto it = mStoryboards.find(event);
	if (it == mStoryboards.cend())
		return false;

	if (mStoryboardAnimator != nullptr && mStoryboardAnimator->getName() == event)
		mStoryboardAnimator->evaluateEnabled(enabledFn);
	else
	{
		if (mStoryboardAnimator != nullptr)
		{
			if (restoreInitialProperties)
				mStoryboardAnimator->reset(); // restores the properties the previous storyboard changed
			else
			{
				mStoryboardAnimator->clearInitialProperties(); // keep the current values
				mStoryboardAnimator->stop();
			}

			delete mStoryboardAnimator;
		}

		mStoryboardAnimator = new StoryboardAnimator(this, it->second, enabledFn);
	}

	mStoryboardAnimator->reset(0, restoreInitialProperties);
	mStoryboardAnimator->update(0); // apply begin=0 values now, avoids a 1-frame flash of the base state
	return true;
}

void GuiComponent::stopStoryboard()
{
	if (mStoryboardAnimator == nullptr)
		return;

	mStoryboardAnimator->stop();
	delete mStoryboardAnimator;
	mStoryboardAnimator = nullptr;
}

void GuiComponent::pauseStoryboard()
{
	if (mStoryboardAnimator != nullptr)
		mStoryboardAnimator->pause();
}

bool GuiComponent::isStoryboardSelected(const std::string& event) const
{
	return mStoryboardAnimator != nullptr && mStoryboardAnimator->getName() == event;
}

bool GuiComponent::storyboardHasProperty(const std::string& event, const std::string& propertyName) const
{
	auto it = mStoryboards.find(event);
	if (it == mStoryboards.cend() || it->second == nullptr)
		return false;

	for (auto anim : it->second->animations)
		if (anim->propertyName == propertyName)
			return true;

	return false;
}

void GuiComponent::enableStoryboardProperty(const std::string& name, bool enable)
{
	if (mStoryboardAnimator != nullptr)
		mStoryboardAnimator->enableProperty(name, enable);
}

bool GuiComponent::isStoryboardRunning(const std::string& event)
{
	if (mStoryboardAnimator == nullptr)
		return false;

	if (!event.empty() && mStoryboardAnimator->getName() != event)
		return false;

	return mStoryboardAnimator->isRunning();
}

void GuiComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible() || mChildren.empty())
		return;

	Transform4x4f trans = parentTrans * getTransform();

	// AmberELEC <clipChildren> / <clipRect> (containers)
	bool clip = false;
	if (mClipChildren)
	{
		Vector3f dim = trans * Vector3f(mSize.x(), mSize.y(), 0) - trans.translation();
		Renderer::pushClipRect(Vector2i((int)Math::round(trans.translation().x()), (int)Math::round(trans.translation().y())), Vector2i((int)Math::round(dim.x()), (int)Math::round(dim.y())));
		clip = true;
	}
	else if (mClipRect.z() > 0 && mClipRect.w() > 0)
	{
		Renderer::pushClipRect(Vector2i((int)mClipRect.x(), (int)mClipRect.y()), Vector2i((int)mClipRect.z(), (int)mClipRect.w()));
		clip = true;
	}

	renderChildren(trans);

	if (clip)
		Renderer::popClipRect();
}

void GuiComponent::renderChildren(const Transform4x4f& transform) const
{
	for(unsigned int i = 0; i < getChildCount(); i++)
	{
		getChild(i)->render(transform);
	}
}

Vector3f GuiComponent::getPosition() const
{
	return mPosition;
}

void GuiComponent::setPosition(float x, float y, float z)
{
	mPosition = Vector3f(x, y, z);
	onPositionChanged();
}

Vector2f GuiComponent::getOrigin() const
{
	return mOrigin;
}

void GuiComponent::setOrigin(float x, float y)
{
	mOrigin = Vector2f(x, y);
	onOriginChanged();
}

Vector2f GuiComponent::getRotationOrigin() const
{
	return mRotationOrigin;
}

void GuiComponent::setRotationOrigin(float x, float y)
{
	mRotationOrigin = Vector2f(x, y);
}

Vector2f GuiComponent::getSize() const
{
	return mSize;
}

void GuiComponent::setSize(float w, float h)
{
	Vector4f oldClientRect = getClientRect();

	mSize = Vector2f(w, h);
	onSizeChanged();

	if (!mChildren.empty() && oldClientRect != getClientRect())
		recalcChildrenLayout();
}

void GuiComponent::setPadding(const Vector4f padding)
{
	if (mPadding == padding)
		return;

	Vector4f oldClientRect = getClientRect();

	mPadding = padding;
	onPaddingChanged();

	if (!mChildren.empty() && oldClientRect != getClientRect())
		recalcChildrenLayout();
}

Vector4f GuiComponent::getClientRect()
{
	return Vector4f(mPadding.x(), mPadding.y(),
		getSize().x() - mPadding.x() - mPadding.z(),
		getSize().y() - mPadding.y() - mPadding.w());
}

Vector4f GuiComponent::getParentClientRect() const
{
	if (mParent != nullptr)
		return mParent->getClientRect();

	return Vector4f(0, 0, (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
}

void GuiComponent::recalcChildrenLayout()
{
	for (auto child : mChildren)
		child->recalcLayout();
}

// nested theme elements follow their parent's client area (AmberELEC)
void GuiComponent::recalcLayout()
{
	if (!mThemedChild || mParent == nullptr)
		return;

	Vector4f clientRect = mParent->getClientRect();
	Vector2f newPos = Vector2f(mSourceBounds.x() * clientRect.z() + clientRect.x(), mSourceBounds.y() * clientRect.w() + clientRect.y());
	Vector2f newSize = Vector2f(mSourceBounds.z() * clientRect.z(), mSourceBounds.w() * clientRect.w());

	if (Vector2f(mPosition.x(), mPosition.y()) != newPos)
		setPosition(newPos.x(), newPos.y(), mPosition.z());

	if (mSize != newSize)
		setSize(newSize);
}

float GuiComponent::getRotation() const
{
	return mRotation;
}

void GuiComponent::setRotation(float rotation)
{
	mRotation = rotation;
}

Vector3f GuiComponent::getScale() const
{
	return mScale;
}

void GuiComponent::setScale(Vector3f scale)
{
	mScale = scale;
}

float GuiComponent::getZIndex() const
{
	return mZIndex;
}

void GuiComponent::setZIndex(float z)
{
	mZIndex = z;
}

float GuiComponent::getDefaultZIndex() const
{	
	return mDefaultZIndex;
}

void GuiComponent::setDefaultZIndex(float z)
{
	mDefaultZIndex = z;
}

bool GuiComponent::isVisible() const
{
	return mVisible;
}
void GuiComponent::setVisible(bool visible)
{
	mVisible = visible;
}

Vector2f GuiComponent::getCenter() const
{
	return Vector2f(mPosition.x() - (getSize().x() * mOrigin.x()) + getSize().x() / 2,
	                mPosition.y() - (getSize().y() * mOrigin.y()) + getSize().y() / 2);
}

bool GuiComponent::isChild(GuiComponent* cmp)
{
	for (auto i = mChildren.cbegin(); i != mChildren.cend(); i++)
		if (*i == cmp)
			return true;
	
	return false;
}

//Children stuff.
void GuiComponent::addChild(GuiComponent* cmp)
{
	mChildren.push_back(cmp);

	if(cmp->getParent())
		cmp->getParent()->removeChild(cmp);

	cmp->setParent(this);
}

void GuiComponent::removeChild(GuiComponent* cmp)
{
	if(!cmp->getParent())
		return;

	if(cmp->getParent() != this)
	{
		LOG(LogError) << "Tried to remove child from incorrect parent!";
	}

	cmp->setParent(NULL);

	for(auto i = mChildren.cbegin(); i != mChildren.cend(); i++)
	{
		if(*i == cmp)
		{
			mChildren.erase(i);
			return;
		}
	}
}

void GuiComponent::clearChildren()
{
	mChildren.clear();
}

void GuiComponent::sortChildren()
{
	std::stable_sort(mChildren.begin(), mChildren.end(),  [](GuiComponent* a, GuiComponent* b) {
		return b->getZIndex() > a->getZIndex();
	});
}

unsigned int GuiComponent::getChildCount() const
{
	return (int)mChildren.size();
}

GuiComponent* GuiComponent::getChild(unsigned int i) const
{
	return mChildren.at(i);
}

void GuiComponent::setParent(GuiComponent* parent)
{
	mParent = parent;
}

GuiComponent* GuiComponent::getParent() const
{
	return mParent;
}

unsigned char GuiComponent::getOpacity() const
{
	return mOpacity;
}

void GuiComponent::setOpacity(unsigned char opacity)
{
	mOpacity = opacity;
	for(auto it = mChildren.cbegin(); it != mChildren.cend(); it++)
	{
		// nested theme elements keep their own <opacity>, multiplied by the parent's (AmberELEC)
		if ((*it)->mThemedChild)
			(*it)->setOpacity((unsigned char)(((int)(*it)->mThemeOpacity * (int)opacity) / 255));
		else
			(*it)->setOpacity(opacity);
	}
}

const Transform4x4f& GuiComponent::getTransform()
{
	mTransform = Transform4x4f::Identity();
	mTransform.translate(mPosition);

	if (mScreenOffset.x() != 0.0f || mScreenOffset.y() != 0.0f)
		mTransform.translate(Vector3f(mScreenOffset.x(), mScreenOffset.y(), 0.0f));

	if (mScale != 1.0)
	{
		const float xOff = mSize.x() * mScaleOrigin.x();
		const float yOff = mSize.y() * mScaleOrigin.y();

		if (xOff != 0.0f || yOff != 0.0f)
			mTransform.translate(Vector3f(xOff, yOff, 0.0f));

		mTransform.scale(mScale);

		if (xOff != 0.0f || yOff != 0.0f)
			mTransform.translate(Vector3f(-xOff, -yOff, 0.0f));
	}
	if (mRotation != 0.0)
	{
		// Calculate offset as difference between origin and rotation origin
		Vector2f rotationSize = getRotationSize();
		float xOff = (mOrigin.x() - mRotationOrigin.x()) * rotationSize.x();
		float yOff = (mOrigin.y() - mRotationOrigin.y()) * rotationSize.y();

		// transform to offset point
		if (xOff != 0.0 || yOff != 0.0)
			mTransform.translate(Vector3f(xOff * -1, yOff * -1, 0.0f));

		// apply rotation transform
		mTransform.rotateZ(mRotation);

		// Tranform back to original point
		if (xOff != 0.0 || yOff != 0.0)
			mTransform.translate(Vector3f(xOff, yOff, 0.0f));
	}
	mTransform.translate(Vector3f(mOrigin.x() * mSize.x() * -1, mOrigin.y() * mSize.y() * -1, 0.0f));
	return mTransform;
}

void GuiComponent::setValue(const std::string& /*value*/)
{
}

std::string GuiComponent::getValue() const
{
	return "";
}

void GuiComponent::setTag(const std::string& value)
{
	mTag = value;
}

std::string GuiComponent::getTag() const
{
	return mTag;
}

void GuiComponent::textInput(const char* text)
{
	for(auto iter = mChildren.cbegin(); iter != mChildren.cend(); iter++)
	{
		(*iter)->textInput(text);
	}
}

void GuiComponent::setAnimation(Animation* anim, int delay, std::function<void()> finishedCallback, bool reverse, unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);

	AnimationController* oldAnim = mAnimationMap[slot];
	mAnimationMap[slot] = new AnimationController(anim, delay, finishedCallback, reverse);

	if(oldAnim)
		delete oldAnim;
}

bool GuiComponent::stopAnimation(unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);
	if(mAnimationMap[slot])
	{
		delete mAnimationMap[slot];
		mAnimationMap[slot] = NULL;
		return true;
	}else{
		return false;
	}
}

bool GuiComponent::cancelAnimation(unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);
	if(mAnimationMap[slot])
	{
		mAnimationMap[slot]->removeFinishedCallback();
		delete mAnimationMap[slot];
		mAnimationMap[slot] = NULL;
		return true;
	}else{
		return false;
	}
}

bool GuiComponent::finishAnimation(unsigned char slot)
{
	assert(slot < MAX_ANIMATIONS);
	if(mAnimationMap[slot])
	{
		// skip to animation's end
		const bool done = mAnimationMap[slot]->update(mAnimationMap[slot]->getAnimation()->getDuration() - mAnimationMap[slot]->getTime());
		assert(done);

		delete mAnimationMap[slot]; // will also call finishedCallback
		mAnimationMap[slot] = NULL;
		return true;
	}else{
		return false;
	}
}

bool GuiComponent::advanceAnimation(unsigned char slot, unsigned int time)
{
	assert(slot < MAX_ANIMATIONS);
	AnimationController* anim = mAnimationMap[slot];
	if(anim)
	{
		bool done = anim->update(time);
		if(done)
		{
			mAnimationMap[slot] = NULL;
			delete anim;
		}
		return true;
	}else{
		return false;
	}
}

void GuiComponent::stopAllAnimations()
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		stopAnimation(i);
}

void GuiComponent::cancelAllAnimations()
{
	for(unsigned char i = 0; i < MAX_ANIMATIONS; i++)
		cancelAnimation(i);
}

bool GuiComponent::isAnimationPlaying(unsigned char slot) const
{
	return mAnimationMap[slot] != NULL;
}

bool GuiComponent::isAnimationReversed(unsigned char slot) const
{
	assert(mAnimationMap[slot] != NULL);
	return mAnimationMap[slot]->isReversed();
}

int GuiComponent::getAnimationTime(unsigned char slot) const
{
	assert(mAnimationMap[slot] != NULL);
	return mAnimationMap[slot]->getTime();
}

void GuiComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	// normalized coordinates are relative to the parent's client area (AmberELEC)
	Vector4f clientRect = getParentClientRect();
	Vector2f scale = Vector2f(clientRect.z(), clientRect.w());
	Vector2f offset = Vector2f(clientRect.x(), clientRect.y());

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "");
	if(!elem)
		return;

	setStoryboards(elem->storyboards);
	loadBindingExpressions(elem);

	// AmberELEC default scale pivot for themed components is 0.5 0.5 (image / video: 0 0)
	setScaleOrigin(elem->has("scaleOrigin") ? elem->get<Vector2f>("scaleOrigin") : Vector2f(0.5f, 0.5f));

	using namespace ThemeFlags;
	if(properties & POSITION && elem->has("pos"))
	{
		Vector2f pos = elem->get<Vector2f>("pos");
		mSourceBounds.x() = pos.x(); mSourceBounds.y() = pos.y();
		Vector2f denormalized = pos * scale + offset;
		setPosition(Vector3f(denormalized.x(), denormalized.y(), 0));
	}

	if(properties & ThemeFlags::SIZE && elem->has("size"))
	{
		Vector2f size = elem->get<Vector2f>("size");
		mSourceBounds.z() = size.x(); mSourceBounds.w() = size.y();
		setSize(size * scale);
	}

	if (properties & POSITION && elem->has("x"))
	{
		mSourceBounds.x() = elem->get<float>("x");
		float x = mSourceBounds.x() * scale.x() + offset.x();
		setPosition(Vector3f(x, mPosition.y(), mPosition.z()));
	}

	if (properties & POSITION && elem->has("y"))
	{
		mSourceBounds.y() = elem->get<float>("y");
		float y = mSourceBounds.y() * scale.y() + offset.y();
		setPosition(Vector3f(mPosition.x(), y, mPosition.z()));
	}

	if (properties & ThemeFlags::SIZE && elem->has("w"))
	{
		mSourceBounds.z() = elem->get<float>("w");
		setSize(Vector2f(mSourceBounds.z() * scale.x(), mSize.y()));
	}

	if (properties & ThemeFlags::SIZE && elem->has("h"))
	{
		mSourceBounds.w() = elem->get<float>("h");
		setSize(Vector2f(mSize.x(), mSourceBounds.w() * scale.y()));
	}

	// position + size also implies origin
	if((properties & ORIGIN || (properties & POSITION && properties & ThemeFlags::SIZE)) && elem->has("origin"))
		setOrigin(elem->get<Vector2f>("origin"));

	if(properties & ThemeFlags::ROTATION) {
		if(elem->has("rotation"))
			setRotationDegrees(elem->get<float>("rotation"));
		if(elem->has("rotationOrigin"))
			setRotationOrigin(elem->get<Vector2f>("rotationOrigin"));
	}

	if(properties & ThemeFlags::Z_INDEX && elem->has("zIndex"))
		setZIndex(elem->get<float>("zIndex"));
	else
		setZIndex(getDefaultZIndex());

	if(properties & ThemeFlags::VISIBLE && elem->has("visible"))
		setVisible(elem->get<bool>("visible"));
	else
		setVisible(true);

	applyThemeOpacityAndScale(elem, properties);
	applyThemeLayout(elem, properties);
}

void GuiComponent::applyThemeLayout(const ThemeData::ThemeElement* elem, unsigned int properties)
{
	if (elem == nullptr)
		return;

	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	applyThemePadding(elem);

	if (properties & ThemeFlags::POSITION)
	{
		Vector2f screenOffset = Vector2f::Zero();

		if (elem->has("offset"))
			screenOffset = elem->get<Vector2f>("offset") * screen;

		if (elem->has("offsetX"))
			screenOffset.x() = elem->get<float>("offsetX") * screen.x();

		if (elem->has("offsetY"))
			screenOffset.y() = elem->get<float>("offsetY") * screen.y();

		setScreenOffset(screenOffset);

		if (elem->has("clipRect"))
		{
			Vector4f rect = elem->get<Vector4f>("clipRect");
			setClipRect(Vector4f(rect.x() * screen.x(), rect.y() * screen.y(), rect.z() * screen.x(), rect.w() * screen.y()));
		}
		else
			setClipRect(Vector4f(0, 0, 0, 0));
	}

	if (elem->has("clipChildren"))
		setClipChildren(elem->get<bool>("clipChildren"));

	loadThemedChildren(elem);
}

void GuiComponent::applyThemePadding(const ThemeData::ThemeElement* elem)
{
	if (elem == nullptr || !elem->has("padding"))
		return;

	Vector4f clientRect = getParentClientRect();
	Vector4f padding = elem->get<Vector4f>("padding");

	// values below 1 are relative to the parent's client area, others are pixels
	if (std::abs(padding.x()) < 1 && std::abs(padding.y()) < 1 && std::abs(padding.z()) < 1 && std::abs(padding.w()) < 1)
		setPadding(Vector4f(padding.x() * clientRect.z(), padding.y() * clientRect.w(), padding.z() * clientRect.z(), padding.w() * clientRect.w()));
	else
		setPadding(padding);
}

// nested theme elements: <container name="x"><image name="y">...</image></container> (AmberELEC)
void GuiComponent::loadThemedChildren(const ThemeData::ThemeElement* elem)
{
	for (int i = (int)mChildren.size() - 1; i >= 0; i--)
	{
		GuiComponent* child = mChildren[i];
		if (child->mThemedChild)
			delete child; // its destructor removes it from mChildren
	}

	if (elem == nullptr || elem->children.empty())
		return;

	for (const auto& child : elem->children)
	{
		GuiComponent* comp = ThemeData::createExtraComponent(mWindow, child.second.type);
		if (comp == nullptr)
			continue;

		ThemeData::ThemeElement childElem = child.second;

		// default to the parent's whole client area
		if (!childElem.has("pos") && !childElem.has("x") && !childElem.has("y"))
			childElem.properties["pos"] = Vector2f(0, 0);

		if (!childElem.has("size") && !childElem.has("minSize") && !childElem.has("maxSize") && !childElem.has("w") && !childElem.has("h"))
			childElem.properties["size"] = Vector2f(1, 1);

		comp->mThemedChild = true;
		comp->setTag(child.first);
		comp->setDefaultZIndex(10);
		addChild(comp);
		ThemeData::applySelfTheme(comp, childElem);

		// combine with this component's opacity
		if (mOpacity != 255)
			comp->setOpacity((unsigned char)(((int)comp->mThemeOpacity * (int)mOpacity) / 255));
	}

	sortChildren();
}

// AmberELEC: <opacity> and <scale> (scale with the ROTATION flag, like rotation)
void GuiComponent::applyThemeOpacityAndScale(const ThemeData::ThemeElement* elem, unsigned int properties)
{
	if (elem == nullptr)
		return;

	if ((properties & ThemeFlags::ROTATION) && elem->has("scale"))
	{
		float scale = elem->get<float>("scale");
		setScale(Vector3f(scale, scale, 1.0f));
	}

	if (elem->has("opacity"))
	{
		mThemeOpacity = (unsigned char)(Math::clamp(elem->get<float>("opacity"), 0.0f, 1.0f) * 255.0f);
		setOpacity(mThemeOpacity);
	}
	else
		mThemeOpacity = 255;
}

void GuiComponent::loadBindingExpressions(const ThemeData::ThemeElement* elem)
{
	mBindingExpressions.clear();

	if (elem == nullptr)
		return;

	static const std::string suffix = "_binding";

	for (const auto& prop : elem->properties)
		if (prop.second.type == ThemeData::ThemeElement::Property::PropertyType::String && Utils::String::endsWith(prop.first, suffix))
			mBindingExpressions[prop.first.substr(0, prop.first.size() - suffix.size())] = prop.second.s;
}

ThemeData::ThemeElement::Property GuiComponent::getProperty(const std::string name)
{
	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector4f clientRect = getParentClientRect();
	Vector2f scale = Vector2f(clientRect.z(), clientRect.w());

	if (name == "pos")
		return Vector2f((mPosition.x() - clientRect.x()) / scale.x(), (mPosition.y() - clientRect.y()) / scale.y());

	if (name == "x")
		return (mPosition.x() - clientRect.x()) / scale.x();

	if (name == "y")
		return (mPosition.y() - clientRect.y()) / scale.y();

	if (name == "offset")
		return Vector2f(mScreenOffset.x() / screen.x(), mScreenOffset.y() / screen.y());

	if (name == "offsetX")
		return mScreenOffset.x() / screen.x();

	if (name == "offsetY")
		return mScreenOffset.y() / screen.y();

	if (name == "padding")
		return mPadding;

	if (name == "size")
		return Vector2f(mSize.x() / scale.x(), mSize.y() / scale.y());

	if (name == "w")
		return mSize.x() / scale.x();

	if (name == "h")
		return mSize.y() / scale.y();

	if (name == "origin")
		return getOrigin();

	if (name == "rotation")
		return (float)ES_RAD_TO_DEG(getRotation());

	if (name == "rotationOrigin")
		return getRotationOrigin();

	if (name == "scaleOrigin")
		return getScaleOrigin();

	if (name == "opacity")
		return mOpacity / 255.0f;

	if (name == "zIndex")
		return getZIndex();

	if (name == "scale")
		return mScale.x();

	if (name == "visible")
		return mVisible;

	if (name == "sound")
		return mStoryboardSound;

	ThemeData::ThemeElement::Property unk;
	unk.type = ThemeData::ThemeElement::Property::PropertyType::Unknown;
	return unk;
}

void GuiComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	typedef ThemeData::ThemeElement::Property::PropertyType PropType;

	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector4f clientRect = getParentClientRect();
	Vector2f scale = Vector2f(clientRect.z(), clientRect.w());

	if (name == "sound" && value.type == PropType::String)
	{
		if (mStoryboardSound != value.s)
		{
			mStoryboardSound = value.s;
			if (!mStoryboardSound.empty())
				Sound::get(mStoryboardSound)->play();
		}

		return;
	}

	switch (value.type)
	{
	case PropType::Pair:
		if (name == "pos")
			setPosition(Vector3f(value.v.x() * scale.x() + clientRect.x(), value.v.y() * scale.y() + clientRect.y(), 0));
		else if (name == "offset")
			setScreenOffset(Vector2f(value.v.x() * screen.x(), value.v.y() * screen.y()));
		else if (name == "size")
			setSize(Vector2f(value.v.x() * scale.x(), value.v.y() * scale.y()));
		else if (name == "origin")
			setOrigin(value.v);
		else if (name == "rotationOrigin")
			setRotationOrigin(value.v);
		else if (name == "scaleOrigin")
			setScaleOrigin(value.v);
		break;

	case PropType::Float:
		if (name == "x")
			setPosition(Vector3f(value.f * scale.x() + clientRect.x(), mPosition.y(), mPosition.z()));
		else if (name == "y")
			setPosition(Vector3f(mPosition.x(), value.f * scale.y() + clientRect.y(), mPosition.z()));
		else if (name == "offsetX")
			setScreenOffset(Vector2f(value.f * screen.x(), mScreenOffset.y()));
		else if (name == "offsetY")
			setScreenOffset(Vector2f(mScreenOffset.x(), value.f * screen.y()));
		else if (name == "w")
			setSize(Vector2f(value.f * scale.x(), mSize.y()));
		else if (name == "h")
			setSize(Vector2f(mSize.x(), value.f * scale.y()));
		else if (name == "rotation")
			setRotationDegrees(value.f);
		else if (name == "zIndex")
			setZIndex(value.f);
		else if (name == "opacity")
			setOpacity((unsigned char)(Math::clamp(value.f, 0.0f, 1.0f) * 255.0f));
		else if (name == "scale")
			setScale(Vector3f(value.f, value.f, 1.0f));
		break;

	case PropType::Bool:
		if (name == "visible")
			setVisible(value.b);
		break;

	case PropType::Rect:
		if (name == "padding")
			setPadding(value.r);
		break;

	default:
		break;
	}
}

void GuiComponent::updateHelpPrompts()
{
	if(getParent())
	{
		getParent()->updateHelpPrompts();
		return;
	}

	std::vector<HelpPrompt> prompts = getHelpPrompts();

	if(mWindow->peekGui() == this)
		mWindow->setHelpPrompts(prompts, getHelpStyle());
}

HelpStyle GuiComponent::getHelpStyle()
{
	HelpStyle style = HelpStyle();

	if (ThemeData::getDefaultTheme() != nullptr)
	{
		std::shared_ptr<ThemeData> theme = std::shared_ptr<ThemeData>(ThemeData::getDefaultTheme(), [](ThemeData*) {});
		style.applyTheme(theme, "system");
	}

	return style;
}

bool GuiComponent::isProcessing() const
{
	return mIsProcessing;
}

void GuiComponent::onShow()
{
	if (mStoryboardAnimator != nullptr)
		mStoryboardAnimator->reset();

	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->onShow();
}

void GuiComponent::onHide()
{
	if (mStoryboardAnimator != nullptr)
		mStoryboardAnimator->pause();

	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->onHide();
}

void GuiComponent::onScreenSaverActivate()
{
	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->onScreenSaverActivate();
}

void GuiComponent::onScreenSaverDeactivate()
{
	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->onScreenSaverDeactivate();
}

void GuiComponent::topWindow(bool isTop)
{
	for(unsigned int i = 0; i < getChildCount(); i++)
		getChild(i)->topWindow(isTop);
}

void GuiComponent::animateTo(Vector2f from, Vector2f to, unsigned int  flags, int delay)
{
	if ((flags & AnimateFlags::POSITION) == 0)
		from = to;

	Vector3f scale = mScale;	

	float opacity = mOpacity;

	float x1 = from.x();
	float x2 = to.x();
	float y1 = from.y();
	float y2 = to.y();

	if (Settings::getInstance()->getString("PowerSaverMode") == "instant" || Settings::getInstance()->getString("TransitionStyle") == "instant")
		setPosition(x2, y2);
	else
	{
		setPosition(x1, y1);

		if ((flags & AnimateFlags::OPACITY) == AnimateFlags::OPACITY)
			setOpacity(0);

		if ((flags & AnimateFlags::SCALE) == AnimateFlags::SCALE)
			mScale = Vector3f(0, 0, 1);

		auto fadeFunc = [this, x1, x2, y1, y2, flags, scale, opacity](float t) {

			t -= 1; // cubic ease out
			float pct = Math::lerp(0, 1, t*t*t + 1);

			if ((flags & AnimateFlags::OPACITY) == AnimateFlags::OPACITY)
				setOpacity(pct * opacity);

			if ((flags & AnimateFlags::SCALE) == AnimateFlags::SCALE)
				mScale = Vector3f(pct * scale.x(), pct * scale.y(), pct * scale.z());

			float x = (x1 + mSize.x() / 2 - (mSize.x() / 2 * mScale.x())) * (1 - pct) + (x2 + mSize.x() / 2 - (mSize.x() / 2 * mScale.x())) * pct;
			float y = (y1 + mSize.x() / 2 - (mSize.y() / 2 * mScale.y())) * (1 - pct) + (y2 + mSize.y() / 2 - (mSize.y() / 2 * mScale.y())) * pct;

			if (mScale.x() != 0.0f)
				setPosition(x, y);
		};

		setAnimation(new LambdaAnimation(fadeFunc, delay), 0, [this, fadeFunc, x2, y2, flags, scale, opacity]
		{
			if ((flags & AnimateFlags::SCALE) == AnimateFlags::SCALE)
				mScale = scale;

			if ((flags & AnimateFlags::OPACITY) == AnimateFlags::OPACITY)
				setOpacity(opacity);

			float x = x2 + mSize.x() / 2 - (mSize.x() / 2 * mScale.x());
			float y = y2 + mSize.y() / 2 - (mSize.y() / 2 * mScale.y());

			setPosition(x, y);
		});
	}
}
