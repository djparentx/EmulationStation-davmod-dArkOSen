#include "components/CarouselComponent.h"

#include "animations/LambdaAnimation.h"
#include "components/ImageComponent.h"
#include "components/ItemTemplateComponent.h"
#include "components/TextComponent.h"
#include "utils/FileSystemUtil.h"
#include "BindingManager.h"
#include "EsLocale.h"
#include "FileData.h"
#include "Settings.h"
#include "Sound.h"
#include "ThemeData.h"

// buffer values for scrolling velocity (left, stopped, right)
static const int carouselBuffersLeft[] = { -5, -2, -1 };
static const int carouselBuffersRight[] = { 1, 2, 5 };

CarouselComponent::CarouselComponent(Window* window) :
	IList<CarouselComponentData, FileData*>(window, LIST_SCROLL_STYLE_SLOW, LIST_ALWAYS_LOOP)
{
	mColor = mColorEnd = 0;
	mColorGradientHorizontal = false;

	mThemeViewName = "gamecarousel";

	mWasRendered = false;
	mCamOffset = 0;
	mLastCursor = 0;

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mType = GameCarouselType::VERTICAL;
	mLogoAlignment = ALIGN_CENTER;
	mLogoScale = 1.2f;
	mLogoRotation = 7.5;
	mLogoRotationOrigin.x() = -5;
	mLogoRotationOrigin.y() = 0.5;
	mLogoSize.x() = 0.25f * mSize.x();
	mLogoSize.y() = 0.155f * mSize.y();
	mLogoPos = Vector2f(-1, -1);
	mMaxLogoCount = 3;
	mScrollSound = "";
	mDefaultTransition = "";
	mTransitionSpeed = 500;
	mMinLogoOpacity = 0.5f;
	mScaledSpacing = 0.0f;
	mImageSource = CarouselImageSource::THUMBNAIL;

	mAnyLogoHasScaleStoryboard = false;
	mAnyLogoHasOpacityStoryboard = false;
}

CarouselComponent::~CarouselComponent()
{
	clearEntries();
}

void CarouselComponent::clearEntries()
{
	mWasRendered = false;
	clear();
}

int CarouselComponent::moveCursorFast(bool forward)
{
	int value = mCursor;
	int count = forward ? 10 : -10;
	int sz = (int)mEntries.size();

	if (count >= sz)
		return value;

	value += count;
	if (value < 0)
		value += sz;
	if (value >= sz)
		value -= sz;

	return value;
}

bool CarouselComponent::input(InputConfig* config, Input input)
{
	if (input.value != 0)
	{
		const bool vertical = (mType == GameCarouselType::VERTICAL || mType == GameCarouselType::VERTICAL_WHEEL);

		if (config->isMappedLike(vertical ? "up" : "left", input) || config->isMappedLike("lefttrigger", input))
		{
			listInput(-1);
			return true;
		}
		if (config->isMappedLike(vertical ? "down" : "right", input) || config->isMappedLike("righttrigger", input))
		{
			listInput(1);
			return true;
		}
		if (config->isMappedTo("pagedown", input))
		{
			int cursor = moveCursorFast(true);
			listInput(cursor - mCursor);
			return true;
		}
		if (config->isMappedTo("pageup", input))
		{
			int cursor = moveCursorFast(false);
			listInput(cursor - mCursor);
			return true;
		}
	}
	else
	{
		if (config->isMappedLike("left", input) ||
			config->isMappedLike("right", input) ||
			config->isMappedLike("up", input) ||
			config->isMappedLike("down", input) ||
			config->isMappedLike("pagedown", input) ||
			config->isMappedLike("pageup", input) ||
			config->isMappedLike("lefttrigger", input) ||
			config->isMappedLike("righttrigger", input))
			listInput(0);
	}

	return GuiComponent::input(config, input);
}

