#include <string>
#include "SystemScreenSaver.h"

#ifdef _RPI_
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"
#include "utils/FileSystemUtil.h"
#include "views/gamelist/IGameListView.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "FileFilterIndex.h"
#include "Log.h"
#include "PowerSaver.h"
#include "Sound.h"
#include "SystemData.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include <unordered_map>
#include <time.h>
#include "ApiSystem.h"
#include "DisplayPanelControl.h"
#include "AudioManager.h"
#include "math/Vector2i.h"
#include "SystemConf.h"
#include "resources/Font.h"
#include "resources/ResourceManager.h"
#include "utils/TimeUtil.h"
#include "VolumeControl.h"
#include "platform.h"
#include "CorridorsConfig.h"

#define FADE_TIME					(500)
#define DATE_TIME_UPDATE_INTERVAL	(100)
#define AQUARIUM_DIR				"/usr/bin/emulationstation/resources/aquarium"
#define AQUARIUM_INI				"/home/ark/.config/aquarium.ini"
#define CORRIDORS_BIN				"/usr/bin/emulationstation/resources/corridors/3D_Corridors"

SystemScreenSaver::SystemScreenSaver(Window* window) :
	mVideoScreensaver(NULL),
	mImageScreensaver(NULL),
	mWindow(window),
	mVideosCounted(false),
	mVideoCount(0),
	mImagesCounted(false),
	mImageCount(0),
	mState(STATE_INACTIVE),
	mOpacity(0.0f),
	mTimer(0),
	mSystemName(""),
	mGameName(""),
	mCurrentGame(NULL),
	mLoadingNext(false)
{

	mWindow->setScreenSaver(this);
	std::string path = getTitleFolder();
	if(!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);
	srand((unsigned int)time(NULL));
	mVideoChangeTime = 30000;

	mDefaultRandomEngine = new default_random_engine(mRandomDevice());

}

SystemScreenSaver::~SystemScreenSaver()
{
	// Delete subtitle file, if existing
	remove(getTitlePath().c_str());
	mCurrentGame = NULL;

	if( mDefaultRandomEngine != nullptr)
	{
		delete mDefaultRandomEngine;
		mDefaultRandomEngine = nullptr;
	}

}

bool SystemScreenSaver::allowSleep()
{
	return (mVideoScreensaver == nullptr && mImageScreensaver == nullptr && mDateTimeScreensaver == nullptr && mExternalCmd.empty());
}

bool SystemScreenSaver::isScreenSaverActive()
{
	return (mState != STATE_INACTIVE);
}

void SystemScreenSaver::startScreenSaver()
{
	bool loadingNext = mLoadingNext;

	stopScreenSaver();

	std::string screensaver_behavior = Settings::getInstance()->getString("ScreenSaverBehavior");
	if (screensaver_behavior == "random video")
	{
		if (!loadingNext && Settings::getInstance()->getBool("VideoAudio"))
			//AudioManager::getInstance()->deinit();

		mVideoChangeTime = Settings::getInstance()->getInt("ScreenSaverSwapVideoTimeout");

		// Configure to fade out the windows, Skip Fading if Instant mode
		mState =  PowerSaver::getMode() == PowerSaver::INSTANT
					? STATE_SCREENSAVER_ACTIVE
					: STATE_FADE_OUT_WINDOW;
	
		if (mState == STATE_FADE_OUT_WINDOW)
		{
			mState = STATE_FADE_IN_VIDEO;
			mOpacity = 1.0f;
		}
		else
			mOpacity = 0.0f;
			
		// Load a random video
		std::string path = pickRandomVideo();

		int retry = 200;
		while (retry > 0 && !Utils::FileSystem::exists(path))
		{
			retry--;
			path = pickRandomVideo();
		}

		if (!path.empty() && Utils::FileSystem::exists(path))
		{
			LOG(LogDebug) << "VideoScreenSaver::startScreenSaver " << path.c_str();

			mVideoScreensaver = std::make_shared<VideoScreenSaver>(mWindow);
			mVideoScreensaver->setGame(mCurrentGame);
			mVideoScreensaver->setVideo(path);

			PowerSaver::runningScreenSaver(true);
			mTimer = 0;
			return;
		}
	}
	else if (screensaver_behavior == "slideshow")
	{
		mVideoChangeTime = Settings::getInstance()->getInt("ScreenSaverSwapImageTimeout");

		// Configure to fade out the windows, Skip Fading if Instant mode
		mState = PowerSaver::getMode() == PowerSaver::INSTANT
			? STATE_SCREENSAVER_ACTIVE
			: STATE_FADE_OUT_WINDOW;

		if (mState == STATE_FADE_OUT_WINDOW)
		{
			mState = STATE_FADE_IN_VIDEO;
			mOpacity = 1.0f;
		}
		else
			mOpacity = 0.0f;

		// Load a random image
		std::string path;
		if (Settings::getInstance()->getBool("SlideshowScreenSaverCustomImageSource"))
		{
			path = pickRandomCustomImage();
			// Custom images are not tied to the game list
			mCurrentGame = NULL;
		}
		else
			path = pickRandomGameListImage();

		if (!path.empty() && Utils::FileSystem::exists(path))
		{
			LOG(LogDebug) << "ImageScreenSaver::startScreenSaver " << path.c_str();

			mImageScreensaver = std::make_shared<ImageScreenSaver>(mWindow);
			mImageScreensaver->setGame(mCurrentGame);
			mImageScreensaver->setImage(path);

			PowerSaver::runningScreenSaver(true);
			mTimer = 0;
			return;
		}	
	}
	else if ( screensaver_behavior == "black" )
	{
        auto sysbright = ApiSystem::getInstance()->getBrightnessLevel();
        if ( sysbright > 0 )
		{
		  DisplayPanelControl::getInstance()->setBrightnessLevel(0);
		}
		PowerSaver::runningScreenSaver(true);
		mTimer = 0;
		return;
	}

	else if (screensaver_behavior == "aquarium" || screensaver_behavior == "corridors")
	{
		// separate program: started from update(), not from inside render()
		if (screensaver_behavior == "aquarium")
		{
			mExternalName = "Aquarium";
			mExternalBin  = AQUARIUM_DIR "/aquarium";
			mExternalCmd  = AQUARIUM_DIR "/aquarium --screensaver --data " AQUARIUM_DIR " --config " AQUARIUM_INI;
		}
		else
		{
			mExternalName = "Corridors";
			mExternalBin  = CORRIDORS_BIN;
			mExternalCmd  = std::string(CORRIDORS_BIN) + " " + CorridorsConfig::load().toArgs();
		}
		mState = STATE_SCREENSAVER_ACTIVE;
		mOpacity = 0.0f;
		PowerSaver::runningScreenSaver(true);
		mTimer = 0;
		return;
	}

	else if (screensaver_behavior == "datetime")
	{
		mDateTimeScreensaver = std::make_shared<DateTimeScreenSaver>(mWindow);
		mState = STATE_SCREENSAVER_ACTIVE;
		mOpacity = 0.0f;
		PowerSaver::runningScreenSaver(true);
		mTimer = 0;
		return;
	}

	// No videos. Just use a standard screensaver
	mState = STATE_SCREENSAVER_ACTIVE;
	mCurrentGame = NULL;
}

