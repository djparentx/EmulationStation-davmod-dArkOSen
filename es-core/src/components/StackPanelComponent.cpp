#include "components/StackPanelComponent.h"

// Ported from AmberELEC ES (es-core/src/components/StackPanelComponent.cpp)

#include "Window.h"
#include "renderers/Renderer.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "math/Vector2i.h"
#include "math/Misc.h"

StackPanelComponent::StackPanelComponent(Window* window) : GuiComponent(window), mHorizontal(true), mReverse(false), mSeparator(0.0f)
{
	mClipChildren = true;
}

void StackPanelComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = parentTrans * getTransform();

	if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;

	if (mClipChildren)
		Renderer::pushClipRect(Vector2i((int)trans.translation().x(), (int)trans.translation().y()), Vector2i((int)mSize.x(), (int)mSize.y()));

	GuiComponent::renderChildren(trans);

	if (mClipChildren)
		Renderer::popClipRect();
}

void StackPanelComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "stackpanel");
	if (!elem)
		return;

	if (elem->has("orientation"))
		mHorizontal = (elem->get<std::string>("orientation") != "vertical");

	if (elem->has("reverse"))
		mReverse = elem->get<bool>("reverse");

	if (elem->has("separator"))
	{
		mSeparator = elem->get<float>("separator");
		if (mSeparator > 0 && mSeparator < 1)
		{
			Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
			mSeparator *= mHorizontal ? scale.y() : scale.x();
		}
	}

	performLayout();
}

void StackPanelComponent::recalcLayout()
{
	GuiComponent::recalcLayout();
	performLayout();
}

void StackPanelComponent::onSizeChanged()
{
	performLayout();
}

void StackPanelComponent::performLayout()
{
	float pos = 0;

	if (mReverse)
		pos = mHorizontal ? mSize.x() : mSize.y();

	// back to each child's own theme position / size, then stack them
	recalcChildrenLayout();

	bool aligned = false;

	for (auto child : mChildren)
	{
		if (!child->isVisible())
			continue;

		bool maxSize = false;

		ImageComponent* image = dynamic_cast<ImageComponent*>(child);
		if (image != nullptr)
			maxSize = image->getTargetIsMax();

		TextComponent* text = dynamic_cast<TextComponent*>(child);

		if (mHorizontal)
		{
			float sz = child->getSize().x();

			bool wasAligned = aligned;

			if (!wasAligned)
			{
				if (mReverse && pos - sz < 0)
				{
					wasAligned = true;
					sz = Math::max(0.0f, pos);
				}
				else if (!mReverse && pos + sz >= mSize.x())
				{
					wasAligned = true;
					sz = Math::max(0.0f, mSize.x() - pos);
				}
			}

			if (image != nullptr && maxSize)
				image->setMaxSize(child->getSize().x(), mSize.y());
			else if (text != nullptr)
			{
				child->setSize(0, mSize.y());
				child->setSize(Math::min(sz, child->getSize().x()), mSize.y());

				if (wasAligned && child->getSize().x() < sz)
					wasAligned = false;
			}
			else if (child->getSize() != Vector2f(sz, mSize.y()))
				child->setSize(sz, mSize.y());

			aligned |= wasAligned;

			if (mReverse)
			{
				float childpos = pos - child->getSize().x() + child->getSize().x() * child->getOrigin().x();

				if (image != nullptr && maxSize)
					child->setPosition(childpos, child->getPosition().y() - child->getSize().y() * child->getOrigin().y() + mSize.y() * child->getOrigin().y());
				else
					child->setPosition(childpos, child->getPosition().y());

				if (child->getSize().x() > 0)
					pos -= child->getSize().x() + mSeparator;
			}
			else
			{
				float childpos = pos + child->getSize().x() * child->getOrigin().x();

				if (image != nullptr && maxSize)
					child->setPosition(childpos, child->getPosition().y() - child->getSize().y() * child->getOrigin().y() + mSize.y() * child->getOrigin().y());
				else
					child->setPosition(childpos, child->getPosition().y());

				if (child->getSize().x() > 0)
					pos += child->getSize().x() + mSeparator;
			}
		}
		else
		{
			if (image != nullptr && maxSize)
				image->setMaxSize(mSize.x(), child->getSize().y());
			else if (child->getSize() != Vector2f(mSize.x(), child->getSize().y()))
				child->setSize(mSize.x(), child->getSize().y());

			if (mReverse)
			{
				float childpos = pos - child->getSize().y() + child->getSize().y() * child->getOrigin().y();

				if (image != nullptr && maxSize)
					child->setPosition(child->getPosition().x() - child->getSize().x() * child->getOrigin().x() + mSize.y() * child->getOrigin().x(), childpos);
				else
					child->setPosition(child->getPosition().x(), childpos);

				if (child->getSize().y() > 0)
					pos -= child->getSize().y() + mSeparator;
			}
			else
			{
				float childpos = pos + child->getSize().y() * child->getOrigin().y();

				if (image != nullptr && maxSize)
					child->setPosition(child->getPosition().x() - child->getSize().x() * child->getOrigin().x() + mSize.x() * child->getOrigin().x(), childpos);
				else
					child->setPosition(child->getPosition().x(), childpos);

				if (child->getSize().y() > 0)
					pos += child->getSize().y() + mSeparator;
			}
		}
	}
}

void StackPanelComponent::update(int deltaTime)
{
	Vector2f szBefore;
	for (auto child : mChildren)
		if (child->isVisible())
			szBefore = szBefore + child->getSize();

	GuiComponent::update(deltaTime);

	Vector2f szAfter;
	for (auto child : mChildren)
		if (child->isVisible())
			szAfter = szAfter + child->getSize();

	if (szBefore != szAfter)
		performLayout();
}