void CarouselComponent::update(int deltaTime)
{
	for (unsigned int i = 0; i < mEntries.size(); i++)
	{
		const std::shared_ptr<GuiComponent>& comp = mEntries.at(i).data.logo;
		if (comp != nullptr)
			comp->update(deltaTime);
	}

	listUpdate(deltaTime);

	GuiComponent::update(deltaTime);
}

// AmberELEC: the selected logo plays "activate", the previous one "deactivate", the others "scroll"
void CarouselComponent::playStoryboards(int oldCursor)
{
	bool oldCursorHasStoryboard = false;

	if (oldCursor >= 0 && oldCursor < (int)mEntries.size())
	{
		auto logo = mEntries.at(oldCursor).data.logo;
		if (logo)
		{
			if (startLogoStoryboard(logo.get(), "deactivate"))
				oldCursorHasStoryboard = true;
			else
				stopLogoStoryboard(logo.get());
		}
	}

	bool cursorHasStoryboard = false;

	if (mCursor >= 0 && mCursor < (int)mEntries.size())
	{
		auto logo = mEntries.at(mCursor).data.logo;
		if (logo)
		{
			if (startLogoStoryboard(logo.get(), "activate"))
				cursorHasStoryboard = true;
			else
				stopLogoStoryboard(logo.get());
		}
	}

	for (int i = 0; i < (int)mEntries.size(); i++)
	{
		if ((cursorHasStoryboard && i == mCursor) || (oldCursorHasStoryboard && i == oldCursor))
			continue;

		auto logo = mEntries.at(i).data.logo;
		if (logo)
			startLogoStoryboard(logo.get(), "scroll");
	}
}

void CarouselComponent::onCursorChanged(const CursorState& state)
{
	// update help style
	updateHelpPrompts();

	float startPos = mCamOffset;

	float posMax = (float)mEntries.size();
	float target = (float)mCursor;

	// what's the shortest way to get to our target?
	// it's one of these...

	float endPos = target; // directly
	float dist = fabs(endPos - startPos);

	if (fabs(target + posMax - startPos) < dist)
		endPos = target + posMax; // loop around the end (0 -> max)
	if (fabs(target - posMax - startPos) < dist)
		endPos = target - posMax; // loop around the start (max - 1 -> -1)

	cancelAnimation(1);
	cancelAnimation(2);

	std::string transition_style = Settings::getInstance()->getString("TransitionStyle");
	if (transition_style == "auto")
	{
		if (mDefaultTransition == "instant" || mDefaultTransition == "fade" || mDefaultTransition == "slide" || mDefaultTransition == "fade & slide")
			transition_style = mDefaultTransition;
		else
			transition_style = "slide";
	}

	if (mLastCursor == mCursor)
		return;

	if (!mScrollSound.empty())
		Sound::get(mScrollSound)->play();

	int oldCursor = mLastCursor;
	playStoryboards(oldCursor);

	bool move_carousel = Settings::getInstance()->getBool("MoveCarousel");
	if (Settings::getInstance()->getString("PowerSaverMode") == "instant")
		move_carousel = false;

	const int speed = Math::max(1, (int)mTransitionSpeed);
	const int duration = (transition_style == "instant" && !move_carousel) ? 1 : speed;

	Animation* anim = new LambdaAnimation([this, startPos, endPos, posMax, move_carousel](float t)
	{
		t -= 1; // cubic ease out (same curve as the system carousel)
		float f = Math::lerp(startPos, endPos, t*t*t + 1);
		if (f < 0) f += posMax;
		if (f >= posMax) f -= posMax;

		this->mCamOffset = move_carousel ? f : endPos;
	}, duration);

	if (mCursorChangedCallback)
		mCursorChangedCallback(state);

	mLastCursor = mCursor;

	auto curState = state;
	setAnimation(anim, 0, [this, curState]
	{
		if (curState == CURSOR_SCROLLING && mCursorChangedCallback)
			mCursorChangedCallback(curState);
	}, false, 0);
}

