#include <string>
#pragma once
#ifndef ES_APP_CORRIDORS_CONFIG_H
#define ES_APP_CORRIDORS_CONFIG_H

// Corridors screensaver settings, stored in ~/.config/corridors.ini
struct CorridorsConfig
{
	int speedTenths = 8;   // 5 - 15  (0.5x - 1.5x)
	int brightness  = 50;  // 0 - 100
	int ambient     = 15;  // 0 - 100
	int panels      = 2;   // 1 - 5
	int flicker     = 3;   // 1 - 5
	int fog         = 1;   // 1 - 5
	int volume      = 3;   // 0 - 5, 0 = off

	static std::string     path();
	static CorridorsConfig load();
	void                   save() const;
	std::string            toArgs() const;
};

#endif // ES_APP_CORRIDORS_CONFIG_H
