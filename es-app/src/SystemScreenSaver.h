#include <string>
#pragma once
#ifndef ES_APP_SYSTEM_SCREEN_SAVER_H
#define ES_APP_SYSTEM_SCREEN_SAVER_H

#include "Window.h"
#include "GuiComponent.h"
#include "renderers/Renderer.h"
#include <random>
#include <map>

using namespace std;

class ImageComponent;
class Sound;
class VideoComponent;
class TextComponent;

class GameScreenSaverBase : public GuiComponent
{
public:
	GameScreenSaverBase(Window* window);
	~GameScreenSaverBase();

	virtual void setGame(FileData* mCurrentGame);

	void render(const Transform4x4f& transform) override;
	void update(int deltaTime) override;

	void setOpacity(unsigned char opacity) override;

protected:
	ImageComponent*		mMarquee;
	TextComponent*		mLabelGame;
	TextComponent*		mLabelSystem;
	TextComponent*		mLabelDate;
	TextComponent*		mLabelTime;

	ImageComponent*		mDecoration;

	Renderer::Rect		mViewport;

	int 				mDateTimeUpdateAccumulator;
	time_t				mDateTimeLastUpdate;
};

class ImageScreenSaver : public GameScreenSaverBase
{
public:
	ImageScreenSaver(Window* window);
	~ImageScreenSaver();

	void setImage(const std::string path);
	bool hasImage();

	void render(const Transform4x4f& transform) override;	

private:
	ImageComponent*		mImage;	
};

class VideoScreenSaver : public GameScreenSaverBase
{
public:
	VideoScreenSaver(Window* window);
	~VideoScreenSaver();

	void setVideo(const std::string path);
	void render(const Transform4x4f& transform) override;
	void update(int deltaTime) override;

private:
	VideoComponent*		mVideo;

	int mTime;
	float mFade;
};

class DateTimeScreenSaver : public GameScreenSaverBase
{
public:
	DateTimeScreenSaver(Window* window);
	~DateTimeScreenSaver();

	void setImage(const std::string path);
	bool hasImage();

	void render(const Transform4x4f& transform) override;
	void update(int deltaTime) override;

	static std::map<std::string, std::string> getBackgrounds();

private:
	ImageComponent* createClockLayer(const std::string& file, bool rotates);

	TextComponent*	mDate;
	TextComponent*	mTime;
	ImageComponent*	mClockFace;
	ImageComponent*	mHourHand;
	ImageComponent*	mMinHand;
	ImageComponent*	mSecHand;
	ImageComponent*	mBackground;

	std::string		mClockType;
	std::string		mClockStyle;
	std::string		mBgMode;
	std::string		mBgPath;
	bool			mShowDate;
	bool			mShowSeconds;
	unsigned int	mBgColor;
	unsigned int	mClockColor;

	std::string timeFormat() const;

	// Flip clock (style "flip1")
	void initFlip();
	void updateFlip(const struct tm* t, int deltaTime);
	static const int FLIP_MAX_SLOTS = 12;	// 0-3 clock HHMM, 4-11 date YYYYMMDD
	void getFlipGlyphs(const struct tm* t, std::string glyphs[FLIP_MAX_SLOTS]) const;
	ImageComponent* createFlipLayer(float x, float y, float w, float h);
	bool isFlipStyle() const { return mClockStyle == "flip1" || mClockStyle == "flip2"; }

	std::string		mFlipDir;

	ImageComponent*	mFlipTop[FLIP_MAX_SLOTS] = {};
	ImageComponent*	mFlipBot[FLIP_MAX_SLOTS] = {};
	ImageComponent*	mFlipAmPm = nullptr;
	ImageComponent*	mFlipDateBg = nullptr;
	std::string		mFlipTopGlyph[FLIP_MAX_SLOTS];
	std::string		mFlipBotGlyph[FLIP_MAX_SLOTS];
	std::string		mFlipAmPmGlyph;
	int				mFlipDelay[FLIP_MAX_SLOTS] = {};
	int				mFlipSlots = 4;
	bool			mFlip12h = false;
	float			mFlipScale = 1.0f;
	float			mFlipX = 0.0f;
	float			mFlipY = 0.0f;
};

// Screensaver implementation for main window
class SystemScreenSaver : public Window::ScreenSaver
{
public:
	SystemScreenSaver(Window* window);
	virtual ~SystemScreenSaver();

	virtual void startScreenSaver();
	virtual void stopScreenSaver();
	virtual void nextVideo();
	virtual void renderScreenSaver();
	virtual bool allowSleep();
	virtual void update(int deltaTime);
	virtual bool isScreenSaverActive();

	virtual FileData* getCurrentGame();
	virtual void launchGame();
	inline virtual void resetCounts() { mVideosCounted = false; mImagesCounted = false; };

private:
	unsigned long countGameListNodes(const char *nodeName);
	void countVideos();
	void countImages();
    int curBrightnessLevel();

	std::string pickGameListNode(unsigned long index, const char *nodeName);
	std::string pickRandomVideo();
	std::string pickRandomGameListImage();
	std::string pickRandomCustomImage();

	enum STATE {
		STATE_INACTIVE,
		STATE_FADE_OUT_WINDOW,
		STATE_FADE_IN_VIDEO,
		STATE_SCREENSAVER_ACTIVE
	};

private:
	bool			mVideosCounted;
	unsigned long		mVideoCount;	
	bool			mImagesCounted;
	unsigned long		mImageCount;

	//VideoComponent*		mVideoScreensaver;
	std::shared_ptr<VideoScreenSaver>		mVideoScreensaver;

	std::shared_ptr<ImageScreenSaver>		mFadingImageScreensaver;
	std::shared_ptr<ImageScreenSaver>		mImageScreensaver;
	std::shared_ptr<DateTimeScreenSaver>	mDateTimeScreensaver;
	
	Window*			mWindow;
	STATE			mState;
	float			mOpacity;
	int				mTimer;
	FileData*		mCurrentGame;
	std::string		mGameName;
	std::string		mSystemName;
	int 			mVideoChangeTime;
	int             sysbrighttmp;

	//std::shared_ptr<Sound>	mBackgroundAudio;
	bool			mLoadingNext;

	random_device 	mRandomDevice;
	default_random_engine* mDefaultRandomEngine;

};

#endif // ES_APP_SYSTEM_SCREEN_SAVER_H
