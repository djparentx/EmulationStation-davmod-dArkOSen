#include <string>
#include "guis/GuiGeneralScreensaverOptions.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSlideshowScreensaverOptions.h"
#include "guis/GuiDatetimeScreensaverOptions.h"
#include "guis/GuiVideoScreensaverOptions.h"
#include "Settings.h"
#include <algorithm>
#include <fstream>
#include <strings.h>

#define AQUARIUM_INI "/home/ark/.config/aquarium.ini"

// ini value, menu name
static const std::pair<const char*, const char*> AQUARIUM_FISH[] = {
	{ "Yellow_Tang", "Yellow Tang" },
	{ "Golden_Butterflyfish", "Golden Butterflyfish" },
	{ "Powder_Blue_Tang", "Powder Blue Tang" },
	{ "Flame_Angel", "Flame Angel" },
	{ "Queen_Angel", "Queen Angel" },
	{ "Royal_Gramma", "Royal Gramma" },
	{ "Copperband_Butterflyfish", "Copperband Butterflyfish" },
	{ "Addis_Butterflyfish", "Addis Butterflyfish" },
	{ "French_Angel", "French Angel" },
	{ "Wimplefish", "Wimplefish" },
	{ "Forceps_Fish", "Forceps Fish" },
	{ "Regal_Tang", "Regal Tang" },
	{ "Clown_Triggerfish", "Clown Triggerfish" },
	{ "Percula_Clownfish", "Percula Clownfish" },
	{ "Purple_Tang", "Purple Tang" },
	{ "Picasso_Triggerfish", "Picasso Triggerfish" },
	{ "Regal_Angel", "Regal Angel" },
	{ "Blue_Damselfish", "Blue Damselfish" },
	{ "Koran_Angel", "Koran Angel" },
	{ "Three_Stripe_Damselfish", "Three Stripe Damselfish" },
	{ "LionFish", "Lionfish" },
	{ "EmperorAngel", "Emperor Angel" },
	{ "BlueRingAngel", "Blue Ring Angel" },
	{ "RedfinBfly", "Redfin Butterflyfish" },
	{ "RockBeauty", "Rock Beauty" },
	{ "Squarespot", "Squarespot" },
};

// Reads aquarium.ini, changes only the keys that are set, writes it back
// (keeps the aquarium's own keys such as StarFishPos)
class AquariumIni
{
public:
	AquariumIni()
	{
		std::ifstream f(AQUARIUM_INI);
		std::string line;
		while (std::getline(f, line))
			mLines.push_back(line);
	}

	std::string get(const std::string& key, const std::string& def) const
	{
		int i = find(key);
		return i < 0 ? def : trim(mLines[i].substr(mLines[i].find('=') + 1));
	}

	int getInt(const std::string& key, int def) const
	{
		std::string v = get(key, "");
		return v.empty() ? def : atoi(v.c_str());
	}

	bool getBool(const std::string& key, bool def) const
	{
		std::string v = get(key, "");
		if (v.empty())
			return def;
		return v == "1" || strcasecmp(v.c_str(), "true") == 0 || strcasecmp(v.c_str(), "on") == 0 || strcasecmp(v.c_str(), "yes") == 0;
	}

	void set(const std::string& key, const std::string& value)
	{
		int i = find(key);
		if (i < 0)
			mLines.push_back(key + "=" + value);
		else
			mLines[i] = key + "=" + value;
	}

	void save() const
	{
		std::ofstream f(AQUARIUM_INI, std::ios::trunc);
		for (auto& l : mLines)
			f << l << "\n";
	}

private:
	static std::string trim(const std::string& s)
	{
		size_t a = s.find_first_not_of(" \t\r\n");
		size_t b = s.find_last_not_of(" \t\r\n");
		return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
	}

	int find(const std::string& key) const
	{
		for (size_t i = 0; i < mLines.size(); i++)
		{
			std::string s = trim(mLines[i]);
			size_t eq = s.find('=');
			if (s.empty() || s[0] == '#' || s[0] == ';' || eq == std::string::npos)
				continue;
			if (strcasecmp(trim(s.substr(0, eq)).c_str(), key.c_str()) == 0)
				return (int)i;
		}
		return -1;
	}

	std::vector<std::string> mLines;
};

