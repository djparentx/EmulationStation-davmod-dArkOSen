#pragma once
#ifndef ES_CORE_COMPONENTS_SCROLLBAR_COMPONENT_H
#define ES_CORE_COMPONENTS_SCROLLBAR_COMPONENT_H

// Ported from AmberELEC ES (es-core/src/components/ScrollbarComponent.h)
// Theme properties: scrollbarColor (enables it), scrollbarSize, scrollbarCorner, scrollbarAlignment

#include "GuiComponent.h"

#define SB_ALIGN_NORMAL		0
#define SB_ALIGN_REVERSED	1
#define SB_ALIGN_OUTER		2

class ScrollbarComponent : public GuiComponent
{
public:
	ScrollbarComponent(Window* window);

	bool isEnabled() { return mEnabled; }

	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	void setContainerBounds(Vector3f position, Vector2f size, bool vertical = true);

	void setRange(float min, float max, float pageSize)
	{
		mMin = min;
		mMax = max;
		mPage = pageSize;
	}

	void setScrollPosition(float position);
	void onCursorChanged();

	void setColor(unsigned int color) override { mColor = color; };

	void setCornerSize(float corner) { mCornerSize = corner; }
	void setScrollSize(float size) { mScrollSize = size; }

	void fromTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, const std::string& containerType);

private:
	int   mAlignment;

	float mMin;
	float mMax;
	float mPage;
	float mPosition;

	int   mVisibleTime;
	int   mFadeOutTime;

	float mCornerSize;
	float mScrollSize;

	unsigned int mColor;

	bool mVertical;
	bool mEnabled;
};

#endif // ES_CORE_COMPONENTS_SCROLLBAR_COMPONENT_H
