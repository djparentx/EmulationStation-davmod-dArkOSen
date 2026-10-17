#pragma once
#ifndef ES_APP_LANG_PARSER_H
#define ES_APP_LANG_PARSER_H

#include <string>
#include <unordered_set>

class SystemData;

// Batocera: languages / region from a rom file name ("(USA)", "(Europe) (En,Fr,De)", ...) - used by the theme md_flag element
class LangInfo
{
public:
	LangInfo() { mHardRegion = false; }

	static LangInfo parse(std::string rom, SystemData* system);
	static std::string getFlag(const std::string lang, const std::string region);

	std::string region;
	std::unordered_set<std::string> languages;

	std::string getLanguageString();
	bool empty() { return region.empty() && languages.size() == 0; }

private:
	void extractLang(std::string val);

	bool mHardRegion;
};

#endif // ES_APP_LANG_PARSER_H
