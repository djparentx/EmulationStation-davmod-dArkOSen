#include <string>
#include "views/gamelist/ISimpleGameListView.h"

#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Settings.h"
#include "Sound.h"
#include "SystemData.h"
#include "BindingManager.h"
#include "animations/LambdaAnimation.h"

ISimpleGameListView::ISimpleGameListView(Window* window, FolderData* root) : IGameListView(window, root),
	mHeaderText(window), mHeaderImage(window), mBackground(window)
{
	mHeaderText.setText("Logo Text");
	mHeaderText.setSize(mSize.x(), 0);
	mHeaderText.setPosition(0, 0);
	mHeaderText.setHorizontalAlignment(ALIGN_CENTER);
	mHeaderText.setDefaultZIndex(50);

	mHeaderImage.setResize(0, mSize.y() * 0.185f);
	mHeaderImage.setOrigin(0.5f, 0.0f);
	mHeaderImage.setPosition(mSize.x() / 2, 0);
	mHeaderImage.setDefaultZIndex(50);

	mBackground.setResize(mSize.x(), mSize.y());
	mBackground.setDefaultZIndex(0);

	addChild(&mHeaderText);
	addChild(&mBackground);
}

void ISimpleGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	using namespace ThemeFlags;
	mBackground.applyTheme(theme, getName(), "background", ALL);
	mHeaderImage.applyTheme(theme, getName(), "logo", ALL);
	mHeaderText.applyTheme(theme, getName(), "logoText", ALL);

	// Remove old theme extras
	clearDeactivatingExtras();

	for (auto extra : mThemeExtras)
	{
		removeChild(extra);
		delete extra;
	}
	mThemeExtras.clear();
	mExtrasFile = nullptr;

	// Add new theme extras
	mThemeExtras = ThemeData::makeExtras(theme, getName(), mWindow);
	for (auto extra : mThemeExtras)
	{
		addChild(extra);
	}

	if(mHeaderImage.hasImage())
	{
		removeChild(&mHeaderText);
		addChild(&mHeaderImage);
	}else{
		addChild(&mHeaderText);
		removeChild(&mHeaderImage);
	}
}

void ISimpleGameListView::onFileChanged(FileData* /*file*/, FileChangeType /*change*/)
{
	// we could be tricky here to be efficient;
	// but this shouldn't happen very often so we'll just always repopulate
	FileData* cursor = getCursor();
	if (!cursor->isPlaceHolder()) {
		populateList(cursor->getParent()->getChildrenListToDisplay());
		setCursor(cursor);
	}
	else
	{
		populateList(mRoot->getChildrenListToDisplay());
		setCursor(cursor);
	}
}

