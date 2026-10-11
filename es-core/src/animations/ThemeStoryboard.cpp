#include "animations/ThemeStoryboard.h"

// Ported from AmberELEC ES (es-core/src/anim/ThemeStoryboard.cpp)

#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "utils/FileSystemUtil.h"
#include "utils/HtmlColor.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include <pugixml/src/pugixml.hpp>
#include <cstring>

/////////////////////////////////////////////////////////////////////////////////////////////
// ThemeAnimation values
/////////////////////////////////////////////////////////////////////////////////////////////

ThemeData::ThemeElement::Property ThemeFloatAnimation::computeValue(float value)
{
	return from.f * (1.0f - value) + to.f * value;
}

ThemeData::ThemeElement::Property ThemeColorAnimation::computeValue(float value)
{
	return Renderer::mixColors(from.i, to.i, value);
}

ThemeData::ThemeElement::Property ThemeVector2Animation::computeValue(float value)
{
	return Vector2f(
		from.v.x() * (1.0f - value) + to.v.x() * value,
		from.v.y() * (1.0f - value) + to.v.y() * value);
}

ThemeData::ThemeElement::Property ThemeVector4Animation::computeValue(float value)
{
	return Vector4f(
		from.r.x() * (1.0f - value) + to.r.x() * value,
		from.r.y() * (1.0f - value) + to.r.y() * value,
		from.r.z() * (1.0f - value) + to.r.z() * value,
		from.r.w() * (1.0f - value) + to.r.w() * value);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// ThemeStoryboard
/////////////////////////////////////////////////////////////////////////////////////////////

ThemeStoryboard::ThemeStoryboard(const ThemeStoryboard& src)
{
	eventName = src.eventName;
	repeat = src.repeat;
	repeatAt = src.repeatAt;

	for (auto anim : src.animations)
		animations.push_back(anim->clone());
}

ThemeStoryboard::~ThemeStoryboard()
{
	for (auto anim : animations)
		delete anim;

	animations.clear();
}

// "0.5" -> (0.5 0.5), "0.5 0.25" -> (0.5 0.25); extra spaces ignored
static std::vector<float> parseFloats(const std::string& str)
{
	std::vector<float> ret;
	for (const auto& part : Utils::String::split(Utils::String::trim(str), ' '))
		if (!part.empty())
			ret.push_back(Utils::String::toFloat(part));

	return ret;
}

static Vector2f parsePair(const std::string& str)
{
	auto v = parseFloats(str);
	if (v.size() == 0) return Vector2f(0, 0);
	if (v.size() == 1) return Vector2f(v[0], v[0]);
	return Vector2f(v[0], v[1]);
}

static Vector4f parseRect(const std::string& str)
{
	auto v = parseFloats(str);
	if (v.size() == 0) return Vector4f(0, 0, 0, 0);
	if (v.size() == 1) return Vector4f(v[0], v[0], v[0], v[0]);
	if (v.size() < 4)  return Vector4f(v[0], v[1], v[0], v[1]);
	return Vector4f(v[0], v[1], v[2], v[3]);
}

// properties every component can animate (GuiComponent::getProperty) that are not
// always declared in the element's theme type map
static const std::map<std::string, ThemeData::ElementPropertyType> commonAnimatableProperties =
{
	{ "pos",            ThemeData::NORMALIZED_PAIR },
	{ "size",           ThemeData::NORMALIZED_PAIR },
	{ "origin",         ThemeData::NORMALIZED_PAIR },
	{ "rotationOrigin", ThemeData::NORMALIZED_PAIR },
	{ "scaleOrigin",    ThemeData::NORMALIZED_PAIR },
	{ "x",              ThemeData::FLOAT },
	{ "y",              ThemeData::FLOAT },
	{ "w",              ThemeData::FLOAT },
	{ "h",              ThemeData::FLOAT },
	{ "rotation",       ThemeData::FLOAT },
	{ "opacity",        ThemeData::FLOAT },
	{ "scale",          ThemeData::FLOAT },
	{ "zIndex",         ThemeData::FLOAT },
	{ "visible",        ThemeData::BOOLEAN },
};

static int parseRepeat(const std::string& value)
{
	if (value == "forever" || value == "infinite")
		return 0;

	if (value.empty() || value == "none")
		return 1;

	return Utils::String::toInteger(value);
}

std::shared_ptr<ThemeStoryboard> ThemeStoryboard::fromXml(const pugi::xml_node& root,
	const std::map<std::string, ThemeData::ElementPropertyType>& typeMap,
	const std::string& relativePath,
	const std::function<std::string(const std::string&)>& resolve)
{
	if (strcmp(root.name(), "storyboard") != 0)
		return nullptr;

	auto sb = std::make_shared<ThemeStoryboard>();

	sb->eventName = resolve(root.attribute("event").as_string());
	sb->repeat = parseRepeat(resolve(root.attribute("repeat").as_string()));

	std::string repeatAt = resolve(root.attribute("repeatAt").as_string());
	if (repeatAt.empty())
		repeatAt = resolve(root.attribute("repeatat").as_string());

	if (!repeatAt.empty())
	{
		sb->repeat = 0; // repeatAt implies forever
		sb->repeatAt = Utils::String::toInteger(repeatAt);
	}

	for (pugi::xml_node node = root.child("animation"); node; node = node.next_sibling("animation"))
	{
		std::string prop = node.attribute("property").as_string();
		if (prop.empty())
			continue;

		ThemeData::ElementPropertyType type;

		auto typeIt = typeMap.find(prop);
		if (typeIt != typeMap.cend())
			type = typeIt->second;
		else
		{
			auto commonIt = commonAnimatableProperties.find(prop);
			if (commonIt == commonAnimatableProperties.cend())
			{
				LOG(LogWarning) << "Unknown storyboard property type \"" << prop << "\"";
				continue;
			}

			type = commonIt->second;
		}

		const bool hasFrom = node.attribute("from");
		const bool hasTo = node.attribute("to");
		const std::string from = resolve(node.attribute("from").as_string());
		const std::string to = resolve(node.attribute("to").as_string());

		ThemeAnimation* anim = nullptr;

		switch (type)
		{
		case ThemeData::NORMALIZED_RECT:
			anim = new ThemeVector4Animation();
			if (hasFrom) anim->from = parseRect(from);
			if (hasTo) anim->to = parseRect(to);
			break;

		case ThemeData::NORMALIZED_PAIR:
			anim = new ThemeVector2Animation();
			if (hasFrom) anim->from = parsePair(from);
			if (hasTo) anim->to = parsePair(to);
			break;

		case ThemeData::COLOR:
			anim = new ThemeColorAnimation();
			if (hasFrom) anim->from = Utils::HtmlColor::parse(from);
			if (hasTo) anim->to = Utils::HtmlColor::parse(to);
			break;

		case ThemeData::FLOAT:
			anim = new ThemeFloatAnimation();
			if (hasFrom) anim->from = Utils::String::toFloat(from);
			if (hasTo) anim->to = Utils::String::toFloat(to);
			break;

		case ThemeData::PATH:
			anim = new ThemeStringAnimation();
			if (hasFrom) anim->from = Utils::FileSystem::resolveRelativePath(from, relativePath, true);
			if (hasTo) anim->to = Utils::FileSystem::resolveRelativePath(to, relativePath, true);
			break;

		case ThemeData::STRING:
			anim = new ThemeStringAnimation();
			if (hasFrom) anim->from = from;
			if (hasTo) anim->to = to;
			break;

		case ThemeData::BOOLEAN:
			anim = new ThemeBoolAnimation();
			if (hasFrom) anim->from = Utils::String::toBoolean(from);
			if (hasTo) anim->to = Utils::String::toBoolean(to);
			break;

		default:
			LOG(LogWarning) << "Unsupported animation property type \"" << prop << "\"";
			continue;
		}

		anim->propertyName = prop;

		std::string mode = "linear";

		for (pugi::xml_attribute xattr : node.attributes())
		{
			const std::string name = xattr.name();
			const std::string value = resolve(xattr.as_string());

			if (name == "enabled")
			{
				// {game:xxx} / {system:xxx} / function expressions are evaluated when the storyboard starts
				if ((value.find('{') != std::string::npos && value.find(':') != std::string::npos && value.find('}') != std::string::npos) || value.find('(') != std::string::npos)
					anim->enabledExpression = value;
				else
					anim->enabled = (value == "true" || value == "1");
			}
			else if (name == "begin")
				anim->begin = Utils::String::toInteger(value);
			else if (name == "duration")
				anim->duration = Utils::String::toInteger(value);
			else if (name == "repeat")
				anim->repeat = parseRepeat(value);
			else if (name == "autoreverse" || name == "autoReverse")
				anim->autoReverse = (value == "true" || value == "1");
			else if (name == "mode" || name == "easingMode")
				mode = Utils::String::toLower(value);
		}

		if (mode == "easein")
			anim->easingMode = ThemeAnimation::EasingMode::EaseIn;
		else if (mode == "easeincubic")
			anim->easingMode = ThemeAnimation::EasingMode::EaseInCubic;
		else if (mode == "easeinquint")
			anim->easingMode = ThemeAnimation::EasingMode::EaseInQuint;
		else if (mode == "easeout")
			anim->easingMode = ThemeAnimation::EasingMode::EaseOut;
		else if (mode == "easeoutcubic")
			anim->easingMode = ThemeAnimation::EasingMode::EaseOutCubic;
		else if (mode == "easeoutquint")
			anim->easingMode = ThemeAnimation::EasingMode::EaseOutQuint;
		else if (mode == "easeinout")
			anim->easingMode = ThemeAnimation::EasingMode::EaseInOut;
		else if (mode == "bump")
			anim->easingMode = ThemeAnimation::EasingMode::Bump;
		else
			anim->easingMode = ThemeAnimation::EasingMode::Linear;

		sb->animations.push_back(anim);
	}

	for (pugi::xml_node node = root.child("sound"); node; node = node.next_sibling("sound"))
	{
		std::string path = resolve(node.attribute("path").as_string());
		if (path.empty())
			continue;

		path = Utils::FileSystem::resolveRelativePath(path, relativePath, true);
		if (!ResourceManager::getInstance()->fileExists(path))
			continue;

		auto sound = new ThemeStringAnimation();
		sound->propertyName = "sound";
		sound->from = std::string();
		sound->to = path;

		for (pugi::xml_attribute xattr : node.attributes())
		{
			const std::string name = xattr.name();
			const std::string value = resolve(xattr.as_string());

			if (name == "begin" || name == "at")
				sound->begin = Utils::String::toInteger(value);
			else if (name == "duration")
				sound->duration = Utils::String::toInteger(value);
			else if (name == "repeat")
				sound->repeat = parseRepeat(value);
			else if (name == "autoreverse" || name == "autoReverse")
				sound->autoReverse = (value == "true" || value == "1");
		}

		sb->animations.push_back(sound);
	}

	if (sb->animations.empty())
		return nullptr;

	return sb;
}
