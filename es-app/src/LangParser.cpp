#include "LangParser.h"

#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "EsLocale.h"
#include "MameNames.h"
#include "PlatformId.h"
#include "SystemData.h"
#include <algorithm>
#include <set>
#include <vector>

struct LangData
{
	std::vector<std::string> value;
	std::string lang;
	std::string region;
};

// texts between start and end, e.g. "(usa) (en,fr)" -> "usa", "en,fr"
static std::vector<std::string> extractStrings(const std::string& str, const std::string& start, const std::string& end)
{
	std::vector<std::string> ret;

	size_t pos = 0;
	while ((pos = str.find(start, pos)) != std::string::npos)
	{
		size_t endPos = str.find(end, pos + start.size());
		if (endPos == std::string::npos)
			break;

		ret.push_back(str.substr(pos + start.size(), endPos - pos - start.size()));
		pos = endPos + end.size();
	}

	return ret;
}

// system language, e.g. "en_US" -> "en", "us"
static void getSystemLanguage(std::string& lang, std::string& region)
{
	std::string locale = Utils::String::toLower(EsLocale::getLanguage());

	lang = "en";
	region = "";

	auto shortNameDivider = locale.find("_");
	if (shortNameDivider != std::string::npos)
	{
		lang = locale.substr(0, shortNameDivider);
		region = locale.substr(shortNameDivider + 1);
	}
	else if (!locale.empty())
		lang = locale;
}

std::string LangInfo::getLanguageString()
{
	std::string data;

	for (auto line : languages)
	{
		if (!data.empty())
			data += ",";

		data += line;
	}

	return data;
}

void LangInfo::extractLang(std::string val)
{
	static std::vector<LangData> langDatas =
	{
		{ { "usa", "us", "u" }, "en", "us" },

		{ { "europe", "eu", "e", "ue", "euro" }, "", "eu" },

		{ { "w", "wor", "world" }, "en", "wr" },
		{ { "uk", "gb"}, "en", "eu"},
		{ { "es", "spain", "s" }, "es", "eu" },
		{ { "fr", "france", "fre", "french", "f" }, "fr", "eu" },
		{ { "de", "germany", "d" }, "de", "eu" },
		{ { "it", "italy", "i" }, "it", "eu" },
		{ { "nl", "netherlands" }, "nl", "eu" },
		{ { "gr", "greece" }, "gr", "eu" },
		{ { "no" }, "no", "eu" },
		{ { "sw", "sweden", "se" }, "sw", "eu" },
		{ { "pt", "portugal" }, "pt", "eu" },
		{ { "pl", "poland" }, "pl", "eu" },

		{ { "en" }, "en", "" },

		{ { "jp", "japan", "ja", "j" }, "jp", "jp" },

		{ { "br", "brazil" }, "br", "br" },
		{ { "ru", "r" }, "ru", "ru" },
		{ { "kr", "korea", "k" }, "kr", "kr" },
		{ { "cn", "china", "hong", "kong", "ch", "hk", "as", "tw" }, "cn", "cn" },

		{ { "canada", "ca", "c", "fc" }, "fr", "wr" },

		{ { "in", "india" }, "in", "in" },
	};

	static std::set<std::string> hardRegions =
	{
		"usa", "us",
		"europe", "eu",
		"brazil", "br",
		"japan", "jp",
		"kr", "korea"
	};

	std::string systemLang;
	std::string systemRegion;
	getSystemLanguage(systemLang, systemRegion);

	for (auto langData : langDatas)
		if (std::find(langData.value.cbegin(), langData.value.cend(), systemRegion) != langData.value.cend())
			systemRegion = langData.region;

	for (auto s : Utils::String::splitAny(val, "_, "))
	{
		bool clearLang = s.find("t-") != std::string::npos;

		s = Utils::String::replace(s, "_", "");
		s = Utils::String::replace(s, "t+", "");
		s = Utils::String::replace(s, "t-", "");

		for (auto langData : langDatas)
		{
			if (std::find(langData.value.cbegin(), langData.value.cend(), s) == langData.value.cend())
				continue;

			if (!langData.lang.empty())
			{
				if (clearLang)
					languages.clear();

				languages.insert(langData.lang);
			}

			if (!langData.region.empty())
			{
				bool newHardRegion = hardRegions.find(s) != hardRegions.cend();
				if (!mHardRegion)
				{
					region = langData.region;
					mHardRegion = newHardRegion;
				}
				else if (newHardRegion && region != systemRegion)
				{
					if (langData.region == systemRegion || langData.lang == systemLang)
						region = langData.region;
					else if (langData.lang.empty() && langData.region == "eu")
					{
						for (auto ld : langDatas)
							if (ld.region == "eu" && ld.lang == systemLang)
								region = ld.region;
					}
				}
			}
		}
	}
}

LangInfo LangInfo::parse(std::string rom, SystemData* system)
{
	LangInfo info;
	if (rom.empty())
		return info;

	std::string fileName = Utils::String::toLower(Utils::FileSystem::getFileName(rom));

	if (system && (system->hasPlatformId(PlatformIds::ARCADE) || system->hasPlatformId(PlatformIds::NEOGEO)) && fileName.find("j.zip") == std::string::npos)
	{
		std::string realName = Utils::String::toLower(MameNames::getInstance()->getRealName(Utils::FileSystem::getStem(rom)));
		if (!realName.empty() && realName.find("(") != std::string::npos)
			fileName = realName;
	}

	for (auto s : extractStrings(fileName, "(", ")"))
		info.extractLang(s);

	for (auto s : extractStrings(fileName, "[", "]"))
		info.extractLang(s);

	for (auto s : extractStrings(fileName, "_", "_"))
		info.extractLang(s);

	if (system != nullptr)
	{
		static std::string japanDefaults = "pc88|pc98|pcenginecd|pcfx|satellaview|sg1000|sufami|wswan|wswanc|x68000";

		if (info.region.empty())
		{
			if (japanDefaults.find(system->getName()) != std::string::npos)
				info.region = "jp";
			else if (system->hasPlatformId(PlatformIds::ARCADE))
				info.region = fileName.find("j.zip") != std::string::npos ? "jp" : "us";
			else if (system->getName() == "thomson")
				info.region = "eu";
		}

		if (info.languages.size() == 0)
		{
			if (japanDefaults.find(system->getName()) != std::string::npos)
				info.languages.insert("jp");
			else if (system->getName() == "thomson")
				info.languages.insert("fr");
			else if (system->hasPlatformId(PlatformIds::ARCADE) && fileName.find("j.zip") != std::string::npos)
				info.languages.insert("jp");
			else
				info.languages.insert("en");
		}
	}

	return info;
}

std::string LangInfo::getFlag(const std::string lang, const std::string region)
{
	if (lang.empty() && region.empty())
		return "us";

	std::string locale;
	std::string systemRegion;
	getSystemLanguage(locale, systemRegion);

	auto langs = Utils::String::split(lang, ',');
	if (locale != "en" && std::find(langs.cbegin(), langs.cend(), locale) != langs.cend())
		return locale;

	if (langs.size() >= 1 && region.empty() && langs[0] == "en")
		return "us";
	else if (langs.size() == 1 && lang != "en")
		return lang;

	if (region == "eu" && langs.size() == 1 && std::find(langs.cbegin(), langs.cend(), "en") != langs.cend())
		return "uk";

	if (region == "jp" && langs.size() == 1 && lang != "jp")
	{
		if (lang == "en")
			return "us";

		return lang;
	}

	if (!region.empty())
		return region;

	return "eu";
}
