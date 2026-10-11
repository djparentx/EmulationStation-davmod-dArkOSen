#pragma once
#ifndef ES_CORE_ANIMATIONS_THEME_STORYBOARD_H
#define ES_CORE_ANIMATIONS_THEME_STORYBOARD_H

// Ported from AmberELEC ES (es-core/src/anim/ThemeAnimation.h + ThemeStoryboard.h)
// Animations are typed by the theme property they target and are applied through
// GuiComponent::getProperty / setProperty, so any bindable property can be animated.

#include "ThemeData.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace pugi { class xml_node; }

class ThemeAnimation
{
public:
	enum EasingMode
	{
		Linear,
		EaseIn,
		EaseInCubic,
		EaseInQuint,
		EaseOut,
		EaseOutCubic,
		EaseOutQuint,
		EaseInOut,
		Bump
	};

	ThemeAnimation()
	{
		repeat = 1;
		duration = 0;
		begin = 0;
		autoReverse = false;
		easingMode = EasingMode::Linear;
		enabled = true;

		from.type = ThemeData::ThemeElement::Property::PropertyType::Unknown;
		to.type = ThemeData::ThemeElement::Property::PropertyType::Unknown;
	}

	virtual ~ThemeAnimation() { }
	virtual ThemeAnimation* clone() const = 0;

	std::string propertyName;
	int  duration;
	int  begin;
	bool autoReverse;
	int  repeat; // 0 = forever
	EasingMode easingMode;

	bool enabled;
	std::string enabledExpression; // binding expression, evaluated when the storyboard starts

	ThemeData::ThemeElement::Property from;
	ThemeData::ThemeElement::Property to;

	virtual ThemeData::ThemeElement::Property computeValue(float value) = 0;

	void ensureInitialValue(const ThemeData::ThemeElement::Property& initialValue)
	{
		if (from.type == ThemeData::ThemeElement::Property::PropertyType::Unknown)
			from = initialValue;

		if (to.type == ThemeData::ThemeElement::Property::PropertyType::Unknown)
			to = initialValue;
	}
};

class ThemeFloatAnimation : public ThemeAnimation
{
public:
	ThemeAnimation* clone() const override { return new ThemeFloatAnimation(*this); }
	ThemeData::ThemeElement::Property computeValue(float value) override;
};

class ThemeColorAnimation : public ThemeAnimation
{
public:
	ThemeAnimation* clone() const override { return new ThemeColorAnimation(*this); }
	ThemeData::ThemeElement::Property computeValue(float value) override;
};

class ThemeVector2Animation : public ThemeAnimation
{
public:
	ThemeAnimation* clone() const override { return new ThemeVector2Animation(*this); }
	ThemeData::ThemeElement::Property computeValue(float value) override;
};

class ThemeVector4Animation : public ThemeAnimation
{
public:
	ThemeAnimation* clone() const override { return new ThemeVector4Animation(*this); }
	ThemeData::ThemeElement::Property computeValue(float value) override;
};

// string, path, bool and sound animations switch from "from" to "to" at the end
class ThemeStringAnimation : public ThemeAnimation
{
public:
	ThemeAnimation* clone() const override { return new ThemeStringAnimation(*this); }
	ThemeData::ThemeElement::Property computeValue(float value) override { return value >= 0.9999f ? to : from; }
};

class ThemeBoolAnimation : public ThemeAnimation
{
public:
	ThemeAnimation* clone() const override { return new ThemeBoolAnimation(*this); }
	ThemeData::ThemeElement::Property computeValue(float value) override { return value >= 0.9999f ? to : from; }
};

class ThemeStoryboard
{
public:
	ThemeStoryboard() : repeat(1), repeatAt(0) { }
	ThemeStoryboard(const ThemeStoryboard& src);
	~ThemeStoryboard();

	std::string eventName;
	int repeat; // 0 = forever
	int repeatAt;

	std::vector<ThemeAnimation*> animations;

	// nullptr if the node holds no usable <animation> / <sound> items
	static std::shared_ptr<ThemeStoryboard> fromXml(const pugi::xml_node& root,
		const std::map<std::string, ThemeData::ElementPropertyType>& typeMap,
		const std::string& relativePath,
		const std::function<std::string(const std::string&)>& resolve);

private:
	ThemeStoryboard& operator=(const ThemeStoryboard&) = delete;
};

#endif // ES_CORE_ANIMATIONS_THEME_STORYBOARD_H
