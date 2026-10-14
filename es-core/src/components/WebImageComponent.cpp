#include "components/WebImageComponent.h"

#include "HttpReq.h"
#include "ThemeData.h"
#include "resources/ResourceManager.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <sstream>

WebImageComponent::WebImageComponent(Window* window) : ImageComponent(window), mRequest(nullptr), mTile(false)
{
}

WebImageComponent::~WebImageComponent()
{
	if (mRequest != nullptr)
		delete mRequest;
}

static bool isUrl(const std::string& path)
{
	return Utils::String::startsWith(path, "http://") || Utils::String::startsWith(path, "https://");
}

void WebImageComponent::setWebImage(const std::string& path)
{
	if (mRequest != nullptr)
	{
		delete mRequest;
		mRequest = nullptr;
	}

	mUrlToLoad.clear();
	mLocalFile.clear();

	if (!isUrl(path))
	{
		if (path.empty())
			setImage(std::string(""));
		else if (ResourceManager::getInstance()->fileExists(path))
			setImage(path, mTile);
		return;
	}

	// protocol://host/path?query
	size_t hostStart = path.find("://") + 3;
	size_t pathStart = path.find('/', hostStart);
	if (pathStart == std::string::npos)
		return;

	std::string host = path.substr(hostStart, pathStart - hostStart);
	std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return std::tolower(c); });

	std::string urlPath = path.substr(pathStart + 1);
	std::string query;

	size_t queryStart = urlPath.find('?');
	if (queryStart != std::string::npos)
	{
		query = urlPath.substr(queryStart + 1);
		urlPath = urlPath.substr(0, queryStart);
	}

	if (host.empty() || urlPath.empty())
		return;

	std::string queryHash;
	if (!query.empty())
	{
		std::stringstream ss;
		ss << "-" << std::hex << (unsigned int)std::hash<std::string>()(query);
		queryHash = ss.str();
	}

	std::string localFile = Utils::FileSystem::getHomePath() + "/.emulationstation/tmp/" + host + "/" + urlPath + queryHash;
	if (Utils::FileSystem::exists(localFile))
	{
		setImage(localFile, mTile);
		return;
	}

	mUrlToLoad = path;
	mLocalFile = localFile;
}

void WebImageComponent::update(int deltaTime)
{
	ImageComponent::update(deltaTime);

	if (mRequest == nullptr)
		return;

	auto status = mRequest->status();
	if (status == HttpReq::REQ_IN_PROGRESS)
		return;

	delete mRequest;
	mRequest = nullptr;

	if (status == HttpReq::REQ_SUCCESS && Utils::FileSystem::exists(mLocalFile))
		setImage(mLocalFile, mTile);
}

void WebImageComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	// start the download once the image is actually displayed
	if (mRequest == nullptr && !mUrlToLoad.empty() && !mLocalFile.empty())
	{
		std::string localPath = Utils::FileSystem::getParent(mLocalFile);
		if (!Utils::FileSystem::exists(localPath))
			Utils::FileSystem::createDirectory(localPath);

		mRequest = new HttpReq(mUrlToLoad, mLocalFile);
		mUrlToLoad.clear();
	}

	ImageComponent::render(parentTrans);
}

void WebImageComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	// everything but <path>, which may be an url
	applyThemeWithType(theme, view, element, properties & ~ThemeFlags::PATH, "webimage");

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "webimage");
	if (elem == nullptr)
		return;

	mTile = elem->has("tile") && elem->get<bool>("tile");

	if ((properties & ThemeFlags::PATH) && elem->has("path"))
		setWebImage(elem->get<std::string>("path"));
}

void WebImageComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	if (value.type == ThemeData::ThemeElement::Property::PropertyType::String && name == "path")
		setWebImage(value.s);
	else
		ImageComponent::setProperty(name, value);
}
