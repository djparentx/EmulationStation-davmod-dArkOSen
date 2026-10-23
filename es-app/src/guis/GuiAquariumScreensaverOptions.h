#include <string>
#pragma once
#ifndef ES_APP_GUIS_GUI_AQUARIUM_SCREENSAVER_OPTIONS_H
#define ES_APP_GUIS_GUI_AQUARIUM_SCREENSAVER_OPTIONS_H

#include "GuiScreensaverOptions.h"

class GuiAquariumScreensaverOptions : public GuiScreensaverOptions
{
public:
	GuiAquariumScreensaverOptions(Window* window, const char* title);
	virtual ~GuiAquariumScreensaverOptions();

private:
	void reopen();
};

#endif // ES_APP_GUIS_GUI_AQUARIUM_SCREENSAVER_OPTIONS_H