int SystemScreenSaver::curBrightnessLevel()
{

    sysbrighttmp = ApiSystem::getInstance()->getBrightnessLevel();

    return sysbrighttmp;
}

void SystemScreenSaver::stopScreenSaver()
{
	bool isExitingScreenSaver = !mLoadingNext;

    std::string screensaver_behavior = Settings::getInstance()->getString("ScreenSaverBehavior");
	if ( screensaver_behavior == "black" )
	{
        auto sysbright = ApiSystem::getInstance()->getBrightnessLevel();
        if ( sysbright > 0 )
		{
			curBrightnessLevel();
		}
		DisplayPanelControl::getInstance()->setBrightnessLevel(sysbrighttmp);
	}

	if (mLoadingNext)
		mFadingImageScreensaver = mImageScreensaver;
	else
		mFadingImageScreensaver = nullptr;

	// so that we stop the background audio next time, unless we're restarting the screensaver
	mLoadingNext = false;

	mVideoScreensaver = nullptr;
	mImageScreensaver = nullptr;
	mDateTimeScreensaver = nullptr;
	mExternalCmd.clear();

	// we need this to loop through different videos
	mState = STATE_INACTIVE;
	PowerSaver::runningScreenSaver(false);

	// Exiting video screen saver -> Restore sound
	if (isExitingScreenSaver && Settings::getInstance()->getBool("VideoAudio") && mVideoScreensaver)
	{
		AudioManager::getInstance()->init();
		AudioManager::getInstance()->playRandomMusic();
	}
}

void SystemScreenSaver::renderScreenSaver()
{
	Transform4x4f transform = Transform4x4f::Identity();
	
	if (mVideoScreensaver)
	{
		// Render black background
		Renderer::setMatrix(Transform4x4f::Identity());
		Renderer::drawRect(0.0f, 0.0f, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0x000000FF, 0x000000FF);

		// Only render the video if the state requires it
		if ((int)mState >= STATE_FADE_IN_VIDEO)
		{
			unsigned int opacity = 255 - (unsigned char)(mOpacity * 255);

			mVideoScreensaver->setOpacity(opacity);
			mVideoScreensaver->render(transform);
		}
	}
	else if (mImageScreensaver)
	{
		// Render black background
		Renderer::setMatrix(transform);
		Renderer::drawRect(0.0f, 0.0f, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0x000000FF);

		if (mFadingImageScreensaver != nullptr)		
			mFadingImageScreensaver->render(transform);

		// Only render the video if the state requires it
		if ((int)mState >= STATE_FADE_IN_VIDEO)
		{			
			if (mImageScreensaver->hasImage())
			{
				unsigned int opacity = 255 - (unsigned char)(mOpacity * 255);
													
				Renderer::setMatrix(transform);
				Renderer::drawRect(0.0f, 0.0f, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0x00000000 | opacity);
				mImageScreensaver->setOpacity(opacity);
				mImageScreensaver->render(transform);
			}
		}
	}
	else if (mDateTimeScreensaver)
	{
		Renderer::setMatrix(transform);
		mDateTimeScreensaver->render(transform);
	}
	else if (mState != STATE_INACTIVE)
	{
		std::string screensaver_behavior = Settings::getInstance()->getString("ScreenSaverBehavior");

		Renderer::setMatrix(Transform4x4f::Identity());
		unsigned char color = screensaver_behavior == "dim" ? 0x000000A0 : 0x000000FF;
		Renderer::drawRect(0.0f, 0.0f, Renderer::getScreenWidth(), Renderer::getScreenHeight(), color, color);
	}
}

unsigned long SystemScreenSaver::countGameListNodes(const char *nodeName)
{
	unsigned long nodeCount = 0;
	std::vector<SystemData*>::const_iterator it;
	for (it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); ++it)
	{
		// We only want nodes from game systems that are not collections
		if (!(*it)->isGameSystem() || (*it)->isCollection())
			continue;

		FolderData* rootFileData = (*it)->getRootFolder();

		std::vector<FileData*> allFiles = rootFileData->getFilesRecursive(GAME, true);
		std::vector<FileData*>::const_iterator itf;  // declare an iterator to a vector of strings

		for (itf=allFiles.cbegin() ; itf < allFiles.cend(); itf++) 
		{
			if ((strcmp(nodeName, "video") == 0 && (*itf)->getVideoPath() != "") ||
				(strcmp(nodeName, "image") == 0 && (*itf)->getImagePath() != ""))
			{
				nodeCount++;
			}
		}
	}
	return nodeCount;
}

void SystemScreenSaver::countVideos()
{
	if (!mVideosCounted)
	{
		mVideoCount = countGameListNodes("video");
		mVideosCounted = true;
	}
}

void SystemScreenSaver::countImages()
{
	if (!mImagesCounted)
	{
		mImageCount = countGameListNodes("image");
		mImagesCounted = true;
	}
}

std::string SystemScreenSaver::pickGameListNode(unsigned long index, const char *nodeName)
{
	std::string path;

	std::vector<SystemData*>::const_iterator it;
	for (it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); ++it)
	{
		// We only want nodes from game systems that are not collections
		if (!(*it)->isGameSystem() || (*it)->isCollection())
			continue;

		FolderData* rootFileData = (*it)->getRootFolder();

		FileType type = GAME;
		std::vector<FileData*> allFiles = rootFileData->getFilesRecursive(type, true);
		std::vector<FileData*>::const_iterator itf;  // declare an iterator to a vector of strings

		for(itf=allFiles.cbegin() ; itf < allFiles.cend(); itf++) 
		{
			if ((strcmp(nodeName, "video") == 0 && (*itf)->getVideoPath() != "") ||
				(strcmp(nodeName, "image") == 0 && (*itf)->getImagePath() != ""))
			{
				if (index-- <= 0)
				{
					// We have it
					if (strcmp(nodeName, "video") == 0)
						path = (*itf)->getVideoPath();
					else if (strcmp(nodeName, "image") == 0)
						path = (*itf)->getImagePath();

					if (!Utils::FileSystem::exists(path))
						continue;

					mSystemName = (*it)->getFullName();
					mGameName = (*itf)->getName();
					mCurrentGame = (*itf);

#ifdef _RPI_
					if (Settings::getInstance()->getBool("ScreenSaverOmxPlayer"))
						if (Settings::getInstance()->getString("ScreenSaverGameInfo") != "never" && strcmp(nodeName, "video") == 0)
							writeSubtitle(mGameName.c_str(), mSystemName.c_str(), (Settings::getInstance()->getString("ScreenSaverGameInfo") == "always"));
#endif

					return path;
				}
			}
		}
	}

	return "";
}

