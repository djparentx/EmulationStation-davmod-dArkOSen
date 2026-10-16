#include "components/ItemTemplateComponent.h"

#include "utils/StringUtil.h"
#include "BindingManager.h"
#include "EsLocale.h"
#include "ThemeData.h"
#include <stack>

ItemTemplateComponent::ItemTemplateComponent(const std::string& name, Window* window) : GuiComponent(window)
{
	mName = name;
}

void ItemTemplateComponent::loadTemplatedChildren(const ThemeData::ThemeElement* elem)
{
	if (elem == nullptr)
		return;

	setStoryboards(elem->storyboards);
	loadThemedChildren(elem);
}

// visible themed children, recursively (AmberELEC enumerateExtraChildrens)
std::vector<GuiComponent*> ItemTemplateComponent::enumerateThemedChildren()
{
	std::vector<GuiComponent*> ret;

	std::stack<GuiComponent*> stack;
	stack.push(this);

	while (stack.size())
	{
		GuiComponent* current = stack.top();
		stack.pop();

		for (unsigned int i = 0; i < current->getChildCount(); i++)
		{
			GuiComponent* child = current->getChild(i);
			if (!child->isThemedChild() || !child->isVisible())
				continue;

			stack.push(child);
			ret.push_back(child);
		}
	}

	return ret;
}

bool ItemTemplateComponent::playStoryboard(const std::string& event)
{
	bool started = false;

	for (auto child : enumerateThemedChildren())
		started |= child->startStoryboard(event);

	started |= startStoryboard(event);
	return started;
}

void ItemTemplateComponent::stopStoryboards()
{
	for (auto child : enumerateThemedChildren())
		child->stopStoryboard();

	stopStoryboard();
}

bool ItemTemplateComponent::storyboardExists(const std::string& event, const std::string& propertyName)
{
	for (auto child : enumerateThemedChildren())
	{
		if (propertyName.empty() ? child->hasStoryboard(event) : child->storyboardHasProperty(event, propertyName))
			return true;
	}

	return propertyName.empty() ? hasStoryboard(event) : storyboardHasProperty(event, propertyName);
}

void ItemTemplateComponent::playDefaultActivationStoryboard(bool activation)
{
	if (playStoryboard(activation ? "activate" : "deactivate") || playStoryboard(""))
		update(0);
}

void ItemTemplateComponent::updateTemplateBindings(IBindable* bindable)
{
	auto children = enumerateThemedChildren();

	if (bindable != nullptr)
	{
		// the favorite star prefix is dropped when the template shows the favorite state itself
		if (Utils::String::startsWith(mName, _U("\uF006 ")))
		{
			bool hasFavorite = false;

			for (auto comp : children)
			{
				for (auto xp : comp->getBindingExpressions())
				{
					if (xp.second.find("{game:favorite}") != std::string::npos)
					{
						hasFavorite = true;
						break;
					}
				}
			}

			if (hasFavorite)
				mName = Utils::String::replace(mName, _U("\uF006 "), "");
		}

		std::vector<IBindable*> bindables;

		IBindable* currentParent = bindable;

		for (auto comp : children)
		{
			auto id = comp->getTag();
			if (id.empty())
				continue;

			IBindable* componentBinding = new ComponentBinding(comp, currentParent);
			bindables.push_back(componentBinding);

			currentParent = componentBinding;
		}

		GridTemplateBinding localBinding(this, mName, currentParent);
		BindingManager::updateBindings(this, &localBinding);

		for (auto b : bindables)
			delete b;
	}
	else
	{
		GridTemplateBinding localBinding(this, mName, nullptr);
		BindingManager::updateBindings(this, &localBinding);
	}
}
