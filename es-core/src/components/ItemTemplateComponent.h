#pragma once
#ifndef ES_CORE_COMPONENTS_ITEM_TEMPLATE_COMPONENT_H
#define ES_CORE_COMPONENTS_ITEM_TEMPLATE_COMPONENT_H

#include "GuiComponent.h"
#include <string>
#include <vector>

class IBindable;

// AmberELEC CarouselItemTemplate: one item of a textlist / imagegrid / carousel / gamecarousel drawn from the
// theme's <itemTemplate> elements instead of the default text or image.
// The children are bound to the item ({game:...} / {system:...}), to the template ({grid:label}) and to each
// other ({childname:property}); storyboards ("activate", "deactivate", "scroll", default) play on the template
// and all its children.
class ItemTemplateComponent : public GuiComponent
{
public:
	ItemTemplateComponent(const std::string& name, Window* window);

	void loadTemplatedChildren(const ThemeData::ThemeElement* elem);

	// starts the storyboard "event" on the template and on every child that has one - false if none has it
	bool playStoryboard(const std::string& event);
	void stopStoryboards();
	bool storyboardExists(const std::string& event, const std::string& propertyName = "");

	// "activate" (or the default storyboard) / "deactivate" (or the default storyboard), applied at once
	void playDefaultActivationStoryboard(bool activation);

	void updateTemplateBindings(IBindable* bindable);

	// {grid:label} - a grid tile shows a different item each time it scrolls
	void setLabel(const std::string& label) { mName = label; }

private:
	std::vector<GuiComponent*> enumerateThemedChildren();

	std::string mName;
};

#endif // ES_CORE_COMPONENTS_ITEM_TEMPLATE_COMPONENT_H
