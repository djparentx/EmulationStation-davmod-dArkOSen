#include <string>
#pragma once
#ifndef ES_APP_GUIS_GUI_DATETIME_SCREENSAVER_OPTIONS_H
#define ES_APP_GUIS_GUI_DATETIME_SCREENSAVER_OPTIONS_H

#include "GuiScreensaverOptions.h"

class GuiDatetimeScreensaverOptions : public GuiScreensaverOptions
{
public:
	GuiDatetimeScreensaverOptions(Window* window, const char* title);
	virtual ~GuiDatetimeScreensaverOptions();

private:
	void reopen();
};

#endif // ES_APP_GUIS_GUI_DATETIME_SCREENSAVER_OPTIONS_H