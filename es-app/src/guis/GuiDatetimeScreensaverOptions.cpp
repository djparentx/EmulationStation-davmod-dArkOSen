#include <string>
#include "guis/GuiDatetimeScreensaverOptions.h"

#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "SystemScreenSaver.h"
#include "Settings.h"
#include "Window.h"

GuiDatetimeScreensaverOptions::GuiDatetimeScreensaverOptions(Window* window, const char* title) : GuiScreensaverOptions(window, title)
{
	auto settings = Settings::getInstance();
	typedef std::vector<std::pair<std::string, std::string>> OptionPairs;
	std::string clockType = settings->getString("DateTimeScreenSaverClockType");
	std::string bgMode = settings->getString("DateTimeScreenSaverBackgroundMode");
	std::string clockStyle = settings->getString("DateTimeScreenSaverClockStyle");

	// CLOCK TYPE (changing it rebuilds the menu)
	auto clock_type = std::make_shared< OptionListComponent<std::string> >(mWindow, _("CLOCK TYPE"), false);
	clock_type->addRange(OptionPairs{ { "Number", "Number" }, { "Clock", "Clock" } }, clockType);
	addWithLabel(_("CLOCK TYPE"), clock_type);
	addSaveFunc([clock_type] { Settings::getInstance()->setString("DateTimeScreenSaverClockType", clock_type->getSelected()); });
	clock_type->setSelectedChangedCallback([this](const std::string& value)
	{
		Settings::getInstance()->setString("DateTimeScreenSaverClockType", value);
		reopen();
	});

	// CLOCK COLOR (Number only; colours time and date)
	if (clockType == "Number")
	{
		auto clock_color = std::make_shared< OptionListComponent<std::string> >(mWindow, _("CLOCK COLOR"), false);
		clock_color->addRange(OptionPairs{ { "Neon Green", "Neon Green" }, { "Neon Blue", "Neon Blue" }, { "Neon Yellow", "Neon Yellow" }, { "Neon Red", "Neon Red" }, { "White", "White" } },
			settings->getString("DateTimeScreenSaverClockColor"));
		addWithLabel(_("CLOCK COLOR"), clock_color);
		addSaveFunc([clock_color] { Settings::getInstance()->setString("DateTimeScreenSaverClockColor", clock_color->getSelected()); });

		// CLOCK FONT (Number only)
		auto clock_font = std::make_shared< OptionListComponent<std::string> >(mWindow, _("CLOCK FONT"), false);
		clock_font->addRange(OptionPairs{ { "Digital", "Digital" }, { "Exo", "Exo" } }, settings->getString("DateTimeScreenSaverClockFont"));
		addWithLabel(_("CLOCK FONT"), clock_font);
		addSaveFunc([clock_font] { Settings::getInstance()->setString("DateTimeScreenSaverClockFont", clock_font->getSelected()); });
	}

	// CLOCK STYLE (analog only)
	if (clockType == "Clock")
	{
		auto clock_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("CLOCK STYLE"), false);
		clock_style->addRange(OptionPairs{ { "Style 1", "style1" }, { "Style 2", "style2" }, { "Style 3", "style3" }, { "Flip 1", "flip1" } }, clockStyle);
		addWithLabel(_("CLOCK STYLE"), clock_style);
		addSaveFunc([clock_style] { Settings::getInstance()->setString("DateTimeScreenSaverClockStyle", clock_style->getSelected()); });
		clock_style->setSelectedChangedCallback([this](const std::string& value)
		{
			Settings::getInstance()->setString("DateTimeScreenSaverClockStyle", value);
			reopen();
		});
	}

	// BACKGROUND MODE (changing it rebuilds the menu)
	auto bg_mode = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BACKGROUND MODE"), false);
	bg_mode->addRange(OptionPairs{ { "Color", "Color" }, { "Image", "Image" } }, bgMode);
	addWithLabel(_("BACKGROUND MODE"), bg_mode);
	addSaveFunc([bg_mode] { Settings::getInstance()->setString("DateTimeScreenSaverBackgroundMode", bg_mode->getSelected()); });
	bg_mode->setSelectedChangedCallback([this](const std::string& value)
	{
		Settings::getInstance()->setString("DateTimeScreenSaverBackgroundMode", value);
		reopen();
	});

	// BACKGROUND COLOR
	if (bgMode == "Color")
	{
		auto bg_color = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BACKGROUND COLOR"), false);
		bg_color->addRange(OptionPairs{ { "Night", "Night" }, { "Bistre", "Bistre" }, { "Garnet", "Garnet" }, { "Beaver", "Beaver" }, { "Black Olive", "Black Olive" } },
			settings->getString("DateTimeScreenSaverBackgroundColor"));
		addWithLabel(_("BACKGROUND COLOR"), bg_color);
		addSaveFunc([bg_color] { Settings::getInstance()->setString("DateTimeScreenSaverBackgroundColor", bg_color->getSelected()); });
	}

	// BACKGROUND IMAGE (files in /roms/screensaverbgs)
	if (bgMode == "Image")
	{
		std::vector<std::pair<std::string, std::string>> images;
		images.push_back({ _("None"), "None" });
		for (auto bg : DateTimeScreenSaver::getBackgrounds())
			images.push_back({ bg.first, bg.first });

		auto bg_image = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BACKGROUND IMAGE"), false);
		bg_image->addRange(images, settings->getString("DateTimeScreenSaverBackgroundPath"));
		addWithLabel(_("BACKGROUND IMAGE"), bg_image);
		addSaveFunc([bg_image] { Settings::getInstance()->setString("DateTimeScreenSaverBackgroundPath", bg_image->getSelected()); });
	}

	// SHOW DATE
	auto show_date = std::make_shared<SwitchComponent>(mWindow);
	show_date->setState(settings->getBool("DateTimeScreenSaverShowDate"));
	addWithLabel(_("SHOW DATE"), show_date);
	addSaveFunc([show_date] { Settings::getInstance()->setBool("DateTimeScreenSaverShowDate", show_date->getState()); });

	// SHOW SECONDS (not used by flip clock)
	if (!(clockType == "Clock" && clockStyle == "flip1"))
	{
		auto show_seconds = std::make_shared<SwitchComponent>(mWindow);
		show_seconds->setState(settings->getBool("DateTimeScreenSaverShowSeconds"));
		addWithLabel(_("SHOW SECONDS"), show_seconds);
		addSaveFunc([show_seconds] { Settings::getInstance()->setBool("DateTimeScreenSaverShowSeconds", show_seconds->getState()); });
	}show_seconds->getState()); });
}

GuiDatetimeScreensaverOptions::~GuiDatetimeScreensaverOptions()
{
}

void GuiDatetimeScreensaverOptions::reopen()
{
	Window* pw = mWindow;
	delete this;
	pw->pushGui(new GuiDatetimeScreensaverOptions(pw, _("DATE & TIME SCREENSAVER").c_str()));
}