#include "views/gamelist/DetailedContainer.h"

#include "views/gamelist/ISimpleGameListView.h"
#include "animations/LambdaAnimation.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"
#include "BindingManager.h"
#include "FileData.h"
#include "LangParser.h"
#include "Settings.h"
#include "SystemData.h"
#include "ThemeData.h"
#include "Window.h"
#include "EsLocale.h"

#ifdef _RPI_
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"

DetailedContainer::DetailedContainer(ISimpleGameListView* parent, GuiComponent* list, Window* window, DetailedContainerType viewType) :
	mParent(parent), mList(list), mWindow(window), mViewType(viewType),
	mImage(nullptr), mThumbnail(nullptr), mMarquee(nullptr), mVideo(nullptr), mFlag(nullptr),

	mLblRating(window), mLblReleaseDate(window), mLblDeveloper(window), mLblPublisher(window),
	mLblGenre(window), mLblPlayers(window), mLblLastPlayed(window), mLblPlayCount(window),

	mRating(window), mReleaseDate(window), mDeveloper(window), mPublisher(window),
	mGenre(window), mPlayers(window), mLastPlayed(window), mPlayCount(window),
	mName(window),

	mDescContainer(window), mDescription(window),
	mLblGameTime(window), mGameTime(window), mLblFavorite(window), mTextFavorite(window),
	mIsPerGameExtrasPathBinding(false)
{
	mSize = mParent->getSize();
	const float padding = 0.01f;

	// Batocera image elements - metadata in order of preference
	mMdImages =
	{
		{ "md_fanart",          { "fanart", "titleshot", "image" },   nullptr },
		{ "md_titleshot",       { "titleshot", "image" },             nullptr },
		{ "md_boxart",          { "boxart", "thumbnail" },            nullptr },
		{ "md_wheel",           { "wheel", "marquee" },               nullptr },
		{ "md_cartridge",       { "cartridge" },                      nullptr },
		{ "md_boxback",         { "boxback" },                        nullptr },
		{ "md_mix",             { "mix", "image", "thumbnail" },      nullptr },

		{ "md_image_only",      { "image" },                          nullptr },
		{ "md_thumbnail_only",  { "thumbnail" },                      nullptr },
		{ "md_marquee_only",    { "marquee" },                        nullptr },
		{ "md_wheel_only",      { "wheel" },                          nullptr },
		{ "md_fanart_only",     { "fanart" },                         nullptr },
		{ "md_titleshot_only",  { "titleshot" },                      nullptr },
		{ "md_boxart_only",     { "boxart" },                         nullptr },
		{ "md_boxback_only",    { "boxback" },                        nullptr },
		{ "md_cartridge_only",  { "cartridge" },                      nullptr },
		{ "md_mix_only",        { "mix" },                            nullptr }
	};

	// Batocera state images (shown / hidden for the selected game)
	const char* flagImages[] =
	{
		"md_favorite", "md_notfavorite", "md_hidden", "md_kidgame", "md_notkidgame",
		"md_manual", "md_nomanual", "md_map", "md_nomap", "md_savestate", "md_nosavestate",
		"md_cheevos", "md_notcheevos", "md_netplay", "md_notnetplay",
		"md_gungame", "md_notgungame", "md_wheelgame", "md_notwheelgame",
		"md_trackballgame", "md_nottrackballgame", "md_spinnergame", "md_notspinnergame"
	};

	for (auto id : flagImages)
		mMdFlagImages.push_back({ id, nullptr });

	if (mViewType == VideoView)
	{
		// the video view always has a marquee and a video
		createMarquee();
		createVideo();
	}
	else if (mViewType == DetailedView)
		createImage();

	// metadata labels + values
	if (mViewType == DetailedView)
	{
		mLblRating.setText(_("Rating") + ": ");
		mLblReleaseDate.setText(_("Released") + ": ");
		mLblDeveloper.setText(_("Developer") + ": ");
		mLblPublisher.setText(_("Publisher") + ": ");
		mLblGenre.setText(_("Genre") + ": ");
		mLblPlayers.setText(_("Players") + ": ");
		mLblLastPlayed.setText(_("Last played") + ": ");
		mLblPlayCount.setText(_("Times played"));
	}
	else
	{
		mLblRating.setText("Rating: ");
		mLblReleaseDate.setText("Released: ");
		mLblDeveloper.setText("Developer: ");
		mLblPublisher.setText("Publisher: ");
		mLblGenre.setText("Genre: ");
		mLblPlayers.setText("Players: ");
		mLblLastPlayed.setText("Last played: ");
		mLblPlayCount.setText("Times played: ");
	}

	addChild(&mLblRating);
	addChild(&mRating);
	addChild(&mLblReleaseDate);
	addChild(&mReleaseDate);
	addChild(&mLblDeveloper);
	addChild(&mDeveloper);
	addChild(&mLblPublisher);
	addChild(&mPublisher);
	addChild(&mLblGenre);
	addChild(&mGenre);
	addChild(&mLblPlayers);
	addChild(&mPlayers);
	addChild(&mLblLastPlayed);
	mLastPlayed.setDisplayRelative(true);
	addChild(&mLastPlayed);
	addChild(&mLblPlayCount);
	addChild(&mPlayCount);

	mName.setPosition(mSize.x(), mSize.y());
	mName.setDefaultZIndex(40);
	mName.setColor(0xAAAAAAFF);
	mName.setFont(Font::get(FONT_SIZE_MEDIUM));
	mName.setHorizontalAlignment(ALIGN_CENTER);
	addChild(&mName);

	mDescContainer.setPosition(mSize.x() * padding, mSize.y() * 0.65f);
	mDescContainer.setSize(mSize.x() * (0.50f - 2 * padding), mSize.y() - mDescContainer.getPosition().y());
	mDescContainer.setAutoScroll(true);
	mDescContainer.setDefaultZIndex(40);
	addChild(&mDescContainer);

	mDescription.setFont(Font::get(FONT_SIZE_SMALL));
	mDescription.setSize(mDescContainer.getSize().x(), 0);
	mDescContainer.addChild(&mDescription);

	// Batocera md_gametime / md_favorite texts: only shown when the theme defines them
	mLblGameTime.setText(_("Game time") + ": ");
	mLblFavorite.setText(_("Favorite") + ": ");

	for (auto text : { &mLblGameTime, &mGameTime, &mLblFavorite, &mTextFavorite })
	{
		text->setFont(Font::get(FONT_SIZE_SMALL));
		text->setDefaultZIndex(40);
		text->setVisible(false);
		addChild(text);
	}

	initMDLabels();
	initMDValues();
}