bool ISimpleGameListView::input(InputConfig* config, Input input)
{
	bool hideSystemView = Settings::getInstance()->getBool("HideSystemView");

	if (input.value != 0)
	{
		if (config->isMappedTo(BUTTON_OK, input))
		{
			// Don't launch game if transition is still running
			if (ViewController::get()->isAnimationPlaying(0))
				return true;
			
			FileData* cursor = getCursor();
			FolderData* folder = NULL;

			if (mCursorStack.size() && cursor->getType() == PLACEHOLDER && cursor->getPath() == "..")
			{
				auto top = mCursorStack.top();
				mCursorStack.pop();

				FolderData* folder = top->getParent();
				if (folder == nullptr)
					folder = getCursor()->getSystem()->getParentGroupSystem()->getRootFolder();

				populateList(folder->getChildrenListToDisplay());
				setCursor(top);
				Sound::getFromTheme(getTheme(), getName(), "back")->play();
			}
			else
			{
				if (cursor->getType() == FOLDER)
					folder = (FolderData*)cursor;

				if (cursor->getType() == GAME)
				{
					Sound::getFromTheme(getTheme(), getName(), "launch")->play();
					launch(cursor);
				}
				else {
					// it's a folder
					if (folder != nullptr && folder->getChildren().size() > 0)
					{
						mCursorStack.push(cursor);
						populateList(folder->getChildrenListToDisplay());
						FileData* cursor = getCursor();
						setCursor(cursor);
					}
				}
			}
			return true;
		}
		else if(config->isMappedTo(BUTTON_BACK, input))
		{
			if (mCursorStack.size())
			{
				auto top = mCursorStack.top();
				mCursorStack.pop();

				FolderData* folder = top->getParent();
				if (folder == nullptr && getCursor()->getSystem()->getParentGroupSystem() != nullptr)
					folder = getCursor()->getSystem()->getParentGroupSystem()->getRootFolder();

				if (folder == nullptr)
					return true;

				populateList(folder->getChildrenListToDisplay());
				setCursor(top);				
				Sound::getFromTheme(getTheme(), getName(), "back")->play();
			} 
			else if (!hideSystemView)
			{
				onFocusLost();
				SystemData* systemToView = getCursor()->getSystem();

				if (systemToView->isGroupChildSystem())
					systemToView = systemToView->getParentGroupSystem();
				else if (systemToView->isCollection())
					systemToView = CollectionSystemManager::get()->getSystemToView(systemToView);

				ViewController::get()->goToSystemView(systemToView);
			}

			return true;
		}
		else if (config->isMappedLike(getQuickSystemSelectRightButton(), input) || config->isMappedLike("rightshoulder", input))
		{
			if(Settings::getInstance()->getBool("QuickSystemSelect"))
			{
				onFocusLost();
				ViewController::get()->goToNextGameList();
				return true;
			}
		}
		else if (config->isMappedLike(getQuickSystemSelectLeftButton(), input) || config->isMappedLike("leftshoulder", input))
		{
			if(Settings::getInstance()->getBool("QuickSystemSelect"))
			{
				onFocusLost();
				ViewController::get()->goToPrevGameList();
				return true;
			}
		}
		else if (config->isMappedTo("x", input))
		{
			if (mRoot->getSystem()->isGameSystem())
			{
				// go to random system game
				FileData* randomGame = getCursor()->getSystem()->getRandomGame();
				if (randomGame)
				{
					setCursor(randomGame);
				}
				return true;
			}
		}
		else if (config->isMappedTo("y", input) && !UIModeController::getInstance()->isUIModeKid())
		{
			if (mRoot->getSystem()->isGameSystem() || mRoot->getSystem()->isGroupSystem())
				if (CollectionSystemManager::get()->toggleGameInCollection(getCursor()))
					return true;
		}
	}

	return IGameListView::input(config, input);
}

bool ISimpleGameListView::moveToLetter(const std::string& letter)
{
	auto files = getFileDataEntries();
	long letterIndex = -1;

	for (int i = files.size() - 1; i >= 0; i--)
	{
		const std::string& name = files.at(i)->getName();
		if (name.empty())
			continue;

		std::string checkLetter;

		if (Utils::String::isKorean(name.c_str()))
		{
			const char* koreanLetter = nullptr;

			std::string nameChar = name.substr(0, 3);
			if (!Utils::String::splitHangulSyllable(nameChar.c_str(), &koreanLetter) || !koreanLetter)
				continue; // Korean supports chosung search only.
			else
				checkLetter = std::string(koreanLetter, 3);
		}
		else
		{
			checkLetter = std::string(1, toupper(name[0]));
		}

		if (letterIndex >= 0 && checkLetter != letter)
			break;

		if (checkLetter == letter)
			letterIndex = i;
	}

	if (letterIndex >= 0) {
		setCursor(files.at(letterIndex));
		return true;
	}

	return false;
}

void ISimpleGameListView::moveToLetterByOffset(int offset)
{
	std::vector<std::string> letters = getEntriesLetters();
	if (letters.empty())
		return;

	FileData* game = getCursor();
	if (game == nullptr)
		return;

	const std::string& namecurrent = game->getName();
	std::string curLetter;

	if (Utils::String::isKorean(namecurrent.c_str()))
	{
		const char* koreanLetter = nullptr;

		std::string nameChar = namecurrent.substr(0, 3);
		if (!Utils::String::splitHangulSyllable(nameChar.c_str(), &koreanLetter) || !koreanLetter)
			curLetter = std::string(letters.at(0)); // Korean supports chosung search only. set default.
		else
			curLetter = std::string(koreanLetter, 3);
	}
	else
	{
		curLetter = std::string(1, toupper(namecurrent[0]));
	}

	auto it = std::find(letters.begin(), letters.end(), curLetter);
	if (it != letters.end())
	{
		int index = std::distance(letters.begin(), it) + offset;
		if (index < 0)
			index = letters.size() - 1;
		else if (index >= letters.size())
			index = 0;

		moveToLetter(letters.at(index));
	}
}

void ISimpleGameListView::moveToNextLetter()
{
	moveToLetterByOffset(1);
}

void ISimpleGameListView::moveToPreviousLetter()
{
	moveToLetterByOffset(-1);
}

std::vector<std::string> ISimpleGameListView::getEntriesLetters()
{
	std::set<std::string> setOfLetters;

	for (auto file : getFileDataEntries())
	{
		if (file->getType() != GAME)
			continue;

		const std::string& name = file->getName();

		if (Utils::String::isKorean(name.c_str()))
		{
			const char* koreanLetter = nullptr;

			std::string nameChar = name.substr(0, 3);
			if (!Utils::String::splitHangulSyllable(nameChar.c_str(), &koreanLetter) || !koreanLetter)
				continue;

			setOfLetters.insert(std::string(koreanLetter, 3));
		}
		else
		{
			setOfLetters.insert(std::string(1, toupper(name[0])));
		}
	}

	std::vector<std::string> letters(setOfLetters.begin(), setOfLetters.end());
	return letters;
}

void ISimpleGameListView::updateThemeExtrasBindings(FileData* file)
{
	if (file == nullptr)
		return;

	for (auto extra : mThemeExtras)
		BindingManager::updateBindings(extra, file);
}

ISimpleGameListView::~ISimpleGameListView()
{
	clearDeactivatingExtras();
}

void ISimpleGameListView::clearDeactivatingExtras()
{
	for (auto& set : mDeactivatingExtras)
	{
		for (auto extra : set)
		{
			removeChild(extra);
			delete extra;
		}
	}

	mDeactivatingExtras.clear();
}

void ISimpleGameListView::update(int deltaTime)
{
	IGameListView::update(deltaTime);

	// AmberELEC DetailedContainerHost::update: drop outgoing extras once their deactivate is done
	for (auto it = mDeactivatingExtras.begin(); it != mDeactivatingExtras.end(); )
	{
		bool running = false;

		for (auto extra : *it)
		{
			if (extra->isAnimationPlaying(0) ||
				extra->isStoryboardRunning("deactivate") || extra->isStoryboardRunning("deactivateNext") || extra->isStoryboardRunning("deactivatePrev"))
			{
				running = true;
				break;
			}
		}

		if (running)
		{
			++it;
			continue;
		}

		for (auto extra : *it)
		{
			removeChild(extra);
			delete extra;
		}

		it = mDeactivatingExtras.erase(it);
	}
}

void ISimpleGameListView::onShow()
{
	// replay "open" / default storyboards when the view is shown again
	mExtrasFile = nullptr;
	mLastCursorIndex = -1;

	IGameListView::onShow();
}

int ISimpleGameListView::getMoveBy(int cursorIndex)
{
	int moveBy = (mLastCursorIndex < 0) ? 0 : cursorIndex - mLastCursorIndex;
	mLastCursorIndex = cursorIndex;
	return moveBy;
}

bool ISimpleGameListView::hasActivationStoryboard(GuiComponent* comp, bool checkActivate, bool checkDeactivate)
{
	if (checkDeactivate && (comp->hasStoryboard("deactivate") || comp->hasStoryboard("deactivateNext") || comp->hasStoryboard("deactivatePrev")))
		return true;

	if (checkActivate && (comp->hasStoryboard("activate") || comp->hasStoryboard("activateNext") || comp->hasStoryboard("activatePrev")))
		return true;

	for (unsigned int i = 0; i < comp->getChildCount(); i++)
		if (hasActivationStoryboard(comp->getChild(i), checkActivate, checkDeactivate))
			return true;

	return false;
}

