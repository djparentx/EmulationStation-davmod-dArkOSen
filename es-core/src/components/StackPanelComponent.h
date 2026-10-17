#pragma once
#ifndef ES_CORE_COMPONENTS_STACK_COMPONENT_H
#define ES_CORE_COMPONENTS_STACK_COMPONENT_H

// Ported from AmberELEC ES (es-core/src/components/StackPanelComponent.h)
// Children are the stackpanel's nested theme elements (GuiComponent::loadThemedChildren).

#include "GuiComponent.h"
#include "ThemeData.h"

class StackPanelComponent : public GuiComponent
{
public:
	StackPanelComponent(Window* window);

	void render(const Transform4x4f& parentTrans) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	void onSizeChanged() override;
	void update(int deltaTime) override;

	void recalcLayout() override;

private:
	void performLayout();

	bool mHorizontal;
	bool mReverse;

	float mSeparator;
};

#endif // ES_CORE_COMPONENTS_STACK_COMPONENT_H