DetailedContainer::~DetailedContainer()
{
	clearPerGameExtras();

	if (mThumbnail != nullptr)
		delete mThumbnail;

	if (mImage != nullptr)
		delete mImage;

	if (mMarquee != nullptr)
		delete mMarquee;

	if (mVideo != nullptr)
		delete mVideo;

	if (mFlag != nullptr)
		delete mFlag;

	for (auto& md : mMdImages)
		if (md.component != nullptr)
			delete md.component;

	for (auto& md : mMdFlagImages)
		if (md.component != nullptr)
			delete md.component;
}

void DetailedContainer::addChild(GuiComponent* comp) { mParent->addChild(comp); }
void DetailedContainer::removeChild(GuiComponent* comp) { mParent->removeChild(comp); }
bool DetailedContainer::isChild(GuiComponent* comp) { return mParent->isChild(comp); }

void DetailedContainer::createImage()
{
	if (mImage != nullptr)
		return;

	const float padding = 0.01f;

	if (mViewType == VideoView)
	{
		mImage = new ImageComponent(mWindow, true);
		mImage->setOrigin(0.5f, 0.5f);
		mImage->setPosition(2.0f, 2.0f);
		mImage->setMaxSize(100.0f, 100.0f);
	}
	else
	{
		mImage = new ImageComponent(mWindow, mViewType == DetailedView);
		if (mViewType == GridView)
			mImage->setAllowFading(false);

		mImage->setOrigin(0.5f, 0.5f);
		mImage->setPosition(mSize.x() * 0.25f, mList->getPosition().y() + mSize.y() * 0.2125f);
		mImage->setMaxSize(mSize.x() * (0.50f - 2 * padding), mSize.y() * 0.4f);
	}

	mImage->setDefaultZIndex(30);
	addChild(mImage);
}

void DetailedContainer::createThumbnail()
{
	if (mThumbnail != nullptr)
		return;

	const float padding = 0.01f;

	mThumbnail = new ImageComponent(mWindow);
	mThumbnail->setAllowFading(false);
	mThumbnail->setOrigin(0.5f, 0.5f);
	mThumbnail->setPosition(mSize.x() * 0.25f, mList->getPosition().y() + mSize.y() * 0.2125f);
	mThumbnail->setMaxSize(mSize.x() * (0.50f - 2 * padding), mSize.y() * 0.4f);
	mThumbnail->setDefaultZIndex(30);
	addChild(mThumbnail);
}

void DetailedContainer::createVideo()
{
	if (mVideo != nullptr)
		return;

	const float padding = 0.01f;

	// Create the correct type of video window
#ifdef _RPI_
	if (Settings::getInstance()->getBool("VideoOmxPlayer"))
		mVideo = new VideoPlayerComponent(mWindow, "");
	else
#endif
		mVideo = new VideoVlcComponent(mWindow, "");

	// Default is IMAGE in Recalbox themes -> video view does not exist
	mVideo->setSnapshotSource(IMAGE);

	mVideo->setOrigin(0.5f, 0.5f);
	mVideo->setPosition(mSize.x() * 0.25f, mSize.y() * 0.4f);
	mVideo->setSize(mSize.x() * (0.5f - 2 * padding), mSize.y() * 0.4f);
	if (mViewType != VideoView)
		mVideo->setStartDelay(2000);
	mVideo->setDefaultZIndex(31);
	addChild(mVideo);
}

void DetailedContainer::createMarquee()
{
	if (mMarquee != nullptr)
		return;

	const float padding = 0.01f;

	mMarquee = new ImageComponent(mWindow);
	if (mViewType == GridView)
		mMarquee->setAllowFading(false);

	mMarquee->setOrigin(0.5f, 0.5f);
	mMarquee->setPosition(mSize.x() * 0.25f, mSize.y() * 0.10f);
	mMarquee->setMaxSize(mSize.x() * (0.5f - 2 * padding), mSize.y() * 0.18f);
	mMarquee->setDefaultZIndex(35);
	addChild(mMarquee);
}

ImageComponent* DetailedContainer::createMdImage()
{
	const float padding = 0.01f;

	auto image = new ImageComponent(mWindow);
	image->setAllowFading(false);
	image->setOrigin(0.5f, 0.5f);
	image->setPosition(mSize.x() * 0.25f, mList->getPosition().y() + mSize.y() * 0.2125f);
	image->setMaxSize(mSize.x() * (0.50f - 2 * padding), mSize.y() * 0.4f);
	image->setDefaultZIndex(30);
	addChild(image);
	return image;
}

// Batocera: created when the theme defines a visible element, removed otherwise
void DetailedContainer::loadIfThemed(ImageComponent** pImage, const std::shared_ptr<ThemeData>& theme, const std::string& element, bool loadPath)
{
	using namespace ThemeFlags;

	auto elem = theme->getElement(mParent->getName(), element, "");
	if (elem && elem->type == "image" && elem->properties.size() > 0 && (!elem->has("visible") || elem->get<bool>("visible")))
	{
		if (*pImage == nullptr)
			*pImage = createMdImage();

		(*pImage)->applyTheme(theme, mParent->getName(), element, loadPath ? ALL : ALL ^ (PATH));
	}
	else if (*pImage != nullptr)
	{
		removeChild(*pImage);
		delete *pImage;
		*pImage = nullptr;
	}
}

void DetailedContainer::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	mTheme = theme;
	mCustomTheme = nullptr;
	mCustomThemePath.clear();
	clearPerGameExtras();

	// <gameextras><path>: per-game theme files
	mPerGameExtrasPath.clear();
	mIsPerGameExtrasPathBinding = false;

	const ThemeData::ThemeElement* elem = theme->getElement(mParent->getName(), "gameextras", "gameextras");
	if (elem && elem->has("path_binding"))
	{
		mPerGameExtrasPath = elem->get<std::string>("path_binding");
		mIsPerGameExtrasPathBinding = true;
	}
	else if (elem && elem->has("path"))
		mPerGameExtrasPath = elem->get<std::string>("path");

	applyTheme(theme);
}

