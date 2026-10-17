#pragma once
#ifndef ES_CORE_COMPONENTS_RECTANGLE_COMPONENT_H
#define ES_CORE_COMPONENTS_RECTANGLE_COMPONENT_H

#include "GuiComponent.h"
#include "renderers/Renderer.h"

// Batocera / AmberELEC <rectangle> : filled box with an optional (rounded) border, drawn behind its nested elements.
// borderSize / roundCorners below 1 are a fraction of the height when roundCorners is set (Batocera border shader).
class RectangleComponent : public GuiComponent
{
public:
	RectangleComponent(Window* window);

	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;
	void render(const Transform4x4f& parentTrans) override;

	ThemeData::ThemeElement::Property getProperty(const std::string name) override;
	void setProperty(const std::string name, const ThemeData::ThemeElement::Property& value) override;

	void setColor(unsigned int color) { mColor = color; }
	void setBorderColor(unsigned int color) { mBorderColor = color; }
	void setBorderSize(float size) { mBorderSize = size; }
	void setRoundCorners(float value) { mRoundCorners = value; }

private:
	unsigned int applyOpacity(unsigned int color);
	void drawRoundedFill(float x, float y, float w, float h, float radius, unsigned int color);
	void drawRoundedRing(float w, float h, float radius, float border, unsigned int color);

	unsigned int mColor;
	unsigned int mBorderColor;
	float        mBorderSize;
	float        mRoundCorners;
};

#endif // ES_CORE_COMPONENTS_RECTANGLE_COMPONENT_H
