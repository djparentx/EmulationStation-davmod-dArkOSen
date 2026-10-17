#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_DETAILED_CONTAINER_H
#define ES_APP_VIEWS_GAME_LIST_DETAILED_CONTAINER_H

#include "components/DateTimeComponent.h"
#include "components/RatingComponent.h"
#include "components/ScrollableContainer.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include <memory>
#include <string>
#include <vector>

class FileData;
class ISimpleGameListView;
class ThemeData;
class VideoComponent;
class Window;

// Batocera / AmberELEC DetailedContainer: the game information panel (md_* theme elements) shared by
// the detailed, video, grid and gamecarousel gamelist views.
// Each view type keeps its own default layout and behaviour; the elements below are available to all of them.
class DetailedContainer
{
public:
	enum DetailedContainerType
	{
		DetailedView,
		VideoView,
		GridView
	};

	DetailedContainer(ISimpleGameListView* parent, GuiComponent* list, Window* window, DetailedContainerType viewType);
	~DetailedContainer();

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme);

	// file == nullptr : nothing selected (fade out) - isClearing : the list is being cleared / populated (no fades)
	// restartStoryboards (detailed) : the selected game changed, replay the activate storyboards of the md_* elements
	void updateControls(FileData* file, bool isClearing, int moveBy, bool restartStoryboards);

	VideoComponent* getVideo() { return mVideo; }
	ImageComponent* getImage() { return mImage; }
	ImageComponent* getThumbnail() { return mThumbnail; }
	ImageComponent* getMarquee() { return mMarquee; }

private:
	struct MdImage
	{
		std::string id;
		std::vector<std::string> keys; // metadata, in order of preference
		ImageComponent* component;
	};

	struct MdFlagImage
	{
		std::string id;
		ImageComponent* component;
	};

	void applyTheme(const std::shared_ptr<ThemeData>& theme);

	void createImage();
	void createThumbnail();
	void createVideo();
	void createMarquee();
	ImageComponent* createMdImage();

	void loadIfThemed(ImageComponent** pImage, const std::shared_ptr<ThemeData>& theme, const std::string& element, bool loadPath);

	void initMDLabels();
	void initMDValues();

	std::vector<TextComponent*> getMDLabels();
	std::vector<GuiComponent*> getMDValues();
	std::vector<GuiComponent*> getBatoceraComponents();

	std::string getMetadata(FileData* file, const std::string& name);
	std::string getMediaPath(FileData* file, const std::string& key);

	void updateBatoceraElements(FileData* file);
	void loadPerGameExtras(FileData* file);
	void clearPerGameExtras();

	void addChild(GuiComponent* comp);
	void removeChild(GuiComponent* comp);
	bool isChild(GuiComponent* comp);

	ISimpleGameListView* mParent;
	GuiComponent* mList;
	Window* mWindow;
	DetailedContainerType mViewType;
	Vector2f mSize;

	ImageComponent* mImage;
	ImageComponent* mThumbnail;
	ImageComponent* mMarquee;
	VideoComponent* mVideo;

	TextComponent mLblRating, mLblReleaseDate, mLblDeveloper, mLblPublisher, mLblGenre, mLblPlayers, mLblLastPlayed, mLblPlayCount;

	RatingComponent mRating;
	DateTimeComponent mReleaseDate;
	TextComponent mDeveloper;
	TextComponent mPublisher;
	TextComponent mGenre;
	TextComponent mPlayers;
	DateTimeComponent mLastPlayed;
	TextComponent mPlayCount;
	TextComponent mName;

	ScrollableContainer mDescContainer;
	TextComponent mDescription;

	// Batocera elements
	TextComponent mLblGameTime, mGameTime, mLblFavorite, mTextFavorite;

	std::vector<MdImage> mMdImages;           // md_fanart, md_boxart, md_wheel, md_image_only ...
	std::vector<MdFlagImage> mMdFlagImages;   // md_favorite, md_notfavorite, md_kidgame, md_manual ...
	ImageComponent* mFlag;                    // md_flag

	// <gameextras> per-game theme file
	std::shared_ptr<ThemeData> mTheme;
	std::shared_ptr<ThemeData> mCustomTheme;
	std::string mPerGameExtrasPath;
	bool mIsPerGameExtrasPathBinding;
	std::string mCustomThemePath;
	std::vector<GuiComponent*> mPerGameExtras;
};

#endif // ES_APP_VIEWS_GAME_LIST_DETAILED_CONTAINER_H
