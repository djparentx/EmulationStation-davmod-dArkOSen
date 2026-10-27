#include "CorridorsConfig.h"

#include "utils/FileSystemUtil.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <strings.h>

static std::string trim(const std::string& s)
{
	size_t a = s.find_first_not_of(" \t\r\n");
	size_t b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

static int clampi(int v, int lo, int hi)
{
	return std::max(lo, std::min(hi, v));
}

// "0.8" -> 8 (locale independent)
static int parseTenths(const std::string& s, int def)
{
	int whole = 0, frac = 0;
	bool any = false;
	size_t i = 0;

	while (i < s.size() && isdigit((unsigned char)s[i]))
	{
		whole = whole * 10 + (s[i] - '0');
		any = true;
		i++;
	}
	if (i < s.size() && s[i] == '.')
	{
		i++;
		if (i < s.size() && isdigit((unsigned char)s[i]))
		{
			frac = s[i] - '0';
			any = true;
		}
	}
	return any ? whole * 10 + frac : def;
}

static std::string tenthsToString(int t)
{
	return std::to_string(t / 10) + "." + std::to_string(t % 10);
}

std::string CorridorsConfig::path()
{
	return Utils::FileSystem::getHomePath() + "/.config/corridors.ini";
}

CorridorsConfig CorridorsConfig::load()
{
	CorridorsConfig c;
	std::ifstream f(path());
	std::string line;

	while (std::getline(f, line))
	{
		std::string s = trim(line);
		size_t eq = s.find('=');
		if (s.empty() || s[0] == '#' || s[0] == ';' || eq == std::string::npos)
			continue;

		std::string key = trim(s.substr(0, eq));
		std::string val = trim(s.substr(eq + 1));
		if (val.empty())
			continue;

		if      (strcasecmp(key.c_str(), "Speed") == 0)      c.speedTenths = parseTenths(val, c.speedTenths);
		else if (strcasecmp(key.c_str(), "Brightness") == 0) c.brightness  = atoi(val.c_str());
		else if (strcasecmp(key.c_str(), "Ambient") == 0)    c.ambient     = atoi(val.c_str());
		else if (strcasecmp(key.c_str(), "Panels") == 0)     c.panels      = atoi(val.c_str());
		else if (strcasecmp(key.c_str(), "Flicker") == 0)    c.flicker     = atoi(val.c_str());
		else if (strcasecmp(key.c_str(), "Fog") == 0)        c.fog         = atoi(val.c_str());
		else if (strcasecmp(key.c_str(), "Volume") == 0)     c.volume      = atoi(val.c_str());
	}

	c.speedTenths = clampi(c.speedTenths, 5, 15);
	c.brightness  = clampi(c.brightness, 0, 100);
	c.ambient     = clampi(c.ambient, 0, 100);
	c.panels      = clampi(c.panels, 1, 5);
	c.flicker     = clampi(c.flicker, 1, 5);
	c.fog         = clampi(c.fog, 1, 5);
	c.volume      = clampi(c.volume, 0, 5);
	return c;
}

void CorridorsConfig::save() const
{
	std::string dir = Utils::FileSystem::getParent(path());
	if (!Utils::FileSystem::exists(dir))
		Utils::FileSystem::createDirectory(dir);

	std::ofstream f(path(), std::ios::trunc);
	f << "# 3D Corridors screensaver settings (written by EmulationStation)\n";
	f << "Speed="      << tenthsToString(speedTenths) << "\n";
	f << "Brightness=" << brightness << "\n";
	f << "Ambient="    << ambient    << "\n";
	f << "Panels="     << panels     << "\n";
	f << "Flicker="    << flicker    << "\n";
	f << "Fog="        << fog        << "\n";
	f << "Volume="     << volume     << "\n";
}

std::string CorridorsConfig::toArgs() const
{
	return "--speed "      + tenthsToString(speedTenths) +
	       " --brightness " + std::to_string(brightness) +
	       " --ambient "    + std::to_string(ambient) +
	       " --panels "     + std::to_string(panels) +
	       " --flicker "    + std::to_string(flicker) +
	       " --fog "        + std::to_string(fog) +
	       " --volume "     + std::to_string(volume);
}