GuiGeneralScreensaverOptions::GuiGeneralScreensaverOptions(Window* window, std::string title) : GuiScreensaverOptions(window, title)
{
	auto theme = ThemeData::getMenuTheme();

	// screensaver time
	auto screensaver_time = std::make_shared<SliderComponent>(mWindow, 0.f, 120.0f, 1.f, "m");
	screensaver_time->setValue((float)(Settings::getInstance()->getInt("ScreenSaverTime") / (1000 * 60)));
	addWithLabel(_("SCREENSAVER AFTER"), screensaver_time);
	addSaveFunc([screensaver_time] {
	    Settings::getInstance()->setInt("ScreenSaverTime", (int)Math::round(screensaver_time->getValue()) * (1000 * 60));
	    PowerSaver::updateTimeouts();
	});

	// screensaver behavior
	auto screensaver_behavior = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SCREENSAVER BEHAVIOR"), false);
	std::vector<std::string> screensavers;
	screensavers.push_back("dim");
	screensavers.push_back("black");
	screensavers.push_back("random video");
	screensavers.push_back("slideshow");
	screensavers.push_back("datetime");
	screensavers.push_back("aquarium");
	for(auto it = screensavers.cbegin(); it != screensavers.cend(); it++)
		screensaver_behavior->add(_(it->c_str()), *it, Settings::getInstance()->getString("ScreenSaverBehavior") == *it);
	addWithLabel(_("SCREENSAVER BEHAVIOR"), screensaver_behavior);
	addSaveFunc([this, screensaver_behavior] {
		if (Settings::getInstance()->getString("ScreenSaverBehavior") != "random video"
			&& screensaver_behavior->getSelected() == "random video") {
			// if before it wasn't risky but now there's a risk of problems, show warning
			mWindow->pushGui(new GuiMsgBox(mWindow,
				_("THE 'RANDOM VIDEO' SCREENSAVER SHOWS VIDEOS FROM YOUR GAMELIST.") + "\n" + _("IF YOU DON'T HAVE VIDEOS, OR IF NONE OF THEM CAN BE PLAYED AFTER A FEW ATTEMPTS, IT WILL DEFAULT TO 'BLACK'.") + "\n" + _("MORE OPTIONS IN THE 'UI SETTINGS' -> 'VIDEO SCREENSAVER SETTINGS' MENU."),
				_("OK"), [] { return; }));
		}
		Settings::getInstance()->setString("ScreenSaverBehavior", screensaver_behavior->getSelected());
		PowerSaver::updateTimeouts();
	});
	// switching to or from aquarium rebuilds the menu to show/hide its options
	screensaver_behavior->setSelectedChangedCallback([this](const std::string& value)
	{
		if (value == "aquarium" || Settings::getInstance()->getString("ScreenSaverBehavior") == "aquarium")
			reopen();
	});

	// AQUARIUM options
	if (Settings::getInstance()->getString("ScreenSaverBehavior") == "aquarium")
		addAquariumOptions();

	ComponentListRow row;

	// show filtered menu
	row.elements.clear();
	row.addElement(std::make_shared<TextComponent>(mWindow, _("VIDEO SCREENSAVER SETTINGS"), theme->Text.font, theme->Text.color), true);
	row.addElement(makeArrow(mWindow), false);
	row.makeAcceptInputHandler(std::bind(&GuiGeneralScreensaverOptions::openVideoScreensaverOptions, this));
	addRow(row);

	row.elements.clear();
	row.addElement(std::make_shared<TextComponent>(mWindow, _("SLIDESHOW SCREENSAVER SETTINGS"), theme->Text.font, theme->Text.color), true);
	row.addElement(makeArrow(mWindow), false);
	row.makeAcceptInputHandler(std::bind(&GuiGeneralScreensaverOptions::openSlideshowScreensaverOptions, this));
	addRow(row);

	row.elements.clear();
	row.addElement(std::make_shared<TextComponent>(mWindow, _("DATE TIME SCREENSAVER SETTINGS"), theme->Text.font, theme->Text.color), true);
	row.addElement(makeArrow(mWindow), false);
	row.makeAcceptInputHandler(std::bind(&GuiGeneralScreensaverOptions::openDateTimeScreensaverOptions, this));
	addRow(row);

	// Allow ScreenSaver Controls - ScreenSaverControls
	auto ss_controls = std::make_shared<SwitchComponent>(mWindow);
	ss_controls->setState(Settings::getInstance()->getBool("ScreenSaverControls"));
	addWithLabel(_("SCREENSAVER CONTROLS"), ss_controls);
	addSaveFunc([ss_controls] { Settings::getInstance()->setBool("ScreenSaverControls", ss_controls->getState()); });
}

GuiGeneralScreensaverOptions::~GuiGeneralScreensaverOptions()
{
}

void GuiGeneralScreensaverOptions::openVideoScreensaverOptions() {
	mWindow->pushGui(new GuiVideoScreensaverOptions(mWindow, _("VIDEO SCREENSAVER").c_str()));
}

void GuiGeneralScreensaverOptions::openSlideshowScreensaverOptions() {
    mWindow->pushGui(new GuiSlideshowScreensaverOptions(mWindow, _("SLIDESHOW SCREENSAVER").c_str()));
}

