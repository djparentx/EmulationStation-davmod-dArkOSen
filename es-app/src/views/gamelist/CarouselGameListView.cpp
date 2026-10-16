#include "views/gamelist/CarouselGameListView.h"

#include "components/VideoComponent.h"
#include "utils/FileSystemUtil.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EsLocale.h"
#include "FileData.h"
#include "Settings.h"
#include "SystemData.h"

CarouselGameListView::CarouselGameListView(Window* window, FolderData* root) :
	ISimpleGameListView(window, root), mList(window)
{
	mList.setSize(mSize.x(), mSize.y() * 0.8f);
	mList.setPosition(0, mSize.y() * 0.2f);
	mList.setDefaultZIndex(20);
	mList.setCursorChangedCallback([&](const CursorState& /*state*/) { updateInfoPanel(); });
	addChild(&mList);

	mDetails = new DetailedContainer(this, &mList, window, DetailedContainer::DetailedView);

	populateList(mRoot->getChildrenListToDisplay());
}

CarouselGameListView::~CarouselGameListView()
{
	delete mDetails;
}

void CarouselGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	ISimpleGameListView::onThemeChanged(theme);

	mList.applyTheme(theme, getName(), "gamecarousel", ThemeFlags::ALL);
	mDetails->onThemeChanged(theme);

	sortChildren();

	mStoryboardFile = nullptr; // extras were rebuilt - start their storyboards
	updateInfoPanel();
}

void CarouselGameListView::onShow()
{
	ISimpleGameListView::onShow();
	mStoryboardFile = nullptr; // replay storyboards when the view is shown again
	updateInfoPanel();
}

void CarouselGameListView::updateInfoPanel()
{
	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? nullptr : mList.getSelected();
	const int moveBy = (file != nullptr) ? getMoveBy(mList.getCursorIndex()) : 0;

	// only replay activate storyboards when the selected game actually changes
	const bool restartStoryboards = (file != nullptr && file != mStoryboardFile);
	mStoryboardFile = file;

	if (file != nullptr)
		updateThemeExtras(file, moveBy);

	// We're clearing / populating : don't setup fade animations
	bool isClearing = (file == nullptr && mList.getObjects().size() == 0 && mList.getCursorIndex() == 0 && mList.getScrollingVelocity() == 0);

	mDetails->updateControls(file, isClearing, moveBy, restartStoryboards);
}

void CarouselGameListView::onFileChanged(FileData* file, FileChangeType change)
{
	if (change == FILE_METADATA_CHANGED)
	{
		// might switch to a detailed view
		ViewController::get()->reloadGameListView(this);
		return;
	}

	ISimpleGameListView::onFileChanged(file, change);
}

void CarouselGameListView::populateList(const std::vector<FileData*>& files)
{
	mList.clearEntries();

	std::string systemName = mRoot->getSystem()->getFullName();
	mHeaderText.setText(systemName);

	std::string systemShortName = mRoot->getSystem()->getName();

	bool favoritesFirst = Settings::getInstance()->getBool("FavoritesFirst");
	bool showFavoriteIcon = (systemName != "favorites" && systemShortName != "recent");
	if (!showFavoriteIcon)
		favoritesFirst = false;

	if (files.size() > 0)
	{
		if (mCursorStack.size())
		{
			FileData* placeholder = new FileData(PLACEHOLDER, "..", this->mRoot->getSystem());
			mList.add(". .", placeholder);
		}

		if (favoritesFirst)
		{
			for (auto file : files)
				if (file->getFavorite())
					mList.add(file->getName(), file);
		}

		for (auto file : files)
		{
			if (favoritesFirst && file->getFavorite())
				continue;

			mList.add(file->getName(), file);
		}
	}
	else
		addPlaceholder();
}

FileData* CarouselGameListView::getCursor()
{
	if (mList.size() == 0)
		return nullptr;

	return mList.getSelected();
}