void CarouselComponent::render(const Transform4x4f& parentTrans)
{
	if (size() == 0 || !isVisible())
		return;  // nothing to render

	Transform4x4f trans = getTransform() * parentTrans;

	Vector2f clipPos(trans.translation().x(), trans.translation().y());
	if (!Renderer::isVisibleOnScreen(clipPos.x(), clipPos.y(), mSize.x(), mSize.y()))
		return;

	Renderer::pushClipRect(Vector2i((int)clipPos.x(), (int)clipPos.y()), Vector2i((int)mSize.x(), (int)mSize.y()));
	renderCarousel(trans);
	Renderer::popClipRect();

	if (!mWasRendered)
	{
		// logos exist now: start their storyboards
		mWasRendered = true;
		onShow();
	}
}

std::vector<HelpPrompt> CarouselComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;

	if (mType == GameCarouselType::VERTICAL || mType == GameCarouselType::VERTICAL_WHEEL)
		prompts.push_back(HelpPrompt("up/down", _("CHOOSE")));
	else
		prompts.push_back(HelpPrompt("left/right", _("CHOOSE")));

	return prompts;
}

void CarouselComponent::renderCarousel(const Transform4x4f& trans)
{
	Transform4x4f carouselTrans = trans;

	Renderer::setMatrix(carouselTrans);

	if (mColor != 0 || mColorEnd != 0)
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), mColor, mColorEnd, mColorGradientHorizontal);

	// draw logos
	Vector2f logoSpacing(0.0, 0.0); // NB: logoSpacing will include the size of the logo itself as well!
	float xOff = 0.0;
	float yOff = 0.0;

	switch (mType)
	{
		case GameCarouselType::VERTICAL_WHEEL:
			yOff = (mSize.y() - mLogoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);
			if (mLogoAlignment == ALIGN_LEFT)
				xOff = mLogoSize.x() / 10.f;
			else if (mLogoAlignment == ALIGN_RIGHT)
				xOff = mSize.x() - (mLogoSize.x() * 1.1f);
			else
				xOff = (mSize.x() - mLogoSize.x()) / 2.f;
			break;
		case GameCarouselType::VERTICAL:
			logoSpacing[1] = ((mSize.y() - (mLogoSize.y() * mMaxLogoCount)) / (mMaxLogoCount)) + mLogoSize.y();
			yOff = (mSize.y() - mLogoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);

			if (mLogoAlignment == ALIGN_LEFT)
				xOff = mLogoSize.x() / 10.f;
			else if (mLogoAlignment == ALIGN_RIGHT)
				xOff = mSize.x() - (mLogoSize.x() * 1.1f);
			else
				xOff = (mSize.x() - mLogoSize.x()) / 2;
			break;
		case GameCarouselType::HORIZONTAL_WHEEL:
			xOff = (mSize.x() - mLogoSize.x()) / 2 - (mCamOffset * logoSpacing[1]);
			if (mLogoAlignment == ALIGN_TOP)
				yOff = mLogoSize.y() / 10;
			else if (mLogoAlignment == ALIGN_BOTTOM)
				yOff = mSize.y() - (mLogoSize.y() * 1.1f);
			else
				yOff = (mSize.y() - mLogoSize.y()) / 2;
			break;
		case GameCarouselType::HORIZONTAL:
		default:
			logoSpacing[0] = ((mSize.x() - (mLogoSize.x() * mMaxLogoCount)) / (mMaxLogoCount)) + mLogoSize.x();
			xOff = (mSize.x() - mLogoSize.x()) / 2.f - (mCamOffset * logoSpacing[0]);

			if (mLogoAlignment == ALIGN_TOP)
				yOff = mLogoSize.y() / 10.f;
			else if (mLogoAlignment == ALIGN_BOTTOM)
				yOff = mSize.y() - (mLogoSize.y() * 1.1f);
			else
				yOff = (mSize.y() - mLogoSize.y()) / 2.f;
			break;
	}

	if (mLogoPos.x() >= 0)
		xOff = mLogoPos.x() - (mType == GameCarouselType::HORIZONTAL ? (mCamOffset * logoSpacing[0]) : 0);

	if (mLogoPos.y() >= 0)
		yOff = mLogoPos.y() - (mType == GameCarouselType::VERTICAL ? (mCamOffset * logoSpacing[1]) : 0);

	int center = (int)(mCamOffset);
	int logoCount = Math::min(mMaxLogoCount, (int)mEntries.size());

	// Adding texture loading buffers depending on scrolling speed and status
	int bufferIndex = Math::max(0, Math::min(2, getScrollingVelocity() + 1));
	int bufferLeft = carouselBuffersLeft[bufferIndex];
	int bufferRight = carouselBuffersRight[bufferIndex];

	if (logoCount == 1 && mCamOffset == 0)
	{
		bufferLeft = 0;
		bufferRight = 0;
	}

	auto renderLogo = [this, carouselTrans, logoSpacing, xOff, yOff](int i)
	{
		int index = i % (int)mEntries.size();
		if (index < 0)
			index += (int)mEntries.size();

		Transform4x4f logoTrans = carouselTrans;

		if (mType == GameCarouselType::HORIZONTAL && mLogoScale != 1.0f && mScaledSpacing != 0.0f)
		{
			auto logoDiffX = ((logoSpacing[0] * mLogoScale) - logoSpacing[0]) / 2.0f * mScaledSpacing;

			if (index == mCursor)
				logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff, i * logoSpacing[1] + yOff, 0));
			else if (i < mCursor || (mCursor == 0 && index > mMaxLogoCount))
				logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff - logoDiffX, i * logoSpacing[1] + yOff, 0));
			else
				logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff + logoDiffX, i * logoSpacing[1] + yOff, 0));
		}
		else
			logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff, i * logoSpacing[1] + yOff, 0));

		float distance = i - mCamOffset;

		float scale = 1.0f + ((mLogoScale - 1.0f) * (1.0f - fabs(distance)));
		scale = Math::min(mLogoScale, Math::max(1.0f, scale));
		scale /= mLogoScale;

		int opref = (int)(Math::clamp(mMinLogoOpacity, 0.0f, 1.0f) * 255);

		int opacity = (int)Math::round(opref + ((0xFF - opref) * (1.0f - fabs(distance))));
		opacity = Math::max(opref, opacity);

		ensureLogo(mEntries.at(index));

		const std::shared_ptr<GuiComponent>& comp = mEntries.at(index).data.logo;
		if (mType == GameCarouselType::VERTICAL_WHEEL || mType == GameCarouselType::HORIZONTAL_WHEEL)
		{
			comp->setRotationDegrees(mLogoRotation * distance);
			comp->setRotationOrigin(mLogoRotationOrigin);
		}

		if (!mAnyLogoHasOpacityStoryboard)
			comp->setOpacity((unsigned char)opacity);

		if (!mAnyLogoHasScaleStoryboard)
			comp->setScale(scale);

		comp->render(logoTrans);
	};

	// the selected logo is drawn last, over its neighbours
	std::vector<int> activePositions;
	for (int i = center - logoCount / 2 + bufferLeft; i <= center + logoCount / 2 + bufferRight; i++)
	{
		int index = i % (int)mEntries.size();
		if (index < 0)
			index += (int)mEntries.size();

		if (index == mCursor)
			activePositions.push_back(i);
		else
			renderLogo(i);
	}

	for (auto activePos : activePositions)
		renderLogo(activePos);
}

void CarouselComponent::getCarouselFromTheme(const ThemeData::ThemeElement* elem)
{
	Vector2f size = mSize;

	if (elem->has("type"))
	{
		const std::string type = elem->get<std::string>("type");
		if (type == "vertical")
			mType = GameCarouselType::VERTICAL;
		else if (type == "vertical_wheel")
			mType = GameCarouselType::VERTICAL_WHEEL;
		else if (type == "horizontal_wheel")
			mType = GameCarouselType::HORIZONTAL_WHEEL;
		else
			mType = GameCarouselType::HORIZONTAL;
	}
	if (elem->has("logoScale"))
		mLogoScale = elem->get<float>("logoScale");
	if (elem->has("logoSize"))
		mLogoSize = elem->get<Vector2f>("logoSize") * size;
	if (elem->has("logoPos"))
		mLogoPos = elem->get<Vector2f>("logoPos") * size;
	if (elem->has("maxLogoCount"))
		mMaxLogoCount = Math::max(1, (int)Math::round(elem->get<float>("maxLogoCount")));
	if (elem->has("logoRotation"))
		mLogoRotation = elem->get<float>("logoRotation");
	if (elem->has("logoRotationOrigin"))
		mLogoRotationOrigin = elem->get<Vector2f>("logoRotationOrigin");
	if (elem->has("logoAlignment"))
	{
		const std::string align = elem->get<std::string>("logoAlignment");
		if (align == "left")
			mLogoAlignment = ALIGN_LEFT;
		else if (align == "right")
			mLogoAlignment = ALIGN_RIGHT;
		else if (align == "top")
			mLogoAlignment = ALIGN_TOP;
		else if (align == "bottom")
			mLogoAlignment = ALIGN_BOTTOM;
		else
			mLogoAlignment = ALIGN_CENTER;
	}

	if (elem->has("color"))
		mColor = mColorEnd = elem->get<unsigned int>("color");
	if (elem->has("colorEnd"))
		mColorEnd = elem->get<unsigned int>("colorEnd");
	if (elem->has("gradientType"))
		mColorGradientHorizontal = elem->get<std::string>("gradientType").compare("horizontal");

	if (elem->has("scrollSound"))
		mScrollSound = elem->get<std::string>("scrollSound");

	if (elem->has("defaultTransition"))
		mDefaultTransition = elem->get<std::string>("defaultTransition");

	if (elem->has("transitionSpeed"))
		mTransitionSpeed = elem->get<float>("transitionSpeed");

	if (elem->has("minLogoOpacity"))
		mMinLogoOpacity = elem->get<float>("minLogoOpacity");

	if (elem->has("scaledLogoSpacing"))
		mScaledSpacing = elem->get<float>("scaledLogoSpacing");

	if (elem->has("imageSource"))
	{
		auto source = elem->get<std::string>("imageSource");
		if (source == "text")
			mImageSource = CarouselImageSource::TEXT;
		else if (source == "image")
			mImageSource = CarouselImageSource::IMAGE;
		else if (source == "marquee")
			mImageSource = CarouselImageSource::MARQUEE;
		else if (source == "fanart")
			mImageSource = CarouselImageSource::FANART;
		else if (source == "titleshot")
			mImageSource = CarouselImageSource::TITLESHOT;
		else if (source == "boxart")
			mImageSource = CarouselImageSource::BOXART;
		else if (source == "cartridge")
			mImageSource = CarouselImageSource::CARTRIDGE;
		else if (source == "boxback")
			mImageSource = CarouselImageSource::BOXBACK;
		else if (source == "mix")
			mImageSource = CarouselImageSource::MIX;
		else if (source == "wheel")
			mImageSource = CarouselImageSource::WHEEL;
		else
			mImageSource = CarouselImageSource::THUMBNAIL;
	}
}

void CarouselComponent::onShow()
{
	GuiComponent::onShow();

	// not rendered yet: every logo is null, the storyboards start on the first render()
	if (!mWasRendered)
		return;

	bool cursorStoryboardSet = false;

	if (mCursor >= 0 && mCursor < (int)mEntries.size())
	{
		auto logo = mEntries.at(mCursor).data.logo;
		if (logo)
		{
			if (startLogoStoryboard(logo.get(), "activate") || startLogoStoryboard(logo.get(), ""))
				cursorStoryboardSet = true;
			else
				stopLogoStoryboard(logo.get());
		}
	}

	for (int i = 0; i < (int)mEntries.size(); i++)
	{
		if (cursorStoryboardSet && mCursor == i)
			continue;

		auto logo = mEntries.at(i).data.logo;
		if (logo && !startLogoStoryboard(logo.get(), "scroll"))
			startLogoStoryboard(logo.get(), "");
	}
}

bool CarouselComponent::storyboardHasProperty(GuiComponent* comp, const std::string& event, const std::string& propertyName)
{
	ItemTemplateComponent* templ = dynamic_cast<ItemTemplateComponent*>(comp);
	if (templ != nullptr)
		return templ->storyboardExists(event, propertyName);

	return comp->storyboardHasProperty(event, propertyName);
}