std::string SystemScreenSaver::pickRandomVideo()
{
	countVideos();
	mCurrentGame = NULL;
	if (mVideoCount == 0)
		return "";
	
	//int video = (int)(((float)rand() / float(RAND_MAX)) * (float)mVideoCount);

	uniform_int_distribution<int> uniform_dist(0, mVideoCount -1);
	int video = uniform_dist(*mDefaultRandomEngine);

	return pickGameListNode(video, "video");
}

std::string SystemScreenSaver::pickRandomGameListImage()
{
	countImages();
	mCurrentGame = NULL;
	if (mImageCount == 0)
		return "";
	
	//int image = (int)(((float)rand() / float(RAND_MAX)) * (float)mImageCount);
	uniform_int_distribution<int> uniform_dist(0, mImageCount -1);
	int image = uniform_dist(*mDefaultRandomEngine);
	
	return pickGameListNode(image, "image");
}

std::string SystemScreenSaver::pickRandomCustomImage()
{
	std::string path;

	std::string imageDir = Settings::getInstance()->getString("SlideshowScreenSaverImageDir");
	if ((imageDir != "") && (Utils::FileSystem::exists(imageDir)))
	{
		std::string                   imageFilter = Settings::getInstance()->getString("SlideshowScreenSaverImageFilter");
		std::vector<std::string>      matchingFiles;
		Utils::FileSystem::stringList dirContent  = Utils::FileSystem::getDirContent(imageDir, Settings::getInstance()->getBool("SlideshowScreenSaverRecurse"));

		for(Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
		{
			if (Utils::FileSystem::isRegularFile(*it))
			{
				// If the image filter is empty, or the file extension is in the filter string,
				//  add it to the matching files list
				if ((imageFilter.length() <= 0) ||
					(imageFilter.find(Utils::FileSystem::getExtension(*it)) != std::string::npos))
				{
					matchingFiles.push_back(*it);
				}
			}
		}

		int fileCount = (int)matchingFiles.size();
		if (fileCount > 0)
		{
			// get a random index in the range 0 to fileCount (exclusive)
			int randomIndex = rand() % fileCount;
			path = matchingFiles[randomIndex];
		}
		else
		{
			LOG(LogError) << "Slideshow Screensaver - No image files found\n";
		}
	}
	else
	{
		LOG(LogError) << "Slideshow Screensaver - Image directory does not exist: " << imageDir << "\n";
	}

	return path;
}

void SystemScreenSaver::update(int deltaTime)
{
	if (!mExternalCmd.empty())
	{
		// copy first: stopScreenSaver() clears the members
		std::string bin  = mExternalBin;
		std::string cmd  = mExternalCmd;
		std::string name = mExternalName;
		mExternalCmd.clear();
		runExternalScreensaver(bin, cmd, name);
		return;
	}

	// Date & time screensaver has no fade/swap cycle
	if (mDateTimeScreensaver)
	{
		mDateTimeScreensaver->update(deltaTime);
		return;
	}
	
	// Use this to update the fade value for the current fade stage
	if (mState == STATE_FADE_OUT_WINDOW)
	{
		mOpacity += (float)deltaTime / FADE_TIME;
		if (mOpacity >= 1.0f)
		{
			mOpacity = 1.0f;

			// Update to the next state
			mState = STATE_FADE_IN_VIDEO;			
		}
	}
	else if (mState == STATE_FADE_IN_VIDEO)
	{
		mOpacity -= (float)deltaTime / FADE_TIME;
		if (mOpacity <= 0.0f)
		{
			mOpacity = 0.0f;
			// Update to the next state
			mState = STATE_SCREENSAVER_ACTIVE;
			mFadingImageScreensaver = nullptr;
		}
	}
	else if (mState == STATE_SCREENSAVER_ACTIVE)
	{
		// Update the timer that swaps the videos
		mTimer += deltaTime;
		if (mTimer > mVideoChangeTime)
			nextVideo();
	}

	// If we have a loaded video then update it
	if (mVideoScreensaver)
		mVideoScreensaver->update(deltaTime);

	if (mImageScreensaver)
		mImageScreensaver->update(deltaTime);
}

// Runs an external screensaver like a game launch: ES releases audio and the
// display, waits for the program to exit (any button), then comes back to the menu.
void SystemScreenSaver::runExternalScreensaver(const std::string& bin, const std::string& cmd, const std::string& name)
{
	if (Utils::FileSystem::exists(bin))
	{
		AudioManager::getInstance()->deinit();
		VolumeControl::getInstance()->deinit();

		bool hideWindow = Settings::getInstance()->getBool("HideWindow");
		mWindow->deinit(hideWindow);

		int exitCode = runSystemCommand(cmd, "", hideWindow ? NULL : mWindow);
		if (exitCode != 0)
			LOG(LogWarning) << name << " screensaver exited with code " << exitCode;

		mWindow->init(hideWindow);
		VolumeControl::getInstance()->init();
		AudioManager::getInstance()->init();
		mWindow->normalizeNextUpdate();

		if (Settings::getInstance()->getBool("audio.bgmusic"))
			AudioManager::getInstance()->playRandomMusic();
	}
	else
		LOG(LogError) << name << " screensaver: " << bin << " not found";

	// end the screensaver and restart the idle countdown
	mWindow->cancelScreenSaver();
	mWindow->resetIdleTimer();
}

void SystemScreenSaver::nextVideo() 
{
	mLoadingNext = true;
	startScreenSaver();
}

FileData* SystemScreenSaver::getCurrentGame()
{
	return mCurrentGame;
}

void SystemScreenSaver::launchGame()
{
	if (mCurrentGame != NULL)
	{
		// launching Game
		auto view = ViewController::get()->getGameListView(mCurrentGame->getSystem(), false);
		if (view != nullptr)
			view->setCursor(mCurrentGame);

		if (Settings::getInstance()->getBool("ScreenSaverControls"))
			mCurrentGame->launchGame(mWindow);
		else
			ViewController::get()->goToGameList(mCurrentGame->getSystem());
	}
}


// ------------------------------------------------------------------------------------------------------------------------
// GAME SCREEN SAVER BASE CLASS
// ------------------------------------------------------------------------------------------------------------------------

GameScreenSaverBase::GameScreenSaverBase(Window* window) : GuiComponent(window),
	mViewport(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight())
{
	mDecoration = nullptr;
	mMarquee = nullptr;
	mLabelGame = nullptr;
	mLabelSystem = nullptr;
	mLabelDate = nullptr;
	mLabelTime = nullptr;
	mDateTimeUpdateAccumulator = 0;
	mDateTimeLastUpdate = 0;

	if (Settings::getInstance()->getBool("ScreenSaverDateTime"))
	{
		auto ph = ThemeData::getMenuTheme()->Text.font->getPath();
		auto sz = mViewport.h / 16.f;
		auto margin = sz / 2.f;
		auto font = Font::get(sz, ph);
		int fh = font->getLetterHeight();

		mLabelDate = new TextComponent(mWindow);
		mLabelDate->setPosition(mViewport.x + margin, mViewport.y + margin);
		mLabelDate->setSize(mViewport.w, sz * 0.66);
		mLabelDate->setHorizontalAlignment(ALIGN_LEFT);
		mLabelDate->setVerticalAlignment(ALIGN_CENTER);
		mLabelDate->setColor(0xD0D0D0FF);
		mLabelDate->setGlowColor(0x00000060);
		mLabelDate->setGlowSize(2);
		mLabelDate->setFont(ph, sz * 0.66);

		mLabelTime = new TextComponent(mWindow);
		mLabelTime->setPosition(mViewport.x + margin, mViewport.y + margin + mLabelDate->getSize().y() * 1.3f);
		mLabelTime->setSize(mViewport.w, fh);
		mLabelTime->setHorizontalAlignment(ALIGN_LEFT);
		mLabelTime->setVerticalAlignment(ALIGN_CENTER);
		mLabelTime->setColor(0xFFFFFFFF);
		mLabelTime->setGlowColor(0x00000040);
		mLabelTime->setGlowSize(3);
		mLabelTime->setFont(font);
	}
}

GameScreenSaverBase::~GameScreenSaverBase()
{
	if (mMarquee != nullptr)
	{
		delete mMarquee;
		mMarquee = nullptr;
	}

	if (mDecoration != nullptr)
	{
		delete mDecoration;
		mDecoration = nullptr;
	}

	if (mLabelGame != nullptr)
	{
		delete mLabelGame;
		mLabelGame = nullptr;
	}

	if (mLabelSystem != nullptr)
	{
		delete mLabelSystem;
		mLabelSystem = nullptr;
	}

	if (mLabelDate != nullptr)
	{
		delete mLabelDate;
		mLabelDate = nullptr;
	}

	if (mLabelTime != nullptr)
	{
		delete mLabelTime;
		mLabelTime = nullptr;
	}
}

#include "guis/GuiMenu.h"
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>

void GameScreenSaverBase::setGame(FileData* game)
{	
	if (mLabelGame != nullptr)
	{
		delete mLabelGame;
		mLabelGame = nullptr;
	}

	if (mLabelSystem != nullptr)
	{
		delete mLabelSystem;
		mLabelSystem = nullptr;
	}

	if (mMarquee != nullptr)
	{
		delete mMarquee;
		mMarquee = nullptr;
	}

	if (mDecoration != nullptr)
	{
		delete mDecoration;
		mDecoration = nullptr;
	}

	if (game == nullptr)
		return;

	mViewport = Renderer::Rect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight());

	/*
#ifdef _RPI_
	if (!Settings::getInstance()->getBool("ScreenSaverOmxPlayer"))
#endif
	if (Settings::getInstance()->getBool("ScreenSaverDecoration"))
	{
		auto sets = GuiMenu::getDecorationsSets(game->getSystem());
		int setId = (int)(((float)rand() / float(RAND_MAX)) * (float)sets.size());

		if (setId >= 0 && setId < sets.size() && Utils::FileSystem::exists(sets[setId].imageUrl))
		{
			std::string infoFile = Utils::String::replace(sets[setId].imageUrl, ".png", ".info");
			if (Utils::FileSystem::exists(infoFile))
			{
				FILE* fp = fopen(infoFile.c_str(), "r"); // non-Windows use "r"
				if (fp)
				{
					char readBuffer[65536];
					rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
					rapidjson::Document doc;
					doc.ParseStream(is);

					if (!doc.HasParseError())
					{
						if (doc.HasMember("top") && doc.HasMember("left") && doc.HasMember("bottom") && doc.HasMember("right") && doc.HasMember("width") && doc.HasMember("height"))
						{
							auto width = doc["width"].GetInt();
							auto height = doc["height"].GetInt();
							if (width > 0 && height > 0)
							{
								float px = Renderer::getScreenWidth() / (float)width;
								float py = Renderer::getScreenHeight() / (float)height;

								auto top = doc["top"].GetInt();
								auto left = doc["left"].GetInt();
								auto bottom = doc["bottom"].GetInt();
								auto right = doc["right"].GetInt();

								mViewport = Renderer::Rect(left * px, top * py, (width - right) * px, (height - bottom) * py);
							}
						}
					}

					fclose(fp);
				}
			}

			mDecoration = new ImageComponent(mWindow, true);
			mDecoration->setImage(sets[setId].imageUrl);
			mDecoration->setOrigin(0.5f, 0.5f);
			mDecoration->setPosition(Renderer::getScreenWidth() / 2.0f, (float)Renderer::getScreenHeight() / 2.0f);
			mDecoration->setMaxSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
		}
	}
	*/
	if (!Settings::getInstance()->getBool("SlideshowScreenSaverGameName"))
		return;

	if (Settings::getInstance()->getBool("ScreenSaverMarquee") && Utils::FileSystem::exists(game->getMarqueePath()))
	{
		mMarquee = new ImageComponent(mWindow, true);
		mMarquee->setImage(game->getMarqueePath());
		mMarquee->setOrigin(0.5f, 0.5f);
		mMarquee->setPosition(mViewport.x / 2.0f + mViewport.w * 0.50f, mViewport.y / 2.0f + mViewport.h * 0.18f);
		mMarquee->setMaxSize((float)mViewport.w * 0.40f, (float)mViewport.h * 0.22f);
	}
	
	auto ph = ThemeData::getMenuTheme()->Text.font->getPath();
	auto sz = mViewport.h / 16.f;
	auto font = Font::get(sz, ph);

	int h = mViewport.h / 4.0f;
	int fh = font->getLetterHeight();

	mLabelGame = new TextComponent(mWindow);
	mLabelGame->setPosition(mViewport.x / 2.0f, mViewport.y / 2.0f + mViewport.h - h - fh / 2);
	mLabelGame->setSize(mViewport.w, h - fh / 2);
	mLabelGame->setHorizontalAlignment(ALIGN_CENTER);
	mLabelGame->setVerticalAlignment(ALIGN_CENTER);
	mLabelGame->setColor(0xFFFFFFFF);
	mLabelGame->setGlowColor(0x00000040);
	mLabelGame->setGlowSize(3);
	mLabelGame->setFont(font);
	mLabelGame->setText(game->getName());

	mLabelSystem = new TextComponent(mWindow);
	mLabelSystem->setPosition(mViewport.x / 2.0f, mViewport.y / 2.0f + mViewport.h - h + fh / 2);
	mLabelSystem->setSize(mViewport.w, h + fh / 2);
	mLabelSystem->setHorizontalAlignment(ALIGN_CENTER);
	mLabelSystem->setVerticalAlignment(ALIGN_CENTER);
	mLabelSystem->setColor(0xD0D0D0FF);
	mLabelSystem->setGlowColor(0x00000060);
	mLabelSystem->setGlowSize(2);
	mLabelSystem->setFont(ph, sz * 0.66);
	mLabelSystem->setText(game->getSystem()->getFullName());
}

void GameScreenSaverBase::render(const Transform4x4f& transform)
{
	if (mMarquee)
	{
		mMarquee->setOpacity(mOpacity);
		mMarquee->render(transform);
	}
	else if (mLabelGame)
	{
		mLabelGame->setOpacity(mOpacity);
		mLabelGame->render(transform);
	}

	if (mLabelSystem)
	{
		mLabelSystem->setOpacity(mOpacity);
		mLabelSystem->render(transform);
	}

	if (mDecoration)
	{
		mDecoration->setOpacity(mOpacity);
		mDecoration->render(transform);
	}

	if (mLabelDate)
	{
		mLabelDate->setOpacity(255);
		mLabelDate->render(transform);
	}

	if (mLabelTime)
	{
		mLabelTime->setOpacity(255);
		mLabelTime->render(transform);
	}
}

void GameScreenSaverBase::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	
	if (Settings::getInstance()->getBool("ScreenSaverDateTime"))
	{
		mDateTimeUpdateAccumulator += deltaTime;
		if (mDateTimeUpdateAccumulator >= DATE_TIME_UPDATE_INTERVAL)
		{
			mDateTimeUpdateAccumulator -= DATE_TIME_UPDATE_INTERVAL;

			time_t now = time(NULL);
			if (now != mDateTimeLastUpdate)
			{
				mDateTimeLastUpdate = now;

				struct tm* timeinfo = localtime(&now);

				const std::string& dateFormat = Settings::getInstance()->getString("ScreenSaverDateFormat");
				const std::string& timeFormat = Settings::getInstance()->getString("ScreenSaverTimeFormat");

				char dateBuffer[64];
				char timeBuffer[64];
				strftime(dateBuffer, sizeof(dateBuffer), dateFormat.c_str(), timeinfo);
				strftime(timeBuffer, sizeof(timeBuffer), timeFormat.c_str(), timeinfo);

				if (mLabelDate)
					mLabelDate->setText(std::string(dateBuffer));

				if (mLabelTime)
					mLabelTime->setText(std::string(timeBuffer));
			}
		}
	}
}

void GameScreenSaverBase::setOpacity(unsigned char opacity)
{
	mOpacity = opacity;
}


// ------------------------------------------------------------------------------------------------------------------------
// IMAGE SCREEN SAVER CLASS
// ------------------------------------------------------------------------------------------------------------------------

ImageScreenSaver::ImageScreenSaver(Window* window) : GameScreenSaverBase(window)
{
	mImage = nullptr;
}

ImageScreenSaver::~ImageScreenSaver()
{
	if (mImage != nullptr)
		delete mImage;
}

void ImageScreenSaver::setImage(const std::string path)
{
	if (mImage == nullptr)
	{
		mImage = new ImageComponent(mWindow, true);
		mImage->setOrigin(0.5f, 0.5f);
		mImage->setPosition((mViewport.x + mViewport.w) / 2.0f, (mViewport.y + mViewport.h) / 2.0f);

		if (Settings::getInstance()->getBool("SlideshowScreenSaverStretch"))
			mImage->setMinSize((float)mViewport.w, (float)mViewport.h);
		else
			mImage->setMaxSize((float)mViewport.w, (float)mViewport.h);
	}

	mImage->setImage(path);
}

bool ImageScreenSaver::hasImage()
{
	return mImage != nullptr && mImage->hasImage();
}

void ImageScreenSaver::render(const Transform4x4f& transform)
{
	if (mImage)
	{
		mImage->setOpacity(mOpacity);

		Renderer::pushClipRect(Vector2i(mViewport.x, mViewport.y), Vector2i(mViewport.w, mViewport.h));
		mImage->render(transform);
		Renderer::popClipRect();
	}

	GameScreenSaverBase::render(transform);
}


// ------------------------------------------------------------------------------------------------------------------------
// VIDEO SCREEN SAVER CLASS
// ------------------------------------------------------------------------------------------------------------------------

VideoScreenSaver::VideoScreenSaver(Window* window) : GameScreenSaverBase(window)
{
	mVideo = nullptr;
	mTime = 0;
	mFade = 1.0;
}

VideoScreenSaver::~VideoScreenSaver()
{
	if (mVideo != nullptr)
		delete mVideo;
}

void VideoScreenSaver::setVideo(const std::string path)
{
	if (mVideo == nullptr)
	{
#ifdef _RPI_
		// Create the correct type of video component
		if (Settings::getInstance()->getBool("ScreenSaverOmxPlayer"))
			mVideo = new VideoPlayerComponent(mWindow, getTitlePath());
		else
#endif
		mVideo = new VideoVlcComponent(mWindow);

		mVideo->topWindow(true);
		mVideo->setOrigin(0.5f, 0.5f);
		
		mVideo->setPosition((mViewport.x + mViewport.w) / 2.0f, (mViewport.y + mViewport.h) / 2.0f);

		if (Settings::getInstance()->getBool("StretchVideoOnScreenSaver"))
			mVideo->setMinSize((float)mViewport.w, (float)mViewport.h);
		else
			mVideo->setMaxSize((float)mViewport.w, (float)mViewport.h);

		mVideo->setVideo(path);
		mVideo->setScreensaverMode(true);
		mVideo->onShow();
	}

	mFade = 1.0;
	mTime = 0;
	mVideo->setVideo(path);
}

#define SUBTITLE_DURATION 4000
#define SUBTITLE_FADE 150

void VideoScreenSaver::render(const Transform4x4f& transform)
{	
	if (mVideo)
	{		
		mVideo->setOpacity(mOpacity);

		Renderer::pushClipRect(Vector2i(mViewport.x, mViewport.y), Vector2i(mViewport.w, mViewport.h));
		mVideo->render(transform);
		Renderer::popClipRect();
	}

#ifdef _RPI_
	if (Settings::getInstance()->getBool("ScreenSaverOmxPlayer"))
		return;
#endif

	if (Settings::getInstance()->getString("ScreenSaverGameInfo") != "never")
	{
		if (mMarquee && mFade != 0)
		{
			mMarquee->setOpacity(mOpacity * mFade);
			mMarquee->render(transform);
		}
		else if (mLabelGame && mFade != 0)
		{
			mLabelGame->setOpacity(mOpacity * mFade);
			mLabelGame->render(transform);
		}

		if (mLabelSystem && mFade != 0)
		{
			mLabelSystem->setOpacity(mOpacity * mFade);
			mLabelSystem->render(transform);
		}
	}
	
	if (mDecoration)
	{
		mDecoration->setOpacity(mOpacity);
		mDecoration->render(transform);		
	}

	if (mLabelDate)
	{
		mLabelDate->setOpacity(255);
		mLabelDate->render(transform);
	}

	if (mLabelTime)
	{
		mLabelTime->setOpacity(255);
		mLabelTime->render(transform);
	}
}

void VideoScreenSaver::update(int deltaTime)
{
	GameScreenSaverBase::update(deltaTime);

	if (mVideo)
	{
		if (Settings::getInstance()->getString("ScreenSaverGameInfo") == "start & end")
		{
			int duration = SUBTITLE_DURATION;
			int end = Settings::getInstance()->getInt("ScreenSaverSwapVideoTimeout") - duration;

			if (mTime >= duration - SUBTITLE_FADE && mTime < duration)
			{
				mFade -= (float)deltaTime / SUBTITLE_FADE;
				if (mFade < 0)
					mFade = 0;
			}
			else if (mTime >= end - SUBTITLE_FADE && mTime < end)
			{
				mFade += (float)deltaTime / SUBTITLE_FADE;
				if (mFade > 1)
					mFade = 1;
			}
			else if (mTime > duration && mTime < end - SUBTITLE_FADE)
				mFade = 0;
		}
	
		mTime += deltaTime;	
		mVideo->update(deltaTime);
	}
}

// ------------------------------------------------------------------------------------------------------------------------
// DATE TIME SCREEN SAVER CLASS
// ------------------------------------------------------------------------------------------------------------------------

#define DATETIME_BG_DIR "/roms/screensaverbgs"

// Flip clock layout, in background.png design pixels (1024x768)
static const float FLIP_DESIGN_W = 1024.0f;
static const float FLIP_DESIGN_H = 768.0f;
static const float FLIP_DIGIT_X[4] = { 163.0f, 325.0f, 550.0f, 712.0f };
static const float FLIP_TOP_Y  = 252.0f;
static const float FLIP_BOT_Y  = 382.0f;
static const float FLIP_HALF_W = 148.0f;
static const float FLIP_HALF_H = 130.0f;
static const float FLIP_AMPM_X = 180.0f;
static const float FLIP_AMPM_Y = 272.0f;
static const float FLIP_AMPM_W = 40.0f;
static const float FLIP_AMPM_H = 21.0f;
static const float FLIP_DATE_Y = 575.0f;
static const float FLIP_CONTENT_TOP = 206.0f;	// frame top
static const float FLIP_CONTENT_H   = 354.0f;	// frame top -> frame bottom (560)
static const int   FLIP_DELAY_MS = 100;

static std::string dateTimeFontPath(const std::string& path)
{
	return ResourceManager::getInstance()->fileExists(path) ? path : FONT_PATH_REGULAR;
}

static void renderClipped(ImageComponent* image, const Renderer::Rect& viewport, const Transform4x4f& transform)
{
	if (image == nullptr || !image->hasImage())
		return;

	Renderer::pushClipRect(Vector2i(viewport.x, viewport.y), Vector2i(viewport.w, viewport.h));
	image->render(transform);
	Renderer::popClipRect();
}

std::map<std::string, std::string> DateTimeScreenSaver::getBackgrounds()
{
	std::map<std::string, std::string> backgrounds;

	if (!Utils::FileSystem::isDirectory(DATETIME_BG_DIR))
		return backgrounds;

	for (auto entry : Utils::FileSystem::getDirContent(DATETIME_BG_DIR))
	{
		if (Utils::FileSystem::isDirectory(entry))
		{
			for (auto sub : Utils::FileSystem::getDirContent(entry))
				if (!Utils::FileSystem::isDirectory(sub))
					backgrounds[Utils::FileSystem::getFileName(sub)] = sub;
		}
		else
			backgrounds[Utils::FileSystem::getFileName(entry)] = entry;
	}

	return backgrounds;
}

