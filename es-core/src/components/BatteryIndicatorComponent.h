#include <string>
#pragma once

#ifndef ES_APP_COMPONENTS_BATTERYINDICATOR_COMPONENT_H
#define ES_APP_COMPONENTS_BATTERYINDICATOR_COMPONENT_H

#include "ControllerActivityComponent.h"

class BatteryIndicatorComponent : public ControllerActivityComponent
{
public:
	BatteryIndicatorComponent(Window* window);

public:
	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

protected:
	virtual void init() override;
};

#endif // ES_APP_COMPONENTS_RATING_COMPONENT_H
