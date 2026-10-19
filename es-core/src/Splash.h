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

		// image bar (<path>): drawn in its box at any resolution
		std::string path;
		bool  tile = false;
		int   sizeMode = 0; // 0 <size> (stretch), 1 <minSize> (cover), 2 <maxSize> (fit)
		float originX = 0, originY = 0;
		std::shared_ptr<TextureResource> texture;
		float dx = 0, dy = 0, dw = 0, dh = 0; // drawn quad
		float u0 = 0, v0 = 0, u1 = 1, v1 = 1; // texture area (v: 0 = top)
	};

	void loadProgressBar(const ThemeData::ThemeElement* elem, ProgressBar& bar, bool geometry);
	void loadProgressBarImage(ProgressBar& bar);
	void renderProgressBarImage(const ProgressBar& bar, unsigned char alpha);

	bool            mThemed;
	std::string     mThemeSet;

	ImageComponent  mBackground;
	TextComponent   mText;
	float           mPercent;

	ProgressBar     mInactiveProgressbar;
	ProgressBar     mActiveProgressbar;

	// the background picture, laid out like the progress bar drawn with the same picture
	ProgressBar     mBackgroundBar;
	bool            mBackgroundAsBar;
	float           mRoundCorners;

	unsigned int    mBackgroundColor;

	std::shared_ptr<TextureResource> mTexture;

	std::vector<GuiComponent*> mExtras;
};

#endif // ES_CORE_SPLASH_H
