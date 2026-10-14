#include "components/RectangleComponent.h"

#include "ThemeData.h"
#include "math/Misc.h"
#include <cmath>
#include <vector>

#define CORNER_PIECES 8

RectangleComponent::RectangleComponent(Window* window) : GuiComponent(window),
	mColor(0), mBorderColor(0), mBorderSize(1.0f), mRoundCorners(0.0f)
{
}

unsigned int RectangleComponent::applyOpacity(unsigned int color)
{
	if (mOpacity == 255)
		return color;

	unsigned int alpha = ((color & 0xFF) * mOpacity) / 255;
	return (color & 0xFFFFFF00) | alpha;
}

// points of a rounded rectangle outline, clockwise from the top-left corner (same count for any radius)
static void roundedOutline(float x, float y, float w, float h, float r, std::vector<Vector2f>& pts)
{
	const float cx[4] = { x + r, x + w - r, x + w - r, x + r };
	const float cy[4] = { y + r, y + r, y + h - r, y + h - r };
	const float start[4] = { (float)ES_PI, (float)ES_PI * 1.5f, 0.0f, (float)ES_PI * 0.5f };

	pts.clear();
	for (int c = 0; c < 4; c++)
	{
		for (int i = 0; i <= CORNER_PIECES; i++)
		{
			float a = start[c] + ((float)ES_PI * 0.5f) * (float)i / (float)CORNER_PIECES;
			pts.push_back(Vector2f(cx[c] + r * cosf(a), cy[c] + r * sinf(a)));
		}
	}
}

void RectangleComponent::drawRoundedFill(float x, float y, float w, float h, float radius, unsigned int color)
{
	if (w <= 0 || h <= 0)
		return;

	if (radius <= 0)
	{
		Renderer::drawRect(x, y, w, h, color, color);
		return;
	}

	Renderer::drawRoundRect(x, y, w, h, radius, color);
}

void RectangleComponent::drawRoundedRing(float w, float h, float radius, float border, unsigned int color)
{
	std::vector<Vector2f> outer;
	std::vector<Vector2f> inner;
	roundedOutline(0, 0, w, h, radius, outer);
	roundedOutline(border, border, w - 2.0f * border, h - 2.0f * border, Math::max(0.0f, radius - border), inner);

	const unsigned int col = Renderer::convertColor(color);

	std::vector<Renderer::Vertex> vertices;
	for (size_t i = 0; i <= outer.size(); i++)
	{
		size_t idx = i % outer.size();
		vertices.push_back({ { outer[idx].x(), outer[idx].y() }, { 0.0f, 0.0f }, col });
		vertices.push_back({ { inner[idx].x(), inner[idx].y() }, { 0.0f, 0.0f }, col });
	}

	Renderer::bindTexture(0);
	Renderer::drawTriangleStrips(vertices.data(), (unsigned int)vertices.size());
}

void RectangleComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();

	if (mRotation == 0 && trans.r0().y() == 0)
	{
		Vector3f dim = trans * Vector3f(mSize.x(), mSize.y(), 0) - trans.translation();
		if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), dim.x(), dim.y()))
			return;
	}

	const float w = mSize.x();
	const float h = mSize.y();

	// Batocera: with roundCorners, values below 1 are a fraction of the height (border shader) - else pixels
	float border = mBorderSize;
	float radius = mRoundCorners;
	if (radius != 0)
	{
		if (radius > 0 && radius < 1)
			radius *= h;
		if (border > 0 && border < 1)
			border *= h;
	}

	radius = Math::clamp(radius, 0.0f, Math::min(w, h) / 2.0f);
	border = Math::clamp(border, 0.0f, Math::min(w, h) / 2.0f);

	if (mColor != 0)
	{
		Renderer::setMatrix(trans);
		drawRoundedFill(border, border, w - 2.0f * border, h - 2.0f * border, Math::max(0.0f, radius - border), applyOpacity(mColor));
	}

	if (!mChildren.empty())
		GuiComponent::render(parentTrans); // children, with clipChildren / clipRect

	if (border > 0 && mBorderColor != 0)
	{
		Renderer::setMatrix(trans);

		const unsigned int color = applyOpacity(mBorderColor);
		if (radius <= 0)
		{
			Renderer::drawRect(0, 0, w, border, color, color);
			Renderer::drawRect(w - border, border, border, h - border, color, color);
			Renderer::drawRect(0, h - border, w - border, border, color, color);
			Renderer::drawRect(0, border, border, h - 2.0f * border, color, color);
		}
		else
			drawRoundedRing(w, h, radius, border, color);
	}
}

void RectangleComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "rectangle");
	if (!elem)
		return;

	if (elem->has("color"))
		mColor = elem->get<unsigned int>("color");

	if (elem->has("borderColor"))
		mBorderColor = elem->get<unsigned int>("borderColor");

	if (elem->has("borderSize"))
		mBorderSize = elem->get<float>("borderSize");

	if (elem->has("roundCorners"))
		mRoundCorners = elem->get<float>("roundCorners");

	GuiComponent::applyTheme(theme, view, element, properties);
}

ThemeData::ThemeElement::Property RectangleComponent::getProperty(const std::string name)
{
	if (name == "color")
		return mColor;

	if (name == "borderColor")
		return mBorderColor;

	if (name == "borderSize")
		return mBorderSize;

	if (name == "roundCorners")
		return mRoundCorners;

	return GuiComponent::getProperty(name);
}

void RectangleComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "color")
		mColor = value.i;
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "borderColor")
		mBorderColor = value.i;
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "borderSize")
		mBorderSize = value.f;
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "roundCorners")
		mRoundCorners = value.f;
	else
		GuiComponent::setProperty(name, value);
}