DateTimeScreenSaver::DateTimeScreenSaver(Window* window) : GameScreenSaverBase(window),
	mDate(nullptr), mTime(nullptr), mClockFace(nullptr), mHourHand(nullptr), mMinHand(nullptr), mSecHand(nullptr), mBackground(nullptr),
	mBgColor(0x171614FF)
{
	auto settings = Settings::getInstance();
	mClockType   = settings->getString("DateTimeScreenSaverClockType");
	mClockStyle  = settings->getString("DateTimeScreenSaverClockStyle");
	mShowDate    = settings->getBool("DateTimeScreenSaverShowDate");
	mShowSeconds = settings->getBool("DateTimeScreenSaverShowSeconds");
	mBgMode      = settings->getString("DateTimeScreenSaverBackgroundMode");

	std::string clockColor = settings->getString("DateTimeScreenSaverClockColor");
	if (clockColor == "Neon Blue")        mClockColor = 0x1F51FFCC;
	else if (clockColor == "Neon Yellow") mClockColor = 0xFFFF33CC;
	else if (clockColor == "Neon Red")    mClockColor = 0xFF3131CC;
	else if (clockColor == "White")       mClockColor = 0xFFFFFFCC;
	else                                  mClockColor = 0x39FF14CC; // Neon Green

	// CLOCK FONT (Number mode): time / date font files
	bool exoFont = settings->getString("DateTimeScreenSaverClockFont") == "Exo";
	std::string timeFontFile = exoFont ? ":/Exo2-Light.ttf"      : ":/DS-DIGIT.TTF";
	std::string dateFontFile = exoFont ? ":/Exo2-ExtraLight.ttf" : ":/DS-DIGII.TTF";

	// Background
	if (mBgMode == "Color")
	{
		std::string color = settings->getString("DateTimeScreenSaverBackgroundColor");
		if (color == "Night")            mBgColor = 0x171614FF;
		else if (color == "Bistre")      mBgColor = 0x3A2618FF;
		else if (color == "Garnet")      mBgColor = 0x754043FF;
		else if (color == "Beaver")      mBgColor = 0x9A8873FF;
		else if (color == "Black Olive") mBgColor = 0x37423DFF;
	}
	else
	{
		mBgColor = 0x000000FF;
		mBgPath = settings->getString("DateTimeScreenSaverBackgroundPath");
		if (mBgPath != "None")
		{
			auto backgrounds = getBackgrounds();
			auto it = backgrounds.find(mBgPath);
			if (it != backgrounds.cend())
				setImage(it->second);
		}
	}

	time_t now = time(NULL);
	int h = mViewport.h;
	int boxHeight = (int)(h * 0.25f);

	// Date
	if (mShowDate)
	{
		auto font = Font::get((int)(h / 15.0f), dateTimeFontPath(dateFontFile));
		int lh = (int)font->getLetterHeight();

		mDate = new TextComponent(mWindow);
		mDate->setPosition((float)mViewport.x, (float)(mViewport.y + h - (h / 3.8 - lh) - (boxHeight - lh / 2)));
		mDate->setSize((float)mViewport.w, (float)(boxHeight - lh / 2));
		mDate->setHorizontalAlignment(ALIGN_CENTER);
		mDate->setVerticalAlignment(ALIGN_CENTER);
		mDate->setColor(mClockType == "Number" ? mClockColor : 0xFFFFFFFF);
		mDate->setGlowColor(mClockType == "Number" ? ((mClockColor & 0xFFFFFF00) | 0x10) : 0x00000040);
		mDate->setGlowSize(2);
		mDate->setFont(font);
		mDate->setText(Utils::Time::timeToString(now, "%Y/%m/%d"));
	}

	// Digital time
	if (mClockType == "Number")
	{
		// up to double size on every aspect ratio, reduced only as far as needed so the widest
		// possible time (all '0' digits, current 12h/seconds format) keeps 5% padding each side
		int timeSize = (int)(h / 5.0f) * 2;
		auto font = Font::get(timeSize, dateTimeFontPath(timeFontFile));

		bool h12 = Settings::getInstance()->getBool("ClockMode12");
		std::string widest = mShowSeconds ? "00:00:00" : "00:00";
		float textW = font->sizeText(widest).x();
		if (h12)
			textW = std::max(font->sizeText(widest + " AM").x(), font->sizeText(widest + " PM").x());

		float maxW = mViewport.w * 0.9f;
		if (textW > maxW)
			font = Font::get((int)(timeSize * maxW / textW), dateTimeFontPath(timeFontFile));
		int lh = (int)font->getLetterHeight();

		mTime = new TextComponent(mWindow);
		mTime->setSize((float)mViewport.w, (float)lh);
		mTime->setPosition((float)mViewport.x, mViewport.y + h * 0.5f - lh * 0.5f);
		mTime->setHorizontalAlignment(ALIGN_CENTER);
		mTime->setVerticalAlignment(ALIGN_CENTER);
		mTime->setColor(mClockColor);
		mTime->setGlowColor((mClockColor & 0xFFFFFF00) | 0x10);
		mTime->setGlowSize(3);
		mTime->setFont(font);
		mTime->setText(Utils::Time::timeToString(now, timeFormat()));

		// date directly under the time: gap = 0.135 x time letter height (same look on every aspect ratio)
		if (mDate)
		{
			int dateLh = (int)Font::get((int)(h / 15.0f), dateTimeFontPath(dateFontFile))->getLetterHeight();
			mDate->setSize((float)mViewport.w, (float)dateLh);
			mDate->setPosition((float)mViewport.x, mViewport.y + h * 0.5f + lh * 0.5f + lh * 0.135f);
		}
		return;
	}

	// Analog clock
	if (mDate)
	{
		mDate->setFont(Font::get((int)(h / 15.0f), FONT_PATH_REGULAR));
		mDate->setPosition((float)mViewport.x, (float)(mViewport.y + h / 3));
		mDate->setSize((float)mViewport.w, h * 0.1f);

		if (mClockStyle == "style3")
		{
			mDate->setOrigin(0.0f, 0.5f);
			mDate->setPosition((float)(mViewport.x + 0.18 * h), (float)(h / 2));
		}
	}

	if (isFlipStyle())
	{
		initFlip();
		return;
	}

	mClockFace = createClockLayer("clock.png", false);
	mHourHand  = createClockLayer("hour_hand.png", true);
	mMinHand   = createClockLayer("min_hand.png", true);
	mSecHand   = createClockLayer("sec_hand.png", true);
}

// Follows the global ES clock setting (SHOW CLOCK IN 12-HOUR FORMAT); %-I = hour without leading zero
std::string DateTimeScreenSaver::timeFormat() const
{
	if (Settings::getInstance()->getBool("ClockMode12"))
		return mShowSeconds ? "%-I:%M:%S %p" : "%-I:%M %p";

	return mShowSeconds ? "%H:%M:%S" : "%H:%M";
}

DateTimeScreenSaver::~DateTimeScreenSaver()
{
	delete mDate;
	delete mTime;
	delete mClockFace;
	delete mHourHand;
	delete mMinHand;
	delete mSecHand;
	delete mBackground;

	for (int i = 0; i < 4; i++)
	{
		delete mFlipTop[i];
		delete mFlipBot[i];
	}
	delete mFlipAmPm;
}

ImageComponent* DateTimeScreenSaver::createClockLayer(const std::string& file, bool rotates)
{
	auto layer = new ImageComponent(mWindow, true, true);
	layer->setOrigin(0.5f, 0.5f);
	if (rotates)
		layer->setRotationOrigin(0.5f, 0.5f);
	layer->setPosition(mViewport.x + mViewport.w * 0.5f, mViewport.y + mViewport.h * 0.5f);
	layer->setMaxSize((float)mViewport.w, (float)mViewport.h);
	layer->setImage(":/clock/" + mClockStyle + "/" + file);
	return layer;
}

// Flip clock: all coordinates are in the 1024x768 design space of background.png,
// mapped to the screen through mFlipScale / mFlipX / mFlipY
ImageComponent* DateTimeScreenSaver::createFlipLayer(float x, float y, float w, float h)
{
	auto layer = new ImageComponent(mWindow, true, true);
	layer->setOrigin(0.0f, 0.0f);
	layer->setPosition(mFlipX + x * mFlipScale, mFlipY + y * mFlipScale);
	layer->setResize(w * mFlipScale, h * mFlipScale);
	return layer;
}

// 12h: leading blank for 1-9 ; 24h: leading 0
void DateTimeScreenSaver::getFlipGlyphs(const struct tm* t, std::string glyphs[4]) const
{
	int hour = t->tm_hour;

	if (mFlip12h)
	{
		hour = hour % 12;
		if (hour == 0)
			hour = 12;
		glyphs[0] = hour >= 10 ? "1" : "blank";
	}
	else
		glyphs[0] = std::to_string(hour / 10);

	glyphs[1] = std::to_string(hour % 10);
	glyphs[2] = std::to_string(t->tm_min / 10);
	glyphs[3] = std::to_string(t->tm_min % 10);
}

