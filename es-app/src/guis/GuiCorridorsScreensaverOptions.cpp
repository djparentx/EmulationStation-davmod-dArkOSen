#include <string>
#include "guis/GuiCorridorsScreensaverOptions.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "math/Misc.h"
#include "CorridorsConfig.h"
#include "Window.h"
#include <memory>

// 1..5 list, optional OFF (= 0) entry first
static std::shared_ptr< OptionListComponent<std::string> > makeLevelList(Window* window, const std::string& title, int cur, bool withOff)
{
	auto list = std::make_shared< OptionListComponent<std::string> >(window, title, false);
	if (withOff)
		list->add(_("OFF"), "0", cur == 0);
	for (int i = 1; i <= 5; i++)
		list->add(std::to_string(i), std::to_string(i), cur == i);
	return list;
}

GuiCorridorsScreensaverOptions::GuiCorridorsScreensaverOptions(Window* window, const char* title) : GuiScreensaverOptions(window, title)
{
	auto cfg = std::make_shared<CorridorsConfig>(CorridorsConfig::load());

	// SPEED
	auto speed = std::make_shared<SliderComponent>(mWindow, 0.5f, 1.5f, 0.1f, "x", 1);
	speed->setValue((float)cfg->speedTenths / 10.0f);
	addWithLabel(_("SPEED"), speed);
	addSaveFunc([cfg, speed] { cfg->speedTenths = (int)Math::round(speed->getValue() * 10.0f); });

	// BRIGHTNESS
	auto brightness = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
	brightness->setValue((float)cfg->brightness);
	addWithLabel(_("BRIGHTNESS"), brightness);
	addSaveFunc([cfg, brightness] { cfg->brightness = (int)Math::round(brightness->getValue()); });

	// AMBIENT
	auto ambient = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
	ambient->setValue((float)cfg->ambient);
	addWithLabel(_("AMBIENT"), ambient);
	addSaveFunc([cfg, ambient] { cfg->ambient = (int)Math::round(ambient->getValue()); });

	// PANELS
	auto panels = makeLevelList(mWindow, _("PANELS"), cfg->panels, false);
	addWithLabel(_("PANELS"), panels);
	addSaveFunc([cfg, panels] { cfg->panels = atoi(panels->getSelected().c_str()); });

	// FLICKER
	auto flicker = makeLevelList(mWindow, _("FLICKER"), cfg->flicker, false);
	addWithLabel(_("FLICKER"), flicker);
	addSaveFunc([cfg, flicker] { cfg->flicker = atoi(flicker->getSelected().c_str()); });

	// FOG
	auto fog = makeLevelList(mWindow, _("FOG"), cfg->fog, false);
	addWithLabel(_("FOG"), fog);
	addSaveFunc([cfg, fog] { cfg->fog = atoi(fog->getSelected().c_str()); });

	// VOLUME
	auto volume = makeLevelList(mWindow, _("VOLUME"), cfg->volume, true);
	addWithLabel(_("VOLUME"), volume);
	addSaveFunc([cfg, volume] { cfg->volume = atoi(volume->getSelected().c_str()); });

	// write the ini after all values are set
	addSaveFunc([cfg] { cfg->save(); });
}

GuiCorridorsScreensaverOptions::~GuiCorridorsScreensaverOptions()
{
}