// Aquarium settings, written straight into the aquarium's own ini
void GuiGeneralScreensaverOptions::addAquariumOptions()
{
	auto ini = std::make_shared<AquariumIni>();

	int fishCount = std::max(1, std::min(8, ini->getInt("FishCount", 7)));
	bool allRandom = ini->getBool("AllRandom", false);
	bool autoLights = ini->getBool("AutoLights", true);

	// FISH COUNT (changing it rebuilds the menu)
	auto fish_count = std::make_shared< OptionListComponent<std::string> >(mWindow, _("FISH COUNT"), false);
	for (int i = 1; i <= 8; i++)
		fish_count->add(std::to_string(i), std::to_string(i), i == fishCount);
	addWithLabel(_("FISH COUNT"), fish_count);
	addSaveFunc([ini, fish_count] { ini->set("FishCount", fish_count->getSelected()); });
	fish_count->setSelectedChangedCallback([this](const std::string&) { reopen(); });

	// ALL RANDOM (hides the fish pickers; changing it rebuilds the menu)
	auto all_random = std::make_shared<SwitchComponent>(mWindow);
	all_random->setState(allRandom);
	addWithLabel(_("ALL RANDOM"), all_random);
	addSaveFunc([ini, all_random] { ini->set("AllRandom", all_random->getState() ? "1" : "0"); });
	all_random->setOnChangedCallback([this] { reopen(); });

	// FISH 1..FISH COUNT
	if (!allRandom)
	{
		for (int n = 1; n <= fishCount; n++)
		{
			std::string key = "Fish" + std::to_string(n);
			std::string cur = ini->get(key, "random");
			std::string sel = "random";
			if (strcasecmp(cur.c_str(), "off") == 0)
				sel = "off";
			for (auto& f : AQUARIUM_FISH)
				if (strcasecmp(cur.c_str(), f.first) == 0)
					sel = f.first;

			std::string label = _("FISH") + " " + std::to_string(n);
			auto fish = std::make_shared< OptionListComponent<std::string> >(mWindow, label, false);
			fish->add(_("RANDOM"), "random", sel == "random");
			fish->add(_("OFF"), "off", sel == "off");
			for (auto& f : AQUARIUM_FISH)
				fish->add(f.second, f.first, sel == f.first);
			addWithLabel(label, fish);
			addSaveFunc([ini, fish, key] { ini->set(key, fish->getSelected()); });
		}
	}

	// SOUND
	auto sound = std::make_shared<SwitchComponent>(mWindow);
	sound->setState(ini->getBool("Sound", true));
	addWithLabel(_("SOUND"), sound);
	addSaveFunc([ini, sound] { ini->set("Sound", sound->getState() ? "1" : "0"); });

	// BUBBLE COLUMN
	auto bubbles = std::make_shared<SwitchComponent>(mWindow);
	bubbles->setState(ini->getBool("BubbleColumn", true));
	addWithLabel(_("BUBBLE COLUMN"), bubbles);
	addSaveFunc([ini, bubbles] { ini->set("BubbleColumn", bubbles->getState() ? "1" : "0"); });

	// AUTO LIGHTS (hides the light sliders; changing it rebuilds the menu)
	auto auto_lights = std::make_shared<SwitchComponent>(mWindow);
	auto_lights->setState(autoLights);
	addWithLabel(_("AUTO LIGHTS"), auto_lights);
	addSaveFunc([ini, auto_lights] { ini->set("AutoLights", auto_lights->getState() ? "1" : "0"); });
	auto_lights->setOnChangedCallback([this] { reopen(); });

	// FOREGROUND / BACKGROUND LIGHTS
	if (!autoLights)
	{
		auto fg = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		fg->setValue((float)std::max(0, std::min(100, ini->getInt("ForegroundLights", 100))));
		addWithLabel(_("FOREGROUND LIGHTS"), fg);
		addSaveFunc([ini, fg] { ini->set("ForegroundLights", std::to_string((int)Math::round(fg->getValue()))); });

		auto bg = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		bg->setValue((float)std::max(0, std::min(100, ini->getInt("BackgroundLights", 100))));
		addWithLabel(_("BACKGROUND LIGHTS"), bg);
		addSaveFunc([ini, bg] { ini->set("BackgroundLights", std::to_string((int)Math::round(bg->getValue()))); });
	}

	// STARFISH ON GLASS
	auto starfish = std::make_shared<SwitchComponent>(mWindow);
	starfish->setState(ini->getBool("StarfishAllowedGlass", true));
	addWithLabel(_("STARFISH ON GLASS"), starfish);
	addSaveFunc([ini, starfish] { ini->set("StarfishAllowedGlass", starfish->getState() ? "1" : "0"); });

	// write the ini after all aquarium values are set
	addSaveFunc([ini] { ini->save(); });
}

void GuiGeneralScreensaverOptions::reopen()
{
	Window* pw = mWindow;
	delete this;
	pw->pushGui(new GuiGeneralScreensaverOptions(pw, _("SCREENSAVER SETTINGS")));
}