#include <string>
#include <SystemData.h>
#include "guis/GuiCollectionSystemsOptions.h"

#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiSettings.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiTextEditPopup.h"
#include "utils/StringUtil.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Window.h"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdio>
#include "renderers/Renderer.h"

GuiCollectionSystemsOptions::GuiCollectionSystemsOptions(Window* window)
	: GuiSettings(window, _("GAME COLLECTION SETTINGS").c_str())
{
	initializeMenu();
}

void GuiCollectionSystemsOptions::initializeMenu()
{
	auto groupNames = SystemData::getAllGroupNames();
	if (groupNames.size() > 0)
	{
		auto ungroupedSystems = std::make_shared<OptionListComponent<std::string>>(mWindow, _("GROUPED SYSTEMS"), true);
		for (auto groupName : groupNames)
		{
			std::string description;
			for (auto zz : SystemData::getGroupChildSystemNames(groupName))
			{
				if (!description.empty())
					description += ", ";

				description += zz;
			}

			ungroupedSystems->addEx(groupName, description, groupName, !Settings::getInstance()->getBool(groupName + ".ungroup"));
		}

		addWithLabel(_("GROUPED SYSTEMS"), ungroupedSystems);

		addSaveFunc([this, ungroupedSystems, groupNames]
		{
			std::vector<std::string> checkedItems = ungroupedSystems->getSelectedObjects();
			for (auto groupName : groupNames)
			{
				bool isGroupActive = std::find(checkedItems.cbegin(), checkedItems.cend(), groupName) != checkedItems.cend();
				if (Settings::getInstance()->setBool(groupName + ".ungroup", !isGroupActive))
					setVariable("reloadSystems", true);
			}
		});
	}

	// get collections
	addSystemsToMenu();

	// add "Create New Custom Collection from Theme"
	std::vector<std::string> unusedFolders = CollectionSystemManager::get()->getUnusedSystemsFromTheme();
	if (unusedFolders.size() > 0)
	{
		addEntry(_("CREATE NEW CUSTOM COLLECTION FROM THEME").c_str(), true,
			[this, unusedFolders] {
			auto s = new GuiSettings(mWindow, _("SELECT THEME FOLDER").c_str());
			std::shared_ptr< OptionListComponent<std::string> > folderThemes = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SELECT THEME FOLDER"), true);

			// add Custom Systems
			for (auto it = unusedFolders.cbegin(); it != unusedFolders.cend(); it++)
			{
				ComponentListRow row;
				std::string name = *it;

				std::function<void()> createCollectionCall = [name, this, s] {
					createCollection(name);
				};
				row.makeAcceptInputHandler(createCollectionCall);

				auto themeFolder = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(name), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
				row.addElement(themeFolder, true);
				s->addRow(row);
			}
			mWindow->pushGui(s);
		});
	}

	auto createCustomCollection = [this](const std::string& newVal) {
		std::string name = newVal;
		// we need to store the first Gui and remove it, as it'll be deleted by the actual Gui
		Window* window = mWindow;
		GuiComponent* topGui = window->peekGui();
		window->removeGui(topGui);
		createCollection(name);
	};
	addEntry(_("CREATE NEW CUSTOM COLLECTION").c_str(), true, [this, createCustomCollection] {
		if (Settings::getInstance()->getBool("UseOSK")) {
			mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("New Collection Name"), "", createCustomCollection, false));
		}
		else {
			mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("New Collection Name"), "", createCustomCollection, false));
		}
	});

	std::shared_ptr<SwitchComponent> bundleCustomCollections = std::make_shared<SwitchComponent>(mWindow);
	bundleCustomCollections->setState(Settings::getInstance()->getBool("UseCustomCollectionsSystem"));
	addWithLabel(_("GROUP UNTHEMED CUSTOM COLLECTIONS"), bundleCustomCollections);
	addSaveFunc([this, bundleCustomCollections]
	{
		if (Settings::getInstance()->setBool("UseCustomCollectionsSystem", bundleCustomCollections->getState()))
			setVariable("reloadAll", true);
	});

	// SORT COLLECTIONS AND SYSTEMS
	std::string sortMode = Settings::getInstance()->getString("SortSystems");

	auto sortType = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SORT COLLECTIONS AND SYSTEMS"), false);
	sortType->add(_("NO"), "", sortMode.empty());
	sortType->add(_("ALPHABETICALLY"), "alpha", sortMode == "alpha");

	if (SystemData::isManufacturerSupported())
	{
		sortType->add(_("BY MANUFACTURER"), "manufacturer", sortMode == "manufacturer");
		sortType->add(_("BY HARDWARE TYPE"), "hardware", sortMode == "hardware");
		sortType->add(_("BY RELEASE YEAR"), "releaseDate", sortMode == "releaseDate");
	}

	if (!sortType->hasSelection())
		sortType->selectFirstItem();
	
	addWithLabel(_("SORT SYSTEMS"), sortType);
	addSaveFunc([this, sortType]
	{
		if (Settings::getInstance()->setString("SortSystems", sortType->getSelected()))
			setVariable("reloadAll", true);
	});

	std::shared_ptr<SwitchComponent> toggleSystemNameInCollections = std::make_shared<SwitchComponent>(mWindow);
	toggleSystemNameInCollections->setState(Settings::getInstance()->getBool("CollectionShowSystemInfo"));
	addWithLabel(_("SHOW SYSTEM NAME IN COLLECTIONS"), toggleSystemNameInCollections);
	addSaveFunc([this, toggleSystemNameInCollections]
	{
		if (Settings::getInstance()->setBool("CollectionShowSystemInfo", toggleSystemNameInCollections->getState()))
			setVariable("reloadAll", true);
	});




	if (CollectionSystemManager::get()->isEditing())
		addEntry((_("FINISH EDITING COLLECTION") + " : " + Utils::String::toUpper(CollectionSystemManager::get()->getEditingCollection())).c_str(), false, std::bind(&GuiCollectionSystemsOptions::exitEditMode, this));

	addSaveFunc([this]
	{
		std::string newAutoSettings = Utils::String::vectorToCommaString(autoOptionList->getSelectedObjects());
		std::string newCustomSettings = Utils::String::vectorToCommaString(customOptionList->getSelectedObjects());

		bool dirty = Settings::getInstance()->setString("CollectionSystemsAuto", newAutoSettings);
		dirty |= Settings::getInstance()->setString("CollectionSystemsCustom", newCustomSettings);

		if (dirty)
			setVariable("reloadAll", true);
	});

	onFinalize([this]
	{
		if (getVariable("reloadSystems"))
		{
			Window* window = mWindow;
			window->renderLoadingScreen(_("Loading..."));

			ViewController::get()->goToStart();
			delete ViewController::get();
			ViewController::init(window);
			CollectionSystemManager::deinit();
			CollectionSystemManager::init(window);
			SystemData::loadConfig(window);

			GuiComponent* gui;
			while ((gui = window->peekGui()) != NULL)
			{
				window->removeGui(gui);
				if (gui != this)
					delete gui;
			}
			ViewController::get()->reloadAll(nullptr); // Avoid reloading themes a second time
			window->endRenderLoadingScreen();

			window->pushGui(ViewController::get());
		}
		else if (getVariable("reloadAll"))
		{
			Settings::getInstance()->saveFile();

			CollectionSystemManager::get()->loadEnabledListFromSettings();
			CollectionSystemManager::get()->updateSystemsList();
			ViewController::get()->goToStart();
			ViewController::get()->reloadAll(mWindow);
			mWindow->endRenderLoadingScreen();
		}
	});
}

void GuiCollectionSystemsOptions::createCollection(std::string inName)
{
	std::string name = CollectionSystemManager::get()->getValidNewCollectionName(inName);
	SystemData* newSys = CollectionSystemManager::get()->addNewCustomCollection(name);
	customOptionList->add(name, name, true);

	std::string outAuto = Utils::String::vectorToCommaString(autoOptionList->getSelectedObjects());
	std::string outCustom = Utils::String::vectorToCommaString(customOptionList->getSelectedObjects());
	updateSettings(outAuto, outCustom);

	ViewController::get()->goToSystemView(newSys);

	Window* window = mWindow;
	CollectionSystemManager::get()->setEditMode(name);
	while (window->peekGui() && window->peekGui() != ViewController::get())
		delete window->peekGui();

	return;
}

void GuiCollectionSystemsOptions::exitEditMode()
{
	CollectionSystemManager::get()->exitEditMode();
	close();
}

GuiCollectionSystemsOptions::~GuiCollectionSystemsOptions()
{

}

void GuiCollectionSystemsOptions::addSystemsToMenu()
{

	std::map<std::string, CollectionSystemData> &autoSystems = CollectionSystemManager::get()->getAutoCollectionSystems();

	autoOptionList = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SELECT COLLECTIONS"), true);

	bool hasGroup = false;

	// add Auto Systems && preserve order
	for (auto systemDecl : CollectionSystemManager::getSystemDecls())
	{
		auto it = autoSystems.find(systemDecl.name);
		if (it == autoSystems.cend())
			continue;

		if (it->second.decl.displayIfEmpty)
			autoOptionList->add(it->second.decl.longName, it->second.decl.name, it->second.isEnabled);
		else
		{
			if (!it->second.isPopulated)
				CollectionSystemManager::get()->populateAutoCollection(&(it->second));

			if (it->second.system->getRootFolder()->getChildren().size() == 0)
				continue;

			if (!hasGroup)
			{
				autoOptionList->addGroup(_("ARCADE SYSTEMS"));
				hasGroup = true;
			}

			autoOptionList->add(_(it->second.decl.longName.c_str()), it->second.decl.name, it->second.isEnabled);
		}
	}
	addEntry(_("LAST 20 PLAYED GAMES"), true, [this] { openLastPlayedGames(); });
	addWithLabel(_("AUTOMATIC GAME COLLECTIONS"), autoOptionList);

	std::map<std::string, CollectionSystemData> customSystems = CollectionSystemManager::get()->getCustomCollectionSystems();

	customOptionList = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SELECT COLLECTIONS"), true);

	// add Custom Systems
	for (std::map<std::string, CollectionSystemData>::const_iterator it = customSystems.cbegin(); it != customSystems.cend(); it++)
	{
		customOptionList->add(it->second.decl.longName, it->second.decl.name, it->second.isEnabled);
	}
	addWithLabel(_("CUSTOM GAME COLLECTIONS"), customOptionList);
}

