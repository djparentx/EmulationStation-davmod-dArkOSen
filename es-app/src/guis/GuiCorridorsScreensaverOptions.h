#include <string>
#pragma once
#ifndef ES_APP_GUIS_GUI_CORRIDORS_SCREENSAVER_OPTIONS_H
#define ES_APP_GUIS_GUI_CORRIDORS_SCREENSAVER_OPTIONS_H

#include "GuiScreensaverOptions.h"

class GuiCorridorsScreensaverOptions : public GuiScreensaverOptions
{
public:
	GuiCorridorsScreensaverOptions(Window* window, const char* title);
	virtual ~GuiCorridorsScreensaverOptions();
};

#endif // ES_APP_GUIS_GUI_CORRIDORS_SCREENSAVER_OPTIONS_H