// <itemTemplate> logos play the storyboard on all their elements
bool CarouselComponent::startLogoStoryboard(GuiComponent* comp, const std::string& event)
{
	ItemTemplateComponent* templ = dynamic_cast<ItemTemplateComponent*>(comp);
	if (templ != nullptr)
		return templ->playStoryboard(event);

	return comp->startStoryboard(event);
}

void CarouselComponent::stopLogoStoryboard(GuiComponent* comp)
{
	ItemTemplateComponent* templ = dynamic_cast<ItemTemplateComponent*>(comp);
	if (templ != nullptr)
		templ->stopStoryboards();
	else
		comp->stopStoryboard();
}

void CarouselComponent::ensureLogo(IList<CarouselComponentData, FileData*>::Entry& entry)
{
	if (entry.data.logo != nullptr)
		return;

	std::string mediaPath;

	FileData* file = entry.object;

	// AmberELEC <itemTemplate>
	const ThemeData::ThemeElement* carouselElem = mTheme->getElement(mThemeViewName, "gamecarousel", "gamecarousel");
	if (carouselElem != nullptr)
	{
		for (auto& child : carouselElem->children)
		{
			if (child.first != "itemTemplate")
				continue;

			ItemTemplateComponent* templ = new ItemTemplateComponent(entry.name, mWindow);
			templ->setScaleOrigin(Vector2f(0.0f, 0.0f));
			templ->setSize(mLogoSize * mLogoScale);
			templ->loadTemplatedChildren(&child.second);
			entry.data.logo = std::shared_ptr<GuiComponent>(templ);
			break;
		}
	}

	if (entry.data.logo == nullptr && file != nullptr && mImageSource != CarouselImageSource::TEXT)
	{
		switch (mImageSource)
		{
		case CarouselImageSource::IMAGE:     mediaPath = file->getImagePath(); break;
		case CarouselImageSource::MARQUEE:   mediaPath = file->getMarqueePath(); break;
		case CarouselImageSource::FANART:    mediaPath = file->getMetadata().get("fanart"); break;
		case CarouselImageSource::TITLESHOT: mediaPath = file->getMetadata().get("titleshot"); break;
		case CarouselImageSource::BOXART:    mediaPath = file->getMetadata().get("boxart"); break;
		case CarouselImageSource::CARTRIDGE: mediaPath = file->getMetadata().get("cartridge"); break;
		case CarouselImageSource::BOXBACK:   mediaPath = file->getMetadata().get("boxback"); break;
		case CarouselImageSource::MIX:       mediaPath = file->getMetadata().get("mix"); break;
		case CarouselImageSource::WHEEL:     mediaPath = file->getMetadata().get("wheel"); break;
		default:                             mediaPath = file->getThumbnailPath(); break;
		}
	}

	if (entry.data.logo != nullptr)
	{
		// <itemTemplate> logo
	}
	else if (!mediaPath.empty() && Utils::FileSystem::exists(mediaPath))
	{
		ImageComponent* logo = new ImageComponent(mWindow, false, true);
		logo->setMaxSize(mLogoSize * mLogoScale);

		if (mImageSource == CarouselImageSource::IMAGE)
			logo->setIsLinear(true);

		logo->applyTheme(mTheme, mThemeViewName, "gamecarouselLogo", ThemeFlags::COLOR | ThemeFlags::ALIGNMENT | ThemeFlags::VISIBLE);
		logo->setImage(mediaPath, false, MaxSizeInfo(mLogoSize * mLogoScale));

		entry.data.logo = std::shared_ptr<GuiComponent>(logo);
	}
	else // no media: use text
	{
		TextComponent* text = new TextComponent(mWindow, entry.name, Renderer::isSmallScreen() ? Font::get(FONT_SIZE_MEDIUM) : Font::get(FONT_SIZE_LARGE), 0x000000FF, ALIGN_CENTER);
		text->setScaleOrigin(Vector2f(0.0f, 0.0f));
		text->setSize(mLogoSize * mLogoScale);
		text->applyTheme(mTheme, mThemeViewName, "gamecarouselLogoText", ThemeFlags::FONT_PATH | ThemeFlags::FONT_SIZE | ThemeFlags::COLOR | ThemeFlags::FORCE_UPPERCASE | ThemeFlags::LINE_SPACING | ThemeFlags::TEXT);

		if (mType == GameCarouselType::VERTICAL || mType == GameCarouselType::VERTICAL_WHEEL)
		{
			text->setHorizontalAlignment(mLogoAlignment);
			text->setVerticalAlignment(ALIGN_CENTER);
		}
		else
		{
			text->setHorizontalAlignment(ALIGN_CENTER);
			text->setVerticalAlignment(mLogoAlignment);
		}

		entry.data.logo = std::shared_ptr<GuiComponent>(text);
	}

	ItemTemplateComponent* itemTemplate = dynamic_cast<ItemTemplateComponent*>(entry.data.logo.get());
	if (itemTemplate != nullptr)
		itemTemplate->updateTemplateBindings(file);
	else if (file != nullptr)
		BindingManager::updateBindings(entry.data.logo.get(), file);

	if (mType == GameCarouselType::VERTICAL || mType == GameCarouselType::VERTICAL_WHEEL)
	{
		if (mLogoAlignment == ALIGN_LEFT)
			entry.data.logo->setOrigin(0, 0.5);
		else if (mLogoAlignment == ALIGN_RIGHT)
			entry.data.logo->setOrigin(1.0, 0.5);
		else
			entry.data.logo->setOrigin(0.5, 0.5);
	}
	else
	{
		if (mLogoAlignment == ALIGN_TOP)
			entry.data.logo->setOrigin(0.5, 0);
		else if (mLogoAlignment == ALIGN_BOTTOM)
			entry.data.logo->setOrigin(0.5, 1);
		else
			entry.data.logo->setOrigin(0.5, 0.5);
	}

	Vector2f denormalized = mLogoSize * entry.data.logo->getOrigin();
	entry.data.logo->setPosition(denormalized.x(), denormalized.y(), 0.0);

	GuiComponent* logo = entry.data.logo.get();

	for (auto event : { "deactivate", "activate", "scroll", "" })
	{
		mAnyLogoHasScaleStoryboard |= storyboardHasProperty(logo, event, "scale");
		mAnyLogoHasOpacityStoryboard |= storyboardHasProperty(logo, event, "opacity");
	}
}

void CarouselComponent::add(const std::string& name, FileData* obj)
{
	typename IList<CarouselComponentData, FileData*>::Entry entry;
	entry.name = name;
	entry.object = obj;
	entry.data.logo = nullptr;

	static_cast<IList<CarouselComponentData, FileData*>*>(this)->add(entry);
}

void CarouselComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	mTheme = theme;
	mThemeViewName = view;

	GuiComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* carouselElem = theme->getElement(view, element, "gamecarousel");
	if (carouselElem)
		getCarouselFromTheme(carouselElem);

	// logos are rebuilt with the new theme
	for (auto& entry : mEntries)
		entry.data.logo = nullptr;

	mAnyLogoHasScaleStoryboard = false;
	mAnyLogoHasOpacityStoryboard = false;
	mWasRendered = false;
}

std::shared_ptr<GuiComponent> CarouselComponent::getLogo(int index)
{
	if (index >= 0 && index < (int)mEntries.size())
		return mEntries.at(index).data.logo;

	return nullptr;
}