void GuiCollectionSystemsOptions::updateSettings(std::string newAutoSettings, std::string newCustomSettings)
{
	bool dirty = Settings::getInstance()->setString("CollectionSystemsAuto", newAutoSettings);
	dirty |= Settings::getInstance()->setString("CollectionSystemsCustom", newCustomSettings);

	if (dirty)
	{
		Settings::getInstance()->saveFile();
		CollectionSystemManager::get()->loadEnabledListFromSettings();
		CollectionSystemManager::get()->updateSystemsList();
		ViewController::get()->goToStart();
		ViewController::get()->reloadAll();
	}
}	

// ---------------------------------------------------------------------------
// Menu "LAST 20 PLAYED GAMES"
// ---------------------------------------------------------------------------

static std::string esFormatPlayTime(int seconds)
{
	if (seconds <= 0)
		return "-";

	int hours = seconds / 3600;
	int minutes = (seconds % 3600) / 60;
	int secs = seconds % 60;

	if (hours > 0)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%dh%02dm%02ds", hours, minutes, secs);
		return std::string(buf);
	}

	if (minutes > 0)
	{
		char buf2[32];
		snprintf(buf2, sizeof(buf2), "%dm%02ds", minutes, secs);
		return std::string(buf2);
	}

	return std::to_string(secs) + "s";
}

static void esScanDir(const std::string& dir, std::vector<std::string>& out, int depth)
{
	if (depth > 4)
		return;

	DIR* d = opendir(dir.c_str());
	if (d == NULL)
		return;

	struct dirent* ent;
	while ((ent = readdir(d)) != NULL)
	{
		std::string name = ent->d_name;
		if (name == "." || name == "..")
			continue;

		std::string full = dir + "/" + name;

		struct stat st;
		if (stat(full.c_str(), &st) != 0)
			continue;

		if (S_ISDIR(st.st_mode))
			esScanDir(full, out, depth + 1);
		else
			out.push_back(full);
	}

	closedir(d);
}

static int esFindLatestSaveSlot(FileData* game)
{
	const std::string esSys = Utils::String::toLower(game->getSystemName());
	if (esSys.compare(0, 4, "mame") == 0 || esSys == "arcade" ||
		esSys == "neogeo" || esSys == "fbneo")
		return -1;

	const std::string statesDir = Utils::FileSystem::getHomePath() + "/.config/retroarch/states";
	if (!Utils::FileSystem::exists(statesDir))
		return -1;

	const std::string stem = Utils::FileSystem::getStem(game->getPath());
	if (stem.empty())
		return -1;

	std::vector<std::string> files;
	esScanDir(statesDir, files, 0);

	int bestManualSlot = -1;
	time_t bestManualTime = 0;
	bool hasAuto = false;

	for (auto& f : files)
	{
		std::string fname = Utils::FileSystem::getFileName(f);

		const std::string prefix = stem + ".state";
		if (fname.compare(0, prefix.length(), prefix) != 0)
			continue;

		std::string suffix = fname.substr(prefix.length());

		int slot;
		if (suffix.empty())
			slot = 0;
		else if (suffix == ".auto")
			slot = -2;
		else if (suffix.length() == 1 && suffix[0] >= '1' && suffix[0] <= '9')
			slot = suffix[0] - '0';
		else
			continue;

		struct stat st;
		if (stat(f.c_str(), &st) != 0)
			continue;

		if (slot == -2)
		{
			hasAuto = true;
			continue;
		}

		if (st.st_mtime > bestManualTime)
		{
			bestManualTime = st.st_mtime;
			bestManualSlot = slot;
		}
	}

	if (bestManualSlot >= 0)
		return bestManualSlot;

	return hasAuto ? -2 : -1;
}

static std::vector<FileData*> esGetLastPlayedGames(size_t maxCount)
{
	std::vector<std::pair<std::string, FileData*>> entries;

	for (auto system : SystemData::sSystemVector)
	{
		if (system->isCollection())
			continue;

		if (system->getRootFolder() == NULL)
			continue;

		for (auto game : system->getRootFolder()->getFilesRecursive(GAME))
		{
			std::string stemLower = Utils::String::toLower(Utils::FileSystem::getStem(game->getPath()));
			std::string nameLower = Utils::String::toLower(game->getName());
			if (stemLower.compare(0, 9, "retroarch") == 0 || nameLower.compare(0, 9, "retroarch") == 0)
				continue;

			std::string lastPlayed = game->getMetadata().get("lastplayed");

			if (lastPlayed.empty() || lastPlayed == "0" || lastPlayed == "not-a-date-time")
				continue;

			entries.push_back(std::make_pair(lastPlayed, game));
		}
	}

	std::sort(entries.begin(), entries.end(),
		[](const std::pair<std::string, FileData*>& a, const std::pair<std::string, FileData*>& b)
		{
			return a.first > b.first;
		});

	std::vector<FileData*> result;
	for (auto& e : entries)
	{
		if (result.size() >= maxCount)
			break;
		result.push_back(e.second);
	}

	return result;
}

void GuiCollectionSystemsOptions::openLastPlayedGames()
{
	auto s = new GuiSettings(mWindow, _("LAST 20 PLAYED GAMES").c_str());

	std::vector<FileData*> games = esGetLastPlayedGames(20);

	if (games.empty())
	{
		s->addEntry(_("NO GAME PLAYED YET"), false, nullptr);
		mWindow->pushGui(s);
		return;
	}

	Window* window = mWindow;

	{
		ComponentListRow header;
		auto hLeft = std::make_shared<TextComponent>(window, _("GAME"),
			ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
		auto hRight = std::make_shared<TextComponent>(window, _("SESSION | TOTAL"),
			ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
		auto hMid = std::make_shared<TextComponent>(window, "  " + _("SYSTEM") + "  |  ",
			ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
		header.addElement(hLeft, true);
		header.addElement(hMid, false);
		header.addElement(hRight, false);
		header.selectable = false;
		s->addRow(header);
	}

	for (auto game : games)
	{
		FileData* src = game->getSourceFileData();

		int lastSession = src->getMetadata().getInt("lastsession");
		int totalTime   = src->getMetadata().getInt("gametime");
		int slot        = esFindLatestSaveSlot(src);

		std::string info = esFormatPlayTime(lastSession) + " | " + esFormatPlayTime(totalTime);
		if (slot != -1)
			info += " *";

		auto infoText = std::make_shared<TextComponent>(window, info,
			ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);

		ComponentListRow row;

		auto nameText = std::make_shared<TextComponent>(window, game->getName(),
			ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);

		auto sysText = std::make_shared<TextComponent>(window, "  " + src->getSystemName() + "  |  ",
			ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);

		row.addElement(nameText, true);
		row.addElement(sysText, false);
		row.addElement(infoText, false);

		row.makeAcceptInputHandler([window, src, slot, s]
		{
			FileData* esGame = src;
			int esSlot = slot >= 0 ? slot : -1;

			window->postToUiThread([esGame, esSlot](Window* w)
			{
				GuiComponent* gui;
				while ((gui = w->peekGui()) != NULL && gui != ViewController::get())
				{
					w->removeGui(gui);
					delete gui;
				}

				ViewController::get()->launch(esGame, Vector3f(Renderer::getScreenWidth() / 2.0f,
					Renderer::getScreenHeight() / 2.0f, 0), esSlot);
			});
		});

		s->addRow(row);
	}

	mWindow->pushGui(s);
}