void DetailedContainer::applyTheme(const std::shared_ptr<ThemeData>& theme)
{
	using namespace ThemeFlags;

	const std::string name = mParent->getName();

	if (mViewType == VideoView)
	{
		mMarquee->applyTheme(theme, name, "md_marquee", ALL ^ (PATH));

		if (theme->getElement(name, "md_image", "image"))
		{
			createImage();
			mImage->applyTheme(theme, name, "md_image", ALL ^ (PATH));
		}
		else if (mImage != nullptr)
		{
			removeChild(mImage);
			delete mImage;
			mImage = nullptr;
		}

		mVideo->applyTheme(theme, name, "md_video", ALL ^ (PATH));
		mName.applyTheme(theme, name, "md_name", ALL);
	}
	else
	{
		mName.applyTheme(theme, name, "md_name", ALL);

		if (theme->getElement(name, "md_video", "video"))
		{
			createVideo();
			mVideo->applyTheme(theme, name, "md_video", ALL ^ (PATH));
		}
		else if (mVideo != nullptr)
		{
			removeChild(mVideo);
			delete mVideo;
			mVideo = nullptr;
		}

		// detailed view: an image is always there when there's no video
		if ((mViewType == DetailedView && mVideo == nullptr) || theme->getElement(name, "md_image", "image"))
		{
			createImage();
			mImage->applyTheme(theme, name, "md_image", ALL ^ (PATH));
		}
		else if (mImage != nullptr)
		{
			removeChild(mImage);
			delete mImage;
			mImage = nullptr;
		}
	}

	if (theme->getElement(name, "md_thumbnail", "image"))
	{
		createThumbnail();
		mThumbnail->applyTheme(theme, name, "md_thumbnail", ALL ^ (PATH));
	}
	else if (mThumbnail != nullptr)
	{
		removeChild(mThumbnail);
		delete mThumbnail;
		mThumbnail = nullptr;
	}

	if (mViewType != VideoView)
	{
		if (theme->getElement(name, "md_marquee", "image"))
		{
			createMarquee();
			mMarquee->applyTheme(theme, name, "md_marquee", ALL ^ (PATH));
		}
		else if (mMarquee != nullptr)
		{
			removeChild(mMarquee);
			delete mMarquee;
			mMarquee = nullptr;
		}
	}

	initMDLabels();
	std::vector<TextComponent*> labels = getMDLabels();

	const char* lblElements[8] = {
		"md_lbl_rating", "md_lbl_releasedate", "md_lbl_developer", "md_lbl_publisher",
		"md_lbl_genre", "md_lbl_players", "md_lbl_lastplayed", "md_lbl_playcount"
	};

	for (unsigned int i = 0; i < labels.size(); i++)
		labels[i]->applyTheme(theme, name, lblElements[i], ALL);

	initMDValues();
	std::vector<GuiComponent*> values = getMDValues();

	const char* valElements[8] = {
		"md_rating", "md_releasedate", "md_developer", "md_publisher",
		"md_genre", "md_players", "md_lastplayed", "md_playcount"
	};

	for (unsigned int i = 0; i < values.size(); i++)
		values[i]->applyTheme(theme, name, valElements[i], ALL ^ ThemeFlags::TEXT);

	if (mViewType == GridView)
	{
		if (theme->getElement(name, "md_description", "text"))
		{
			mDescContainer.applyTheme(theme, name, "md_description", POSITION | ThemeFlags::SIZE | Z_INDEX);
			mDescription.setSize(mDescContainer.getSize().x(), 0);
			mDescription.applyTheme(theme, name, "md_description", ALL ^ (POSITION | ThemeFlags::SIZE | ThemeFlags::ORIGIN | TEXT | ROTATION));

			if (!isChild(&mDescContainer))
				addChild(&mDescContainer);
		}
		else
			removeChild(&mDescContainer);
	}
	else
	{
		mDescContainer.applyTheme(theme, name, "md_description", POSITION | ThemeFlags::SIZE | Z_INDEX | VISIBLE);
		mDescription.setSize(mDescContainer.getSize().x(), 0);
		mDescription.applyTheme(theme, name, "md_description", ALL ^ (POSITION | ThemeFlags::SIZE | ThemeFlags::ORIGIN | TEXT | ROTATION));
	}

	// Batocera elements
	std::vector<std::pair<TextComponent*, std::string>> batoceraTexts =
	{
		{ &mLblGameTime, "md_lbl_gametime" }, { &mGameTime, "md_gametime" },
		{ &mLblFavorite, "md_lbl_favorite" }, { &mTextFavorite, "md_favorite" }
	};

	for (auto& bt : batoceraTexts)
	{
		// md_favorite can be an image or a text element: look it up without a type mismatch warning
		auto textElem = theme->getElement(name, bt.second, "");
		bool themed = textElem != nullptr && textElem->type == "text";
		bt.first->setVisible(themed);
		if (themed)
			bt.first->applyTheme(theme, name, bt.second, bt.first == &mGameTime || bt.first == &mTextFavorite ? ALL ^ ThemeFlags::TEXT : ALL);
	}

	loadIfThemed(&mFlag, theme, "md_flag", false);

	for (auto& md : mMdImages)
		loadIfThemed(&md.component, theme, md.id, false);

	for (auto& md : mMdFlagImages)
		loadIfThemed(&md.component, theme, md.id, true);

	mParent->sortChildren();
}

void DetailedContainer::initMDLabels()
{
	std::vector<TextComponent*> components = getMDLabels();

	const unsigned int colCount = 2;
	const unsigned int rowCount = (int)(components.size() / 2);

	Vector3f start(mSize.x() * 0.01f, mSize.y() * 0.625f, 0.0f);

	const float colSize = (mSize.x() * 0.48f) / colCount;
	const float rowPadding = 0.01f * mSize.y();

	for (unsigned int i = 0; i < components.size(); i++)
	{
		const unsigned int row = i % rowCount;
		Vector3f pos(0.0f, 0.0f, 0.0f);
		if (row == 0)
		{
			pos = start + Vector3f(colSize * (i / rowCount), 0, 0);
		}
		else
		{
			// work from the last component
			GuiComponent* lc = components[i - 1];
			pos = lc->getPosition() + Vector3f(0, lc->getSize().y() + rowPadding, 0);
		}

		components[i]->setFont(Font::get(FONT_SIZE_SMALL));
		components[i]->setPosition(pos);
		components[i]->setDefaultZIndex(40);
	}
}

void DetailedContainer::initMDValues()
{
	std::vector<TextComponent*> labels = getMDLabels();
	std::vector<GuiComponent*> values = getMDValues();

	std::shared_ptr<Font> defaultFont = Font::get(FONT_SIZE_SMALL);
	mRating.setSize(defaultFont->getHeight() * 5.0f, (float)defaultFont->getHeight());
	mReleaseDate.setFont(defaultFont);
	mDeveloper.setFont(defaultFont);
	mPublisher.setFont(defaultFont);
	mGenre.setFont(defaultFont);
	mPlayers.setFont(defaultFont);
	mLastPlayed.setFont(defaultFont);
	mPlayCount.setFont(defaultFont);

	float bottom = 0.0f;

	const float colSize = (mSize.x() * 0.48f) / 2;
	for (unsigned int i = 0; i < labels.size(); i++)
	{
		const float heightDiff = (labels[i]->getSize().y() - values[i]->getSize().y()) / 2;
		values[i]->setPosition(labels[i]->getPosition() + Vector3f(labels[i]->getSize().x(), heightDiff, 0));
		values[i]->setSize(colSize - labels[i]->getSize().x(), values[i]->getSize().y());
		values[i]->setDefaultZIndex(40);

		float testBot = values[i]->getPosition().y() + values[i]->getSize().y();
		if (testBot > bottom)
			bottom = testBot;
	}

	mDescContainer.setPosition(mDescContainer.getPosition().x(), bottom + mSize.y() * 0.01f);
	mDescContainer.setSize(mDescContainer.getSize().x(), mSize.y() - mDescContainer.getPosition().y());
}

std::vector<TextComponent*> DetailedContainer::getMDLabels()
{
	return { &mLblRating, &mLblReleaseDate, &mLblDeveloper, &mLblPublisher, &mLblGenre, &mLblPlayers, &mLblLastPlayed, &mLblPlayCount };
}

std::vector<GuiComponent*> DetailedContainer::getMDValues()
{
	return { &mRating, &mReleaseDate, &mDeveloper, &mPublisher, &mGenre, &mPlayers, &mLastPlayed, &mPlayCount };
}

// Batocera components that exist (themed)
std::vector<GuiComponent*> DetailedContainer::getBatoceraComponents()
{
	std::vector<GuiComponent*> comps;

	if (mFlag != nullptr)
		comps.push_back(mFlag);

	for (auto& md : mMdImages)
		if (md.component != nullptr)
			comps.push_back(md.component);

	for (auto& md : mMdFlagImages)
		if (md.component != nullptr)
			comps.push_back(md.component);

	for (auto text : { &mLblGameTime, &mGameTime, &mLblFavorite, &mTextFavorite })
		if (text->isVisible())
			comps.push_back(text);

	for (auto extra : mPerGameExtras)
		comps.push_back(extra);

	return comps;
}

std::string DetailedContainer::getMetadata(FileData* file, const std::string& name)
{
	std::string ret = file->getMetadata().get(name);
	if (ret == "unknown")
		return _(ret);

	return ret;
}

// path of a media, "" if the file does not exist
std::string DetailedContainer::getMediaPath(FileData* file, const std::string& key)
{
	std::string path;

	if (key == "image")
		path = file->getImagePath();
	else if (key == "thumbnail")
		path = file->getThumbnailPath();
	else if (key == "marquee")
		path = file->getMarqueePath();
	else
		path = file->getMetadata().get(key);

	if (path.empty() || !Utils::FileSystem::exists(path))
		return "";

	return path;
}

void DetailedContainer::updateBatoceraElements(FileData* file)
{
	for (auto& md : mMdImages)
	{
		if (md.component == nullptr)
			continue;

		std::string image;
		for (auto& key : md.keys)
		{
			image = getMediaPath(file, key);
			if (!image.empty())
				break;
		}

		md.component->setImage(image, false, md.component->getMaxSizeInfo());
	}

	if (mFlag != nullptr)
	{
		if (file->getType() == GAME)
		{
			std::string lang = file->getMetadata().get("lang");
			std::string region = file->getMetadata().get("region");

			if (lang.empty() && region.empty())
			{
				auto info = LangInfo::parse(file->getSourceFileData()->getPath(), file->getSourceFileData()->getSystem());
				lang = info.getLanguageString();
				region = info.region;
			}

			mFlag->setImage(":/flags/" + LangInfo::getFlag(lang, region) + ".png", false, mFlag->getMaxSizeInfo());
		}
		else
			mFlag->setImage(":/folder.svg");
	}

	bool hasManual = !getMediaPath(file, "manual").empty() || !getMediaPath(file, "magazine").empty();
	bool hasMap = !getMediaPath(file, "map").empty();

	for (auto& md : mMdFlagImages)
	{
		if (md.component == nullptr)
			continue;

		bool visible = false;

		if (md.id == "md_favorite") visible = file->getFavorite();
		else if (md.id == "md_notfavorite") visible = !file->getFavorite();
		else if (md.id == "md_hidden") visible = file->getHidden();
		else if (md.id == "md_kidgame") visible = file->getKidGame();
		else if (md.id == "md_notkidgame") visible = !file->getKidGame();
		else if (md.id == "md_manual") visible = hasManual;
		else if (md.id == "md_nomanual") visible = !hasManual;
		else if (md.id == "md_map") visible = hasMap;
		else if (md.id == "md_nomap") visible = !hasMap;
		// no light gun / wheel / trackball / spinner game database on this platform: games are never of those types
		else if (md.id == "md_notgungame" || md.id == "md_notwheelgame" || md.id == "md_nottrackballgame" || md.id == "md_notspinnergame") visible = true;
		// save states, RetroAchievements and netplay are not managed by EmulationStation here: Batocera hides both images then
		else visible = false;

		md.component->setVisible(visible);
	}

	mTextFavorite.setText(file->getFavorite() ? _("YES") : _("NO"));

	if (file->getType() == GAME)
		mGameTime.setValue(Utils::Time::secondsToString(atol(file->getMetadata().get("gametime").c_str())));
	else
		mGameTime.setValue("");

	for (auto extra : mPerGameExtras)
		BindingManager::updateBindings(extra, file);
}

void DetailedContainer::clearPerGameExtras()
{
	for (auto extra : mPerGameExtras)
	{
		removeChild(extra);
		delete extra;
	}

	mPerGameExtras.clear();
}

// <gameextras><path>: <path>/<rom name>.xml, <path>/<crc32>.xml, <path>/<rom name>/theme.xml or <path>/<crc32>/theme.xml
// (or a .xml file) appended to the view's theme for the selected game
void DetailedContainer::loadPerGameExtras(FileData* file)
{
	if (mPerGameExtrasPath.empty() || mTheme == nullptr)
		return;

	std::string rootPath = mPerGameExtrasPath;

	if (mIsPerGameExtrasPathBinding)
	{
		std::string evaluationResult = BindingManager::evaluateBindableExpression(mPerGameExtrasPath, file);
		if (evaluationResult.size())
			rootPath = Utils::FileSystem::getCanonicalPath(evaluationResult);
	}

	std::string stem = Utils::FileSystem::getStem(file->getPath());
	std::string crc = file->getMetadata().get("crc32");

	std::string path;

	if (Utils::String::endsWith(rootPath, ".xml") && Utils::FileSystem::exists(rootPath))
		path = rootPath;
	else
	{
		std::vector<std::string> candidates = { Utils::FileSystem::combine(rootPath, stem + ".xml") };
		if (!crc.empty())
			candidates.push_back(Utils::FileSystem::combine(rootPath, crc + ".xml"));

		candidates.push_back(Utils::FileSystem::combine(rootPath, stem + "/theme.xml"));
		if (!crc.empty())
			candidates.push_back(Utils::FileSystem::combine(rootPath, crc + "/theme.xml"));

		for (auto& candidate : candidates)
		{
			if (Utils::FileSystem::exists(candidate))
			{
				path = candidate;
				break;
			}
		}
	}

	if (path == mCustomThemePath)
		return;

	clearPerGameExtras();

	if (path.empty())
	{
		// back to the view's own theme
		mCustomThemePath.clear();
		mCustomTheme = nullptr;
		applyTheme(mTheme);
		return;
	}

	auto customTheme = mTheme->clone(mParent->getName());
	if (!customTheme->appendFile(path, true))
	{
		mCustomThemePath.clear();
		mCustomTheme = nullptr;
		applyTheme(mTheme);
		return;
	}

	mCustomThemePath = path;
	mCustomTheme = customTheme;
	applyTheme(mCustomTheme);

	mPerGameExtras = ThemeData::makeExtras(mCustomTheme, mParent->getName(), mWindow, false, true);
	for (auto extra : mPerGameExtras)
		addChild(extra);

	mParent->sortChildren();
}

void DetailedContainer::updateControls(FileData* file, bool isClearing, int moveBy, bool restartStoryboards)
{
	if (file != nullptr && !isClearing)
		loadPerGameExtras(file);

	bool fadingOut;
	if (file == nullptr)
	{
		if (mViewType == VideoView)
		{
			mVideo->setVideo("");
			mVideo->setImage("");
		}
		else if (mViewType == GridView)
		{
			if (mVideo != nullptr)
			{
				mVideo->setVideo("");
				mVideo->setImage("");
			}

			if (mImage != nullptr)
				mImage->setImage("");
		}
		else if (mVideo != nullptr)
			mVideo->setVideo("");

		fadingOut = true;
	}
	else
	{
		std::string imagePath = file->getImagePath().empty() ? file->getThumbnailPath() : file->getImagePath();

		// Batocera md_marquee: the wheel when the game has no marquee
		std::string marqueePath = file->getMarqueePath();
		if (mMarquee != nullptr && getMediaPath(file, "marquee").empty())
		{
			std::string wheel = getMediaPath(file, "wheel");
			if (!wheel.empty())
				marqueePath = wheel;
		}

		if (mViewType == VideoView)
		{
			if (!mVideo->setVideo(file->getVideoPath()))
				mVideo->setDefaultVideo();

			std::string snapShot = file->getThumbnailPath();

			auto src = mVideo->getSnapshotSource();
			if (src == MARQUEE && !file->getMarqueePath().empty())
				snapShot = file->getMarqueePath();
			if (src == IMAGE && !file->getImagePath().empty())
				snapShot = file->getImagePath();

			mVideo->setImage(snapShot);

			mMarquee->setImage(marqueePath); // no max size: too slow on pi

			if (mThumbnail != nullptr)
			{
				if (mImage != nullptr)
					mImage->setImage(file->getImagePath(), false, mImage->getMaxSizeInfo());

				mThumbnail->setImage(file->getThumbnailPath(), false, mThumbnail->getMaxSizeInfo());
			}
			else if (mImage != nullptr)
				mImage->setImage(file->getThumbnailPath());
		}
		else
		{
			if (mVideo != nullptr)
			{
				if (!mVideo->setVideo(file->getVideoPath()))
					mVideo->setDefaultVideo();

				std::string snapShot = imagePath;

				auto src = mVideo->getSnapshotSource();
				if (src == MARQUEE && !file->getMarqueePath().empty())
					snapShot = file->getMarqueePath();
				if (src == THUMBNAIL && !file->getThumbnailPath().empty())
					snapShot = file->getThumbnailPath();

				mVideo->setImage(snapShot);
			}

			if (mViewType == DetailedView)
			{
				if (mThumbnail != nullptr)
					mThumbnail->setImage(file->getThumbnailPath());

				if (mImage != nullptr)
					mImage->setImage(imagePath);
			}
			else
			{
				if (mImage != nullptr)
					mImage->setImage(imagePath);

				if (mThumbnail != nullptr)
					mThumbnail->setImage(file->getThumbnailPath());
			}

			if (mMarquee != nullptr)
				mMarquee->setImage(marqueePath, false, mMarquee->getMaxSizeInfo());
		}

		mDescription.setText(mViewType == DetailedView ? getMetadata(file, "desc") : file->getMetadata().get("desc"));
		mDescContainer.reset();

		auto value = [this, file](const std::string& name) { return mViewType == DetailedView ? getMetadata(file, name) : file->getMetadata().get(name); };

		mRating.setValue(value("rating"));
		mReleaseDate.setValue(value("releasedate"));
		mDeveloper.setValue(value("developer"));
		mPublisher.setValue(value("publisher"));
		mGenre.setValue(value("genre"));
		mPlayers.setValue(value("players"));
		mName.setValue(value("name"));

		if (file->getType() == GAME)
		{
			mLastPlayed.setValue(value("lastplayed"));
			mPlayCount.setValue(value("playcount"));
		}

		updateBatoceraElements(file);

		fadingOut = false;
	}

	// We're clearing / populating : don't setup fade animations
	if (isClearing)
		return;

	std::vector<GuiComponent*> comps = getMDValues();

	if (mViewType == VideoView)
	{
		comps.push_back(mMarquee);
		comps.push_back(mVideo);
		comps.push_back(&mDescription);

		if (mImage != nullptr)
			comps.push_back(mImage);

		if (mThumbnail != nullptr)
			comps.push_back(mThumbnail);
	}
	else if (mViewType == GridView)
	{
		if (mVideo != nullptr)
			comps.push_back(mVideo);

		if (mMarquee != nullptr)
			comps.push_back(mMarquee);

		if (mThumbnail != nullptr)
			comps.push_back(mThumbnail);

		if (mImage != nullptr)
			comps.push_back(mImage);

		comps.push_back(&mDescription);
	}
	else
	{
		if (mVideo != nullptr)
			comps.push_back(mVideo);

		if (mImage != nullptr)
			comps.push_back(mImage);

		if (mThumbnail != nullptr)
			comps.push_back(mThumbnail);

		if (mMarquee != nullptr)
			comps.push_back(mMarquee);

		comps.push_back(&mDescription);
	}

	comps.push_back(&mName);
	std::vector<TextComponent*> labels = getMDLabels();
	comps.insert(comps.cend(), labels.cbegin(), labels.cend());

	auto batocera = getBatoceraComponents();
	comps.insert(comps.cend(), batocera.cbegin(), batocera.cend());

	for (auto it = comps.cbegin(); it != comps.cend(); it++)
	{
		GuiComponent* comp = *it;

		// detailed view: a theme activate storyboard drives this component instead of the generic fade
		if (mViewType == DetailedView && ISimpleGameListView::hasActivationStoryboard(comp, true, false))
		{
			if (restartStoryboards)
				ISimpleGameListView::handleStoryboard(comp, true, moveBy, file);
			continue;
		}

		// an animation is playing
		//   then animate if reverse != fadingOut
		// an animation is not playing
		//   then animate if opacity != our target opacity
		if ((comp->isAnimationPlaying(0) && comp->isAnimationReversed(0) != fadingOut) ||
			(!comp->isAnimationPlaying(0) && comp->getOpacity() != (fadingOut ? 0 : 255)))
		{
			auto func = [comp](float t)
			{
				comp->setOpacity((unsigned char)(Math::lerp(0.0f, 1.0f, t) * 255));
			};

			if (mViewType == GridView)
			{
				comp->setAnimation(new LambdaAnimation(func, 150), 0, nullptr, fadingOut);
				continue;
			}

			bool isFadeOut = fadingOut;
			comp->setAnimation(new LambdaAnimation(func, 150), 0, [this, isFadeOut]
			{
				if (isFadeOut)
				{
					if (mVideo != nullptr) mVideo->setImage("");
					if (mImage != nullptr) mImage->setImage("");
					if (mThumbnail != nullptr) mThumbnail->setImage("");
					if (mMarquee != nullptr) mMarquee->setImage("");
				}
			}, fadingOut);
		}
	}
}
