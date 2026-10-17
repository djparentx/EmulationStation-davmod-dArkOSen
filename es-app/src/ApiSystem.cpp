#include <string>
#include "ApiSystem.h"
#include "DisplayPanelControl.h"
#include "HttpReq.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <thread>
#include <codecvt> 
#include <locale> 
#include "Log.h"
#include "Window.h"
#include "components/AsyncNotificationComponent.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unistd.h> 
#include <ctime>

ApiSystem* ApiSystem::instance = nullptr;

ApiSystem *ApiSystem::getInstance() 
{
	if (ApiSystem::instance == nullptr)
#if WIN32
		ApiSystem::instance = new Win32ApiSystem();
#else
		ApiSystem::instance = new ApiSystem();
#endif

	return ApiSystem::instance;
}

UpdateState::State ApiSystem::state = UpdateState::State::NO_UPDATE;

class ThreadedUpdater
{
public:
	ThreadedUpdater(Window* window) : mWindow(window)
	{
		ApiSystem::state = UpdateState::State::UPDATER_RUNNING;

		mWndNotification = new AsyncNotificationComponent(window, false);
		mWndNotification->updateTitle(_U("\uF019 ") + _("EMULATIONSTATION"));

		mWindow->registerNotificationComponent(mWndNotification);
		mHandle = new std::thread(&ThreadedUpdater::threadUpdate, this);
	}

	~ThreadedUpdater()
	{
		mWindow->unRegisterNotificationComponent(mWndNotification);
		delete mWndNotification;
	}

	void threadUpdate()
	{
		std::pair<std::string, int> updateStatus = ApiSystem::updateSystem([this](const std::string info)
		{
			auto pos = info.find(">>>");
			if (pos != std::string::npos)
			{
				std::string percent(info.substr(pos));
				percent = Utils::String::replace(percent, ">", "");
				percent = Utils::String::replace(percent, "%", "");
				percent = Utils::String::replace(percent, " ", "");

				int value = atoi(percent.c_str());

				std::string text(info.substr(0, pos));
				text = Utils::String::trim(text);

				mWndNotification->updatePercent(value);
				mWndNotification->updateText(text);
			}
			else
			{
				mWndNotification->updatePercent(-1);
				mWndNotification->updateText(info);
			}
		});

		if (updateStatus.second == 0)
		{
			ApiSystem::state = UpdateState::State::UPDATE_READY;

			mWndNotification->updateTitle(_U("\uF019 ") + _("UPDATE IS READY"));
			mWndNotification->updateText(_("RESTART EMULATIONSTATION TO APPLY"));

			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::hours(12));
		}
		else
		{
			ApiSystem::state = UpdateState::State::NO_UPDATE;

			std::string error = _("AN ERROR OCCURRED") + std::string(": ") + updateStatus.first;
			mWindow->displayNotificationMessage(error);
		}

		delete this;
	}

private:
	std::thread*				mHandle;
	AsyncNotificationComponent* mWndNotification;
	Window*						mWindow;
};

void ApiSystem::startUpdate(Window* c)
{
#if WIN32
	new ThreadedUpdater(c);
#endif
}

std::string ApiSystem::checkUpdateVersion()
{
#if WIN32
	std::string localVersion;
	std::string localVersionFile = Utils::FileSystem::getExePath() + "/version.info";
	if (Utils::FileSystem::exists(localVersionFile))
	{
		localVersion = Utils::FileSystem::readAllText(localVersionFile);
		localVersion = Utils::String::replace(Utils::String::replace(localVersion, "\r", ""), "\n", "");
	}
	
	HttpReq httpreq("https://github.com/fabricecaruso/EmulationStation/releases/download/continuous-master/version.info");	
	if (httpreq.wait())
	{
		std::string serverVersion = httpreq.getContent();
		serverVersion = Utils::String::replace(Utils::String::replace(serverVersion, "\r", ""), "\n", "");
		if (!serverVersion.empty() && serverVersion != localVersion)
			return serverVersion;
	}
#endif

	return "";
}

#if WIN32
#include <ShlDisp.h>
#include <comutil.h> // #include for _bstr_t
#pragma comment(lib, "shell32.lib")
#pragma comment (lib, "comsuppw.lib" ) // link with "comsuppw.lib" (or debug version: "comsuppwd.lib")

bool unzipFile(const std::string fileName, const std::string dest)
{
	bool	ret = false;

	HRESULT          hResult;
	IShellDispatch*	 pISD;
	Folder*			 pFromZip = nullptr;
	VARIANT          vDir, vFile, vOpt;

	OleInitialize(NULL);
	CoInitialize(NULL);

	hResult = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void **)&pISD);

	if (SUCCEEDED(hResult))
	{
		VariantInit(&vDir);
		vDir.vt = VT_BSTR;

		int zipDirLen = (lstrlenA(fileName.c_str()) + 1) * sizeof(WCHAR);
		BSTR bstrZip = SysAllocStringByteLen(NULL, zipDirLen);
		MultiByteToWideChar(CP_ACP, 0, fileName.c_str(), -1, bstrZip, zipDirLen);
		vDir.bstrVal = bstrZip;

		hResult = pISD->NameSpace(vDir, &pFromZip);

		if (hResult == S_OK && pFromZip != nullptr)
		{
			if (!Utils::FileSystem::exists(dest))
				Utils::FileSystem::createDirectory(dest);

			Folder *pToFolder = NULL;

			VariantInit(&vFile);
			vFile.vt = VT_BSTR;

			int fnLen = (lstrlenA(dest.c_str()) + 1) * sizeof(WCHAR);
			BSTR bstrFolder = SysAllocStringByteLen(NULL, fnLen);
			MultiByteToWideChar(CP_ACP, 0, dest.c_str(), -1, bstrFolder, fnLen);
			vFile.bstrVal = bstrFolder;

			hResult = pISD->NameSpace(vFile, &pToFolder);
			if (hResult == S_OK && pToFolder)
			{
				FolderItems *fi = NULL;
				pFromZip->Items(&fi);

				VariantInit(&vOpt);
				vOpt.vt = VT_I4;
				vOpt.lVal = FOF_NO_UI; //4; // Do not display a progress dialog box

				VARIANT newV;
				VariantInit(&newV);
				newV.vt = VT_DISPATCH;
				newV.pdispVal = fi;
				hResult = pToFolder->CopyHere(newV, vOpt);
				if (hResult == S_OK)
					ret = true;

				pFromZip->Release();
				pToFolder->Release();
			}
		}
		pISD->Release();
	}

	CoUninitialize();
	return ret;
}

bool downloadFile(const std::string url, const std::string fileName, const std::string label, const std::function<void(const std::string)>& func)
{
	if (func != nullptr)
		func("Downloading " + label);

	HttpReq httpreq(url, fileName);
	while (httpreq.status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (func != nullptr)
			func(std::string("Downloading " + label + " >>> " + std::to_string(httpreq.getPercent()) + " %"));

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (httpreq.status() != HttpReq::REQ_SUCCESS)
		return false;

	return true;
}


void deleteDirectoryFiles(const std::string path)
{
	auto files = Utils::FileSystem::getDirContent(path, true, true);
	std::reverse(std::begin(files), std::end(files));
	for (auto file : files)
	{
		if (Utils::FileSystem::isDirectory(file))
			::RemoveDirectoryA(Utils::FileSystem::getPreferredPath(file).c_str());
		else
			Utils::FileSystem::removeFile(file);
	}
}
#endif

std::pair<std::string, int> ApiSystem::updateSystem(const std::function<void(const std::string)>& func)
{
#if WIN32
	std::string url = "https://github.com/fabricecaruso/EmulationStation/releases/download/continuous-master/EmulationStation-Win32-no-deps.zip";

	std::string fileName = Utils::FileSystem::getFileName(url);
	std::string path = Utils::FileSystem::getHomePath() + "/.emulationstation/update";

	if (!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);
	else
		deleteDirectoryFiles(path);

	std::string zipFile = path + "/" + fileName;

	if (downloadFile(url, zipFile, "update", func))
	{
		if (func != nullptr)
			func(std::string("Extracting update"));

		unzipFile(Utils::FileSystem::getPreferredPath(zipFile), Utils::FileSystem::getPreferredPath(path));
		Utils::FileSystem::removeFile(zipFile);

		auto files = Utils::FileSystem::getDirContent(path, true, true);
		for (auto file : files)
		{
			
			std::string relative = Utils::FileSystem::createRelativePath(file, path, false);
			if (Utils::String::startsWith(relative, "./"))
				relative = relative.substr(2);

			std::string localPath = Utils::FileSystem::getExePath() + "/" + relative;

			if (Utils::FileSystem::isDirectory(file))
			{
				if (!Utils::FileSystem::exists(localPath))
					Utils::FileSystem::createDirectory(localPath);
			}
			else
			{
				if (Utils::FileSystem::exists(localPath))
				{
					Utils::FileSystem::removeFile(localPath + ".old");
					rename(localPath.c_str(), (localPath + ".old").c_str());
				}

				if (Utils::FileSystem::copyFile(file, localPath))
				{
					Utils::FileSystem::removeFile(localPath + ".old");
					Utils::FileSystem::removeFile(file);
				}
			}
		}

		deleteDirectoryFiles(path);

		return std::pair<std::string, int>("done.", 0);
	}
#endif

	return std::pair<std::string, int>("error.", 1);
}

std::vector<ThemeDownloadInfo> ApiSystem::getThemesList()
{
	LOG(LogDebug) << "ApiSystem::getThemesList";

	std::vector<ThemeDownloadInfo> res;

	HttpReq httpreq("https://batocera.org/upgrades/themes.txt");
	if (httpreq.wait())
	{
		auto lines = Utils::String::split(httpreq.getContent(), '\n');
		for (auto line : lines)
		{
			auto parts = Utils::String::splitAny(line, " \t");
			if (parts.size() > 1)
			{
				auto themeName = parts[0];

				std::string themeUrl = parts[1];
				std::string themeFolder = Utils::FileSystem::getFileName(themeUrl);

				bool themeExists = false;

				std::vector<std::string> paths{
					Utils::FileSystem::getHomePath() + "/.emulationstation/themes",
					"/etc/emulationstation/themes",
					"/userdata/themes"
				};

				for (auto path : paths)
				{
					themeExists = Utils::FileSystem::isDirectory(path + "/" + themeName) ||
						Utils::FileSystem::isDirectory(path + "/" + themeFolder) ||
						Utils::FileSystem::isDirectory(path + "/" + themeFolder + "-master");
					
					if (themeExists)
						break;
				}

				ThemeDownloadInfo info;
				info.installed = themeExists;
				info.name = themeName;
				info.url = themeUrl;

				res.push_back(info);
			}
		}
	}

	return res;
}

bool downloadGitRepository(const std::string url, const std::string fileName, const std::string label, const std::function<void(const std::string)>& func)
{
	if (func != nullptr)
		func(_("Downloading") + " " + label);

	long downloadSize = 0;

	std::string statUrl = Utils::String::replace(url, "https://github.com/", "https://api.github.com/repos/");
	if (statUrl != url)
	{
		HttpReq statreq(statUrl);
		if (statreq.wait())
		{
			std::string content = statreq.getContent();
			auto pos = content.find("\"size\": ");
			if (pos != std::string::npos)
			{
				auto end = content.find(",", pos);
				if (end != std::string::npos)
					downloadSize = atoi(content.substr(pos + 8, end - pos - 8).c_str()) * 1024;
			}
		}
	}

	HttpReq httpreq(url + "/archive/master.zip", fileName);

	int curPos = -1;
	while (httpreq.status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (downloadSize > 0)
		{
			double pos = httpreq.getPosition();
			if (pos > 0 && curPos != pos)
			{
				if (func != nullptr)
				{
					std::string pc = std::to_string((int)(pos * 100.0 / downloadSize));
					func(std::string(_("Downloading") + " " + label + " >>> " + pc + " %"));
				}

				curPos = pos;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (httpreq.status() != HttpReq::REQ_SUCCESS)
		return false;

	return true;
}

std::pair<std::string, int> ApiSystem::installTheme(std::string themeName, const std::function<void(const std::string)>& func)
{
#if WIN32
	for (auto theme : getThemesList())
	{		
		if (theme.name != themeName)
			continue;

		std::string themeFileName = Utils::FileSystem::getFileName(theme.url);
		std::string zipFile = Utils::FileSystem::getHomePath() + "/.emulationstation/themes/" + themeFileName + ".zip";
		zipFile = Utils::String::replace(zipFile, "/", "\\");

		if (downloadGitRepository(theme.url, zipFile, themeName, func))
		{
			if (func != nullptr)
				func(_("Extracting") + " " + themeName);

			if (!unzipFile(zipFile, Utils::String::replace(Utils::FileSystem::getHomePath() + "/.emulationstation/themes", "/", "\\")))
				return std::pair<std::string, int>(std::string("An error OCCURRED while extracting"), 1);

			std::string folderName = Utils::FileSystem::getHomePath() + "/.emulationstation/themes/" + themeFileName + "-master";
			if (Utils::FileSystem::isDirectory(folderName))
			{
				std::string finalfolderName = Utils::FileSystem::getParent(folderName) + "/" + themeName;

				if (Utils::FileSystem::isDirectory(finalfolderName))
					deleteDirectoryFiles(finalfolderName);

				rename(folderName.c_str(), finalfolderName.c_str());

				Utils::FileSystem::removeFile(zipFile);

				return std::pair<std::string, int>(std::string("OK"), 0);
			}			

			return std::pair<std::string, int>(std::string("Invalid extraction folder"), 1);
		}
				
		return std::pair<std::string, int>(std::string("An error occurred while downloading"), 1);
	}
#endif

	return std::pair<std::string, int>(std::string("Theme not found"), 1);
}

const char* DBACKLIGHT_BRIGHTNESS_NAME = "/sys/class/backlight/backlight/brightness";
const char* DBACKLIGHT_BRIGHTNESS_MAX_NAME = "/sys/class/backlight/backlight/max_brightness";
#define BACKLIGHT_BUFFER_SIZE 127

int ApiSystem::getBrightnessLevel()
{
	LOG(LogInfo) << "ApiSystem::getBrightnessLevel()";

	return DisplayPanelControl::getInstance()->getBrightnessLevel();
}

bool ApiSystem::getBrighness(int& value)
{
#if WIN32
	return false;
#else
	value = 0;

	int fd;
	int max = 100;	
	char buffer[BACKLIGHT_BUFFER_SIZE + 1];
	ssize_t count;

	fd = open(DBACKLIGHT_BRIGHTNESS_MAX_NAME, O_RDONLY);
	if (fd < 0)
		return false;

	memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

	count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
	if (count > 0)
		max = atoi(buffer);

	close(fd);

	if (max == 0) 
		return 0;

	fd = open(DBACKLIGHT_BRIGHTNESS_NAME, O_RDONLY);
	if (fd < 0)
		return false;

	memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

	count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
	if (count > 0)
		value = atoi(buffer);

	close(fd);

	value = (uint32_t) ((value / (float)max * 100.0f) + 0.5f);
	return true;
#endif
}

void ApiSystem::setBrighness(int value)
{
#if !WIN32	
	if (value < 1)
		value = 1;

	if (value > 100)
		value = 100;

	int fd;
	int max = 100;
	char buffer[BACKLIGHT_BUFFER_SIZE + 1];
	ssize_t count;

	fd = open(DBACKLIGHT_BRIGHTNESS_MAX_NAME, O_RDONLY);
	if (fd < 0)
		return;

	memset(buffer, 0, BACKLIGHT_BUFFER_SIZE + 1);

	count = read(fd, buffer, BACKLIGHT_BUFFER_SIZE);
	if (count > 0)
		max = atoi(buffer);

	close(fd);

	if (max == 0) 
		return;

	fd = open(DBACKLIGHT_BRIGHTNESS_NAME, O_WRONLY);
	if (fd < 0)
		return;
	
	float percent = (value / 100.0f * (float)max) + 0.5f;
	sprintf(buffer, "%d\n", (uint32_t)percent);

	count = write(fd, buffer, strlen(buffer));
	
	if (count < 0)
		LOG(LogError) << "ApiSystem::setBrighness failed";
	
	close(fd);
#endif
}

// ---- Display LUT: gamma, temperature, night filter ----
#define GAMMA_CONF    "/etc/gamma-settings.conf"
#define NIGHT_PREVIEW "/dev/shm/NIGHT_PREVIEW"

struct GammaConf
{
	float gamma        = 1.0f;
	int   tone         = 5500;
	bool  nightEnabled = false;
	int   nightStart   = 1260; // 9:00 PM (minutes since midnight)
	int   nightEnd     = 420;  // 7:00 AM
	int   nightTone    = 3400;
};

static bool validTone(int t)   { return t >= 2700 && t <= 6500; }
static bool validMinute(int m) { return m >= 0 && m < 1440 && m % 15 == 0; }

static GammaConf readGammaConf()
{
	GammaConf c;
#if !WIN32
	std::ifstream conf(GAMMA_CONF);
	std::string line;
	while (std::getline(conf, line))
	{
		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;

		std::string key = line.substr(0, eq);
		std::string val = line.substr(eq + 1);
		int iv = atoi(val.c_str());

		if (key == "GAMMA")              c.gamma = (float)atof(val.c_str());
		else if (key == "TONE")          { if (validTone(iv)) c.tone = iv; }
		else if (key == "NIGHT_ENABLED") c.nightEnabled = (iv == 1);
		else if (key == "NIGHT_START")   { if (validMinute(iv)) c.nightStart = iv; }
		else if (key == "NIGHT_END")     { if (validMinute(iv)) c.nightEnd = iv; }
		else if (key == "NIGHT_TONE")    { if (validTone(iv)) c.nightTone = iv; }
	}
#endif
	return c;
}

static void writeGammaConf(const GammaConf& c)
{
#if !WIN32
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		"printf 'GAMMA=%.1f\\nTONE=%d\\nNIGHT_ENABLED=%d\\nNIGHT_START=%d\\nNIGHT_END=%d\\nNIGHT_TONE=%d\\n' | sudo tee " GAMMA_CONF " > /dev/null 2>&1",
		c.gamma, c.tone, c.nightEnabled ? 1 : 0, c.nightStart, c.nightEnd, c.nightTone);
	runSystemCommand(cmd, "", nullptr);
#endif
}

static bool nightActive(const GammaConf& c)
{
	if (!c.nightEnabled || c.nightStart == c.nightEnd)
		return false;

	time_t now = time(nullptr);
	struct tm t = *localtime(&now);
	int m = t.tm_hour * 60 + t.tm_min;

	if (c.nightStart < c.nightEnd)
		return m >= c.nightStart && m < c.nightEnd;

	return m >= c.nightStart || m < c.nightEnd; // window crosses midnight
}

static int effectiveTone(const GammaConf& c)
{
	return nightActive(c) ? c.nightTone : c.tone;
}

static void applyDisplayLut(float gamma, int tone)
{
#if !WIN32
	char cmd[96];
	sprintf(cmd, "/usr/local/bin/gamma -s %.1f -t %d > /dev/null 2>&1", gamma, tone);
	runSystemCommand(cmd, "", nullptr);

	std::ofstream shmGamma("/dev/shm/CURRENT_GAMMA");
	shmGamma << std::fixed << std::setprecision(1) << gamma;
	shmGamma.close();

	std::ofstream shmTone("/dev/shm/CURRENT_TONE");
	shmTone << tone;
	shmTone.close();
#endif
}

float ApiSystem::getGamma()
{
	return readGammaConf().gamma;
}

void ApiSystem::setGamma(float value)
{
	GammaConf c = readGammaConf();
	c.gamma = value;
	writeGammaConf(c);
	applyDisplayLut(c.gamma, effectiveTone(c));
}

int ApiSystem::getTone()
{
	return readGammaConf().tone;
}

void ApiSystem::setTone(int value)
{
	GammaConf c = readGammaConf();
	c.tone = value;
	writeGammaConf(c);
	applyDisplayLut(c.gamma, effectiveTone(c));
}

int ApiSystem::getEffectiveTone()
{
	return effectiveTone(readGammaConf());
}

bool ApiSystem::isNightFilterActive()
{
	return nightActive(readGammaConf());
}

bool ApiSystem::getNightFilterEnabled() { return readGammaConf().nightEnabled; }
int  ApiSystem::getNightFilterStart()   { return readGammaConf().nightStart; }
int  ApiSystem::getNightFilterEnd()     { return readGammaConf().nightEnd; }
int  ApiSystem::getNightFilterTone()    { return readGammaConf().nightTone; }

void ApiSystem::setNightFilter(bool enabled, int start, int end, int tone)
{
	GammaConf c = readGammaConf();
	if (c.nightEnabled == enabled && c.nightStart == start && c.nightEnd == end && c.nightTone == tone)
		return; // nothing changed

	c.nightEnabled = enabled;
	c.nightStart   = start;
	c.nightEnd     = end;
	c.nightTone    = tone;
	writeGammaConf(c);
}

void ApiSystem::previewNightTone(int tone)
{
#if !WIN32
	std::ofstream flag(NIGHT_PREVIEW); // tells night_filter.sh to stand down
	flag.close();
#endif
	applyDisplayLut(readGammaConf().gamma, tone);
}

void ApiSystem::endNightPreview()
{
#if !WIN32
	if (access(NIGHT_PREVIEW, F_OK) != 0)
		return; // no preview running
	unlink(NIGHT_PREVIEW);
#endif
	GammaConf c = readGammaConf();
	applyDisplayLut(c.gamma, effectiveTone(c));
}

BatteryInformation ApiSystem::getBatteryInformation(bool summary)
{
	LOG(LogDebug) << "ApiSystem::getBatteryInformation()";

	return queryBatteryInformation(summary); // platform.h
}