void ISimpleGameListView::handleStoryboard(GuiComponent* comp, bool activate, int moveBy, FileData* file)
{
	auto enabledFn = [file](const std::string& expr) { return BindingManager::evaluateBoolean(expr, file); };

	const std::string prefix = activate ? "activate" : "deactivate";

	if (moveBy > 0 && comp->hasStoryboard(prefix + "Next"))
		comp->startStoryboard(prefix + "Next", enabledFn, activate);
	else if (moveBy < 0 && comp->hasStoryboard(prefix + "Prev"))
		comp->startStoryboard(prefix + "Prev", enabledFn, activate);
	else if (moveBy != 0 && comp->hasStoryboard(prefix))
		comp->startStoryboard(prefix, enabledFn, activate);
	else if (activate && moveBy == 0 && comp->hasStoryboard("open"))
		comp->startStoryboard("open", enabledFn);
	else if (activate && moveBy == 0 && comp->hasStoryboard("") && !comp->isStoryboardSelected(""))
		comp->startStoryboard("", enabledFn);
	else if (activate && moveBy == 0 && comp->hasStoryboard("activate"))
		comp->startStoryboard("activate", enabledFn);

	for (unsigned int i = 0; i < comp->getChildCount(); i++)
		handleStoryboard(comp->getChild(i), activate, moveBy, file);
}

void ISimpleGameListView::updateThemeExtras(FileData* file, int moveBy)
{
	if (file == nullptr)
		return;

	// same game (key release, metadata refresh): only refresh the bindings
	if (file == mExtrasFile)
	{
		updateThemeExtrasBindings(file);
		return;
	}

	FileData* previous = mExtrasFile;
	mExtrasFile = file;

	bool handoff = (previous != nullptr && moveBy != 0 && getTheme() != nullptr);
	if (handoff)
	{
		handoff = false;
		for (auto extra : mThemeExtras)
		{
			if (hasActivationStoryboard(extra))
			{
				handoff = true;
				break;
			}
		}
	}

	if (!handoff)
	{
		updateThemeExtrasBindings(file);

		for (auto extra : mThemeExtras)
			handleStoryboard(extra, true, moveBy, file);

		return;
	}

	// outgoing extras keep the previous game's content and play their deactivate storyboard
	for (auto extra : mThemeExtras)
	{
		if (hasActivationStoryboard(extra, false, true))
			handleStoryboard(extra, false, moveBy, previous);
		else if (!hasActivationStoryboard(extra))
		{
			extra->stopStoryboard();
			extra->setVisible(false);
		}
		else
		{
			// activate only: fade out over 500ms (AmberELEC)
			extra->enableStoryboardProperty("opacity", false);

			const unsigned char initialOpacity = extra->getOpacity();
			auto func = [extra, initialOpacity](float t) { extra->setOpacity((unsigned char)(Math::lerp(0.0f, 1.0f, t) * initialOpacity)); };
			extra->setAnimation(new LambdaAnimation(func, 500), 0, [extra] { extra->setVisible(false); }, true);
		}
	}

	mDeactivatingExtras.push_back(mThemeExtras);

	// AmberELEC keeps at most 4 outgoing sets
	while (mDeactivatingExtras.size() > 4)
	{
		for (auto extra : mDeactivatingExtras.front())
		{
			removeChild(extra);
			delete extra;
		}

		mDeactivatingExtras.erase(mDeactivatingExtras.begin());
	}

	// incoming extras: a fresh set bound to the new game
	mThemeExtras = ThemeData::makeExtras(getTheme(), getName(), mWindow);
	for (auto extra : mThemeExtras)
		addChild(extra);

	sortChildren();
	updateThemeExtrasBindings(file);

	for (auto extra : mThemeExtras)
	{
		extra->onShow();
		handleStoryboard(extra, true, moveBy, file);
	}
}