void CarouselGameListView::setCursor(FileData* cursor)
{
	if (!mList.setCursor(cursor) && (!cursor->isPlaceHolder()))
	{
		auto children = mRoot->getChildrenListToDisplay();

		auto gameIter = std::find(children.cbegin(), children.cend(), cursor);
		if (gameIter == children.cend())
		{
			children = cursor->getParent()->getChildrenListToDisplay();

			// update our cursor stack in case our cursor just got set to some folder we weren't in before
			if (mCursorStack.empty() || mCursorStack.top() != cursor->getParent())
			{
				std::stack<FileData*> tmp;
				FileData* ptr = cursor->getParent();
				while (ptr && ptr != mRoot)
				{
					tmp.push(ptr);
					ptr = ptr->getParent();
				}

				// flip the stack and put it in mCursorStack
				mCursorStack = std::stack<FileData*>();
				while (!tmp.empty())
				{
					mCursorStack.push(tmp.top());
					tmp.pop();
				}
			}
		}

		populateList(children);
		mList.setCursor(cursor);
	}
}

void CarouselGameListView::addPlaceholder()
{
	// empty list - add a placeholder
	FileData* placeholder = new FileData(PLACEHOLDER, "<" + _("No Entries Found") + ">", mRoot->getSystem());
	mList.add(placeholder->getName(), placeholder);
}

std::string CarouselGameListView::getQuickSystemSelectRightButton()
{
	if (mList.isHorizontalCarousel())
		return "righttrigger";

	return "right";
}

std::string CarouselGameListView::getQuickSystemSelectLeftButton()
{
	if (mList.isHorizontalCarousel())
		return "lefttrigger";

	return "left";
}

void CarouselGameListView::launch(FileData* game)
{
	Vector3f target(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0);

	VideoComponent* video = mDetails->getVideo();
	ImageComponent* image = mDetails->getImage();

	if (video != nullptr)
		target = Vector3f(video->getCenter().x(), video->getCenter().y(), 0);
	else if (image != nullptr && image->hasImage())
		target = Vector3f(image->getCenter().x(), image->getCenter().y(), 0);

	ViewController::get()->launch(game, target);
}

void CarouselGameListView::remove(FileData* game, bool deleteFile)
{
	if (deleteFile)
		Utils::FileSystem::removeFile(game->getPath());  // actually delete the file on the filesystem

	FolderData* parent = game->getParent();
	if (getCursor() == game)                     // Select next element in list, or prev if none
	{
		std::vector<FileData*> siblings = parent->getChildrenListToDisplay();
		auto gameIter = std::find(siblings.cbegin(), siblings.cend(), game);
		unsigned int gamePos = (int)std::distance(siblings.cbegin(), gameIter);
		if (gameIter != siblings.cend())
		{
			if ((gamePos + 1) < siblings.size())
				setCursor(siblings.at(gamePos + 1));
			else if (gamePos > 1)
				setCursor(siblings.at(gamePos - 1));
		}
	}

	mList.remove(game);
	if (mList.size() == 0)
		addPlaceholder();

	delete game;                                 // remove before repopulating (removes from parent)
	onFileChanged(parent, FILE_REMOVED);           // update the view, with game removed
}

std::vector<HelpPrompt> CarouselGameListView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;

	const bool horizontal = mList.isHorizontalCarousel();

	if (Settings::getInstance()->getBool("QuickSystemSelect"))
		prompts.push_back(HelpPrompt(horizontal ? "lr" : "left/right", _("SYSTEM")));

	prompts.push_back(HelpPrompt(horizontal ? "left/right" : "up/down", _("CHOOSE")));
	prompts.push_back(HelpPrompt(BUTTON_OK, _("LAUNCH")));
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));

	if (!UIModeController::getInstance()->isUIModeKid())
		prompts.push_back(HelpPrompt("select", _("OPTIONS")));

	if (mRoot->getSystem()->isGameSystem())
		prompts.push_back(HelpPrompt("x", _("RANDOM")));

	if (mRoot->getSystem()->isGameSystem() && !UIModeController::getInstance()->isUIModeKid())
	{
		std::string prompt = CollectionSystemManager::get()->getEditingCollection();
		if (prompt == "Favorites")
			prompt = _("FAVORITE");

		prompts.push_back(HelpPrompt("y", prompt));
	}

	return prompts;
}

std::vector<FileData*> CarouselGameListView::getFileDataEntries()
{
	return mList.getObjects();
}
