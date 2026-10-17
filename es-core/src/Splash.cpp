#include "Splash.h"

#include "BindingManager.h"
#include "Log.h"
#include "Settings.h"
#include "ThemeData.h"
#include "Window.h"
#include "components/NinePatchComponent.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "resources/TextureResource.h"
#include "utils/FileSystemUtil.h"
#include <algorithm>

Splash::Splash(Window* window, const std::string& image, bool fullScreenBackground, IBindable* bindable) :
	mThemed(false),
	mBackground(window, true),
	mText(window),
	mPercent(-1),
	mRoundCorners(0.01f),
	mBackgroundColor(0x000000FF)
{
	mThemeSet = Settings::getInstance()->getString("ThemeSet");

	// <theme>/splash.xml or <theme>/gamesplash.xml
	std::string themeFilePath;

	auto themeSets = ThemeData::getThemeSets();
	auto themeset = themeSets.find(mThemeSet);
	if (themeset != themeSets.cend())
	{
		std::string path = themeset->second.path + (fullScreenBackground ? "/splash.xml" : "/gamesplash.xml");
		if (Utils::FileSystem::exists(path))
			themeFilePath = path;
	}

	if (themeFilePath.empty())
		return;

	auto theme = std::make_shared<ThemeData>();

	try
	{
		std::map<std::string, std::string> sysData;
		theme->loadFile("splash", sysData, themeFilePath);
	}
	catch (ThemeException& e)
	{
		LOG(LogError) << "Splash : " << e.what();
		return;
	}

	mThemed = true;

	const float sw = (float)Renderer::getScreenWidth();
	const float sh = (float)Renderer::getScreenHeight();

	// Background image
	std::string imagePath = image;
	bool themeImage = !fullScreenBackground; // the game splash image is the game's own image

	auto backgroundTheme = theme->getElement("splash", "background", "image");
	if (backgroundTheme && backgroundTheme->has("path"))
	{
		std::string tmp = backgroundTheme->get<std::string>("path");
		if (ResourceManager::getInstance()->fileExists(tmp) || tmp.find("{game:") != std::string::npos || tmp.find("{system:") != std::string::npos || tmp.find("{binding:") != std::string::npos)
		{
			imagePath = tmp;
			themeImage = true;
		}
	}

	bool linearSmooth = backgroundTheme && backgroundTheme->has("linearSmooth") && backgroundTheme->get<bool>("linearSmooth");

	mBackground.setOrigin(0.5, 0.5);

	if (fullScreenBackground)
	{
		mBackground.setPosition(sw / 2.0f, sh / 2.0f);

		if (!themeImage)
		{
			// the default image is the dArkOSen logo, not a full screen picture (Batocera's :/logo.png): keep the logo size
			mBackground.setPosition(sw / 2.0f, sh * 0.4f);
			mBackground.setMaxSize(sw * 0.4f, sh * 0.4f);
			linearSmooth = true;
		}
		else if (backgroundTheme)
			mBackground.setMaxSize(sw, sh);
		else
		{
			mBackground.setMinSize(sw, sh);
			linearSmooth = true;
		}
	}
	else
	{
		mBackground.setPosition(sw * 0.5f, sh * 0.42f);
		mBackground.setMaxSize(sw * 0.71f, sh * 0.55f);
		mBackground.setRoundCorners(0.02f);
	}

	// Label
	auto font = Font::get(FONT_SIZE_MEDIUM);

	mText.setColor(0xFFFFFFFF);
	mText.setHorizontalAlignment(ALIGN_CENTER);
	mText.setFont(font);
	mText.setPosition(0, sh * (fullScreenBackground ? 0.78f : 0.835f));
	mText.setSize(sw, font->getLetterHeight());

	if (backgroundTheme)
	{
		mBackground.applyTheme(theme, "splash", "background", ThemeFlags::ALL ^ (ThemeFlags::PATH));

		// theme position for the default logo, but not the full screen sizing meant for a picture
		if (!themeImage)
			mBackground.setMaxSize(sw * 0.4f, sh * 0.4f);
	}

	if (ResourceManager::getInstance()->fileExists(imagePath))
	{
		mTexture = TextureResource::get(imagePath, false, linearSmooth, true, false, false, mBackground.getMaxSizeInfo());
		mBackground.setImage(mTexture);

		if (!fullScreenBackground)
			ResourceManager::getInstance()->removeReloadable(mTexture);
	}

	if (theme->getElement("splash", "label", "text"))
		mText.applyTheme(theme, "splash", "label", ThemeFlags::ALL ^ (ThemeFlags::TEXT));
	else if (fullScreenBackground)
	{
		mText.setGlowColor(0x00000020);
		mText.setGlowSize(2);
		mText.setGlowOffset(1, 1);
	}

	// <splash name="splash"><backgroundColor>
	auto elem = theme->getElement("splash", "splash", "splash");
	if (elem && elem->has("backgroundColor"))
		mBackgroundColor = elem->get<unsigned int>("backgroundColor");

	// Progress bar
	const float baseHeight = 0.036f;

	mInactiveProgressbar.w = sw / 2.0f;
	mInactiveProgressbar.h = sh * baseHeight;
	mInactiveProgressbar.x = sw / 2.0f - mInactiveProgressbar.w / 2.0f;
	mInactiveProgressbar.y = sh - (sh * 3 * baseHeight);
	mInactiveProgressbar.color = 0x6060607F;
	mInactiveProgressbar.colorEnd = 0x9090907F;
	mInactiveProgressbar.horizontalGradient = true;

	elem = theme->getElement("splash", "progressbar", "image");
	if (elem)
	{
		loadProgressBar(elem, mInactiveProgressbar, true);
		mRoundCorners = elem->has("roundCorners") ? elem->get<float>("roundCorners") : 0.0f;
	}

	mActiveProgressbar = mInactiveProgressbar;
	mActiveProgressbar.color = 0xDF1010FF;
	mActiveProgressbar.colorEnd = 0x4F0000FF;
	mActiveProgressbar.horizontalGradient = true;

	elem = theme->getElement("splash", "progressbar:active", "image");
	if (elem)
		loadProgressBar(elem, mActiveProgressbar, false);

	// extras
	mExtras = ThemeData::makeExtras(theme, "splash", window, true);

	if (bindable != nullptr && !fullScreenBackground)
	{
		BindingManager::updateBindings(&mBackground, bindable);
		BindingManager::updateBindings(&mText, bindable);

		for (auto extra : mExtras)
			BindingManager::updateBindings(extra, bindable);
	}

	// game splash: nothing to reload when coming back from the game
	if (!fullScreenBackground)
	{
		auto removeReloadable = [](GuiComponent* p)
		{
			if (p->isKindOf<TextComponent>())
				ResourceManager::getInstance()->removeReloadable(((TextComponent*)p)->getFont());

			if (p->isKindOf<ImageComponent>())
				ResourceManager::getInstance()->removeReloadable(((ImageComponent*)p)->getTexture());
		};

		removeReloadable(&mText);
		removeReloadable(&mBackground);

		for (auto extra : mExtras)
			removeReloadable(extra);
	}

	std::stable_sort(mExtras.begin(), mExtras.end(), [](GuiComponent* a, GuiComponent* b) { return b->getZIndex() > a->getZIndex(); });
}

Splash::~Splash()
{
	for (auto extra : mExtras)
		delete extra;

	mExtras.clear();
}

// <image name="progressbar"> / <image name="progressbar:active">: geometry (pos, size, origin) and color / colorEnd / gradientType
void Splash::loadProgressBar(const ThemeData::ThemeElement* elem, ProgressBar& bar, bool geometry)
{
	const float sw = (float)Renderer::getScreenWidth();
	const float sh = (float)Renderer::getScreenHeight();

	if (geometry)
	{
		if (elem->has("size"))
		{
			Vector2f size = elem->get<Vector2f>("size");
			bar.w = size.x() * sw;
			bar.h = size.y() * sh;
		}

		if (elem->has("w"))
			bar.w = elem->get<float>("w") * sw;

		if (elem->has("h"))
			bar.h = elem->get<float>("h") * sh;

		if (elem->has("pos"))
		{
			Vector2f pos = elem->get<Vector2f>("pos");
			bar.x = pos.x() * sw;
			bar.y = pos.y() * sh;
		}

		if (elem->has("x"))
			bar.x = elem->get<float>("x") * sw;

		if (elem->has("y"))
			bar.y = elem->get<float>("y") * sh;

		if (elem->has("origin"))
		{
			Vector2f origin = elem->get<Vector2f>("origin");
			bar.x -= origin.x() * bar.w;
			bar.y -= origin.y() * bar.h;
		}
	}

	if (elem->has("color"))
	{
		bar.color = elem->get<unsigned int>("color");
		bar.colorEnd = bar.color;
	}

	if (elem->has("colorEnd"))
		bar.colorEnd = elem->get<unsigned int>("colorEnd");

	// same rule as ImageComponent <gradientType>
	if (elem->has("gradientType"))
		bar.horizontalGradient = elem->get<std::string>("gradientType").compare("horizontal") != 0;
}

void Splash::update(const std::string& text, float percent)
{
	mText.setText(text);
	mPercent = percent;
}

void Splash::render(float opacity, bool swapBuffers)
{
	if (opacity == 0)
		return;

	unsigned char alpha = (unsigned char)(opacity * 255);
	auto withAlpha = [alpha](unsigned int color) { return (color & 0xFFFFFF00) | (unsigned char)((color & 0xFF) * alpha / 255); };

	mText.setOpacity(alpha);

	Transform4x4f trans = Transform4x4f::Identity();
	Renderer::setMatrix(trans);
	Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(), (mBackgroundColor & 0xFFFFFF00) | alpha);

	for (auto extra : mExtras)
		extra->setOpacity(alpha);

	for (auto extra : mExtras)
		if (extra->getZIndex() < 5)
			extra->render(trans);

	mBackground.setOpacity(alpha);
	mBackground.render(trans);

	for (auto extra : mExtras)
		if (extra->getZIndex() >= 5 && extra->getZIndex() < 10)
			extra->render(trans);

	if (mPercent >= 0)
	{
		Renderer::setMatrix(trans);

		const ProgressBar& in = mInactiveProgressbar;
		const ProgressBar& act = mActiveProgressbar;

		float radius = Math::min(Math::max(in.w, in.h) * mRoundCorners, Math::min(in.w, in.h) / 2.0f);
		if (radius > 1)
			Renderer::enableRoundCornerStencil(in.x, in.y, in.w, in.h, radius);

		Renderer::drawRect(in.x, in.y, in.w, in.h, withAlpha(in.color), withAlpha(in.colorEnd), in.horizontalGradient);
		Renderer::drawRect(act.x, act.y, act.w * Math::clamp(mPercent, 0.0f, 1.0f), act.h, withAlpha(act.color), withAlpha(act.colorEnd), act.horizontalGradient);

		if (radius > 1)
			Renderer::disableStencil();
	}

	if (!mText.getText().empty())
	{
		// make sure the font is loaded (coming back from a game)
		auto font = mText.getFont();
		if (font != nullptr)
			font->reload();

		mText.render(trans);
	}

	for (auto extra : mExtras)
		if (extra->getZIndex() >= 10)
			extra->render(trans);

	if (swapBuffers)
		Renderer::swapBuffers();
}
