#pragma once
#ifndef ES_CORE_SPLASH_H
#define ES_CORE_SPLASH_H

#include <string>
#include <memory>
#include <vector>
#include "components/ImageComponent.h"
#include "components/TextComponent.h"

class Window;
class TextureResource;
class IBindable;

// Batocera themed splash screens: <theme>/splash.xml (loading screens) and <theme>/gamesplash.xml (game launch),
// both with a <view name="splash">. isThemed() is false when the current theme has no such file:
// the caller then keeps its own (default) loading screen.
class Splash
{
public:
	Splash(Window* window, const std::string& image, bool fullScreenBackground = true, IBindable* bindable = nullptr);
	~Splash();

	bool isThemed() const { return mThemed; }
	const std::string& getThemeSet() const { return mThemeSet; }

	void update(const std::string& text, float percent = -1);
	void render(float opacity, bool swapBuffers = true);

private:
	struct ProgressBar
	{
		float x, y, w, h;
		unsigned int color, colorEnd;
		bool horizontalGradient;
	};

	void loadProgressBar(const ThemeData::ThemeElement* elem, ProgressBar& bar, bool geometry);

	bool            mThemed;
	std::string     mThemeSet;

	ImageComponent  mBackground;
	TextComponent   mText;
	float           mPercent;

	ProgressBar     mInactiveProgressbar;
	ProgressBar     mActiveProgressbar;
	float           mRoundCorners;

	unsigned int    mBackgroundColor;

	std::shared_ptr<TextureResource> mTexture;

	std::vector<GuiComponent*> mExtras;
};

#endif // ES_CORE_SPLASH_H