void DateTimeScreenSaver::initFlip()
{
	mFlip12h = Settings::getInstance()->getBool("ClockMode12");
	mFlipDir = ":/clock/" + mClockStyle + "/";

	// Fit visible content (frame) into viewport: width-limited on 4:3 and 16:9,
	// frame centred; canvas outside the viewport is clipped
	float sx = mViewport.w / FLIP_DESIGN_W;
	float sy = mViewport.h / FLIP_CONTENT_H;
	mFlipScale = sx < sy ? sx : sy;
	mFlipX = mViewport.x + (mViewport.w - FLIP_DESIGN_W * mFlipScale) * 0.5f;
	mFlipY = mViewport.y + mViewport.h * 0.5f - (FLIP_CONTENT_TOP + FLIP_CONTENT_H * 0.5f) * mFlipScale;

	mClockFace = createFlipLayer(0.0f, 0.0f, FLIP_DESIGN_W, FLIP_DESIGN_H);
	mClockFace->setImage(mFlipDir + "background.png");

	time_t now = time(NULL);
	std::string glyphs[4];
	getFlipGlyphs(localtime(&now), glyphs);

	for (int i = 0; i < 4; i++)
	{
		mFlipTop[i] = createFlipLayer(FLIP_DIGIT_X[i], FLIP_TOP_Y, FLIP_HALF_W, FLIP_HALF_H);
		mFlipBot[i] = createFlipLayer(FLIP_DIGIT_X[i], FLIP_BOT_Y, FLIP_HALF_W, FLIP_HALF_H);
		mFlipTopGlyph[i] = glyphs[i];
		mFlipBotGlyph[i] = glyphs[i];
		mFlipTop[i]->setImage(mFlipDir + glyphs[i] + "a.png");
		mFlipBot[i]->setImage(mFlipDir + glyphs[i] + "b.png");
	}

	if (mFlip12h)
	{
		mFlipAmPmGlyph = localtime(&now)->tm_hour < 12 ? "AM" : "PM";
		mFlipAmPm = createFlipLayer(FLIP_AMPM_X, FLIP_AMPM_Y, FLIP_AMPM_W, FLIP_AMPM_H);
		mFlipAmPm->setImage(mFlipDir + mFlipAmPmGlyph + ".png");
	}

	if (mDate)
	{
		mDate->setFont(Font::get((int)(mViewport.h / 15.0f), dateTimeFontPath(":/Helvetica-Regular.ttf")));
		mDate->setPosition((float)mViewport.x, mFlipY + FLIP_DATE_Y * mFlipScale);
		mDate->setSize((float)mViewport.w, mViewport.h * 0.1f);
	}
}

// Top half changes immediately, bottom half follows FLIP_DELAY_MS later
void DateTimeScreenSaver::updateFlip(const struct tm* t, int deltaTime)
{
	std::string glyphs[4];
	getFlipGlyphs(t, glyphs);

	for (int i = 0; i < 4; i++)
	{
		if (glyphs[i] != mFlipTopGlyph[i])
		{
			mFlipTopGlyph[i] = glyphs[i];
			mFlipTop[i]->setImage(mFlipDir + glyphs[i] + "a.png");
			mFlipDelay[i] = FLIP_DELAY_MS;
		}
		else if (mFlipBotGlyph[i] != mFlipTopGlyph[i])
		{
			mFlipDelay[i] -= deltaTime;
			if (mFlipDelay[i] <= 0)
			{
				mFlipBotGlyph[i] = mFlipTopGlyph[i];
				mFlipBot[i]->setImage(mFlipDir + mFlipBotGlyph[i] + "b.png");
			}
		}
	}

	if (mFlipAmPm)
	{
		std::string ampm = t->tm_hour < 12 ? "AM" : "PM";
		if (ampm != mFlipAmPmGlyph)
		{
			mFlipAmPmGlyph = ampm;
			mFlipAmPm->setImage(mFlipDir + ampm + ".png");
		}
	}
}

void DateTimeScreenSaver::setImage(const std::string path)
{
	if (mBackground == nullptr)
	{
		mBackground = new ImageComponent(mWindow, true, true);
		mBackground->setOrigin(0.5f, 0.5f);
		mBackground->setPosition(mViewport.x + mViewport.w * 0.5f, mViewport.y + mViewport.h * 0.5f);
		mBackground->setMaxSize((float)mViewport.w, (float)mViewport.h);
	}

	mBackground->setImage(path);
}

bool DateTimeScreenSaver::hasImage()
{
	return mBackground != nullptr && mBackground->hasImage();
}

void DateTimeScreenSaver::update(int deltaTime)
{
	// GameScreenSaverBase::update skipped: slideshow date/time overlay is off for this screensaver
	GuiComponent::update(deltaTime);

	time_t now = time(NULL);
	struct tm* t = localtime(&now);

	if (mDate)
	{
		std::string date = Utils::Time::timeToString(now, "%Y/%m/%d");
		if (mClockType != "Number" && mClockStyle == "style3")
			date = date.substr(2);
		mDate->setText(date);
	}

	if (mClockType == "Number")
	{
		if (mTime)
			mTime->setText(Utils::Time::timeToString(now, timeFormat()));
		return;
	}

	if (isFlipStyle())
	{
		updateFlip(t, deltaTime);
		return;
	}

	mHourHand->setRotationDegrees(t->tm_hour * 30.0f + (t->tm_min / 60.0f) * 30.0f);
	mMinHand->setRotationDegrees(t->tm_min * 6.0f);
	if (mShowSeconds)
		mSecHand->setRotationDegrees(t->tm_sec * 6.0f);
}

void DateTimeScreenSaver::render(const Transform4x4f& transform)
{
	// GameScreenSaverBase::render skipped: slideshow date/time overlay is off for this screensaver
	if (mBgMode != "Color" && hasImage())
		renderClipped(mBackground, mViewport, transform);
	else
		Renderer::drawRect(0.0f, 0.0f, Renderer::getScreenWidth(), Renderer::getScreenHeight(), mBgColor, mBgColor);

	if (mClockType == "Number")
	{
		if (mDate)
			mDate->render(transform);
		if (mTime)
			mTime->render(transform);
		return;
	}

	renderClipped(mClockFace, mViewport, transform);

	if (isFlipStyle())
	{
		for (int i = 0; i < 4; i++)
		{
			renderClipped(mFlipTop[i], mViewport, transform);
			renderClipped(mFlipBot[i], mViewport, transform);
		}

		renderClipped(mFlipAmPm, mViewport, transform);

		if (mDate)
			mDate->render(transform);
		return;
	}

	if (mDate)
		mDate->render(transform);

	renderClipped(mHourHand, mViewport, transform);
	renderClipped(mMinHand, mViewport, transform);

	if (mShowSeconds)
		renderClipped(mSecHand, mViewport, transform);
}