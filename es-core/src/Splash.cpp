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
	mBackgroundAsBar(false),
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
		loadProgressBarImage(mInactiveProgressbar);
	}

	// a progress bar drawn with the background picture covers it exactly at 1080p:
	// keep the background in step with the bar at other screen sizes
	if (fullScreenBackground && themeImage && mInactiveProgressbar.texture != nullptr && mInactiveProgressbar.path == imagePath)
	{
		mBackgroundBar = mInactiveProgressbar;
		mBackgroundBar.color = 0xFFFFFFFF;
		mBackgroundBar.colorEnd = 0xFFFFFFFF;
		mBackgroundAsBar = true;
	}

	mActiveProgressbar = mInactiveProgressbar;
	mActiveProgressbar.color = 0xDF1010FF;
	mActiveProgressbar.colorEnd = 0x4F0000FF;
	mActiveProgressbar.horizontalGradient = true;

	elem = theme->getElement("splash", "progressbar:active", "image");
	if (elem)
	{
		loadProgressBar(elem, mActiveProgressbar, false);
		loadProgressBarImage(mActiveProgressbar);
	}

	// extras
	mExtras = ThemeData::makeExtras(theme, "splash", window, true);

	// progressbar / progressbar:active belong to the splash itself, even when declared extra="true"
	mExtras.erase(std::remove_if(mExtras.begin(), mExtras.end(), [](GuiComponent* extra)
	{
		if (extra->getTag() != "progressbar" && extra->getTag() != "progressbar:active")
			return false;

		delete extra;
		return true;
	}), mExtras.end());

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

		if (elem->has("minSize"))
		{
			Vector2f size = elem->get<Vector2f>("minSize");
			bar.w = size.x() * sw;
			bar.h = size.y() * sh;
		}

		if (elem->has("maxSize"))
		{
			Vector2f size = elem->get<Vector2f>("maxSize");
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
			bar.originX = origin.x();
			bar.originY = origin.y();
			bar.x -= origin.x() * bar.w;
			bar.y -= origin.y() * bar.h;
		}
	}

	if (elem->has("size"))
		bar.sizeMode = 0;
	else if (elem->has("minSize"))
		bar.sizeMode = 1;
	else if (elem->has("maxSize"))
		bar.sizeMode = 2;

	if (elem->has("tile"))
		bar.tile = elem->get<bool>("tile");

	if (elem->has("path"))
		bar.path = elem->get<std::string>("path");

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

// image progress bars, laid out in the bar's box at any resolution:
// <minSize> covers the box (centred crop), <maxSize> fits in it, <size> stretches,
// <tile> repeats the image at its 1080p scale (the size themes are made for), centred in the box
void Splash::loadProgressBarImage(ProgressBar& bar)
{
	bar.texture = nullptr;

	if (bar.path.empty() || !ResourceManager::getInstance()->fileExists(bar.path))
		return;

	auto texture = TextureResource::get(bar.path, bar.tile, true, true, false, false);
	if (texture == nullptr)
		return;

	const float tw = (float)texture->getSize().x();
	const float th = (float)texture->getSize().y();
	if (tw <= 0 || th <= 0)
		return;

	// one size component set: keep the image aspect ratio
	if (bar.w <= 0 && bar.h > 0)
	{
		bar.w = bar.h * tw / th;
		bar.x -= bar.originX * bar.w;
	}
	else if (bar.h <= 0 && bar.w > 0)
	{
		bar.h = bar.w * th / tw;
		bar.y -= bar.originY * bar.h;
	}

	if (bar.w <= 0 || bar.h <= 0)
		return;

	bar.dx = bar.x; bar.dy = bar.y; bar.dw = bar.w; bar.dh = bar.h;
	bar.u0 = 0; bar.v0 = 0; bar.u1 = 1; bar.v1 = 1;

	if (bar.tile)
	{
		const float scale = (float)Renderer::getScreenHeight() / 1080.0f;
		const float tileW = tw * scale;
		const float tileH = th * scale;

		bar.u0 = (tileW - bar.w) / (2.0f * tileW);
		bar.u1 = bar.u0 + bar.w / tileW;
		bar.v0 = (tileH - bar.h) / (2.0f * tileH);
		bar.v1 = bar.v0 + bar.h / tileH;
	}
	else if (bar.sizeMode == 1)
	{
		const float scale = Math::max(bar.w / tw, bar.h / th);
		const float cropX = (1.0f - bar.w / (tw * scale)) / 2.0f;
		const float cropY = (1.0f - bar.h / (th * scale)) / 2.0f;

		bar.u0 = cropX; bar.u1 = 1.0f - cropX;
		bar.v0 = cropY; bar.v1 = 1.0f - cropY;
	}
	else if (bar.sizeMode == 2)
	{
		const float scale = Math::min(bar.w / tw, bar.h / th);
		bar.dw = tw * scale;
		bar.dh = th * scale;
		bar.dx = bar.x + (bar.w - bar.dw) * bar.originX;
		bar.dy = bar.y + (bar.h - bar.dh) * bar.originY;
	}

	bar.texture = texture;
}

void Splash::renderProgressBarImage(const ProgressBar& bar, unsigned char alpha)
{
	if (bar.texture == nullptr || !bar.texture->bind())
		return;

	auto withAlpha = [alpha](unsigned int color) { return Renderer::convertColor((color & 0xFFFFFF00) | (unsigned char)((color & 0xFF) * alpha / 255)); };

	const unsigned int color = withAlpha(bar.color);
	const unsigned int colorEnd = withAlpha(bar.colorEnd);

	// texture v runs bottom to top
	Renderer::Vertex vertices[4];
	vertices[0] = { { bar.dx,          bar.dy          }, { bar.u0, 1.0f - bar.v0 }, color };
	vertices[1] = { { bar.dx,          bar.dy + bar.dh }, { bar.u0, 1.0f - bar.v1 }, bar.horizontalGradient ? colorEnd : color };
	vertices[2] = { { bar.dx + bar.dw, bar.dy          }, { bar.u1, 1.0f - bar.v0 }, bar.horizontalGradient ? color : colorEnd };
	vertices[3] = { { bar.dx + bar.dw, bar.dy + bar.dh }, { bar.u1, 1.0f - bar.v1 }, colorEnd };

	for (int i = 0; i < 4; ++i)
		vertices[i].pos.round();

	Renderer::drawTriangleStrips(&vertices[0], 4);
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

	if (mBackgroundAsBar)
	{
		Renderer::setMatrix(trans);
		renderProgressBarImage(mBackgroundBar, alpha);
	}
	else
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

		if (in.texture != nullptr)
			renderProgressBarImage(in, alpha);
		else
			Renderer::drawRect(in.x, in.y, in.w, in.h, withAlpha(in.color), withAlpha(in.colorEnd), in.horizontalGradient);

		if (act.texture != nullptr)
		{
			// image bar: revealed from the left, not squeezed
			int revealW = (int)Math::round(act.w * Math::clamp(mPercent, 0.0f, 1.0f));
			int revealH = (int)Math::round(act.h);

			if (revealW > 0 && revealH > 0)
			{
				Renderer::pushClipRect(Vector2i((int)Math::round(act.x), (int)Math::round(act.y)), Vector2i(revealW, revealH));
				renderProgressBarImage(act, alpha);
				Renderer::popClipRect();
			}
		}
		else
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
