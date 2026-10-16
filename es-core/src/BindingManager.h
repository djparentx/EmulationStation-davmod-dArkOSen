#pragma once
#ifndef ES_CORE_BINDINGMANAGER_H
#define ES_CORE_BINDINGMANAGER_H

// Ported from AmberELEC ES (es-core/src/BindingManager.h)
// Resolves theme bindings ({game:xxx}, {system:xxx}, {global:xxx}, {settings:xxx}) stored by
// ThemeData as "<property>_binding" and pushes the result into components via setProperty()

#include <string>

class GuiComponent;
class IBindable;

enum class BindablePropertyType
{
	String,
	Path,
	Int,
	Float,
	Bool,
	Null
};

struct BindableProperty
{
public:
	static BindableProperty Null;
	static BindableProperty EmptyString;

	BindableProperty() { i = 0; f = 0; b = false; type = BindablePropertyType::Null; };

	BindableProperty(const std::string& value, const BindablePropertyType valueType = BindablePropertyType::String) { i = 0; f = 0; b = false; s = value; type = valueType; };
	BindableProperty(const int& value) { i = value; f = 0; b = false; type = BindablePropertyType::Int; };
	BindableProperty(const float& value) { i = 0; f = value; b = false; type = BindablePropertyType::Float; };
	BindableProperty(const bool& value) { i = 0; f = 0; b = value; type = BindablePropertyType::Bool; };
	BindableProperty(const char* value) { i = 0; f = 0; b = false; if (value == nullptr) type = BindablePropertyType::Null; else { s = value; type = BindablePropertyType::String; } };

	int          i;
	float        f;
	bool         b;
	std::string  s;
	BindablePropertyType type;
};

class IBindable
{
public:
	virtual ~IBindable() { }

	virtual BindableProperty getProperty(const std::string& name) = 0;
	virtual std::string getBindableTypeName() = 0;
	virtual IBindable* getBindableParent() { return nullptr; };
};

// AmberELEC: a themed child of an <itemTemplate>, exposed as {name:property} (<image name="cover"> -> {cover:visible})
class ComponentBinding : public IBindable
{
public:
	ComponentBinding(GuiComponent* comp, IBindable* parent);

	BindableProperty getProperty(const std::string& name) override;
	std::string getBindableTypeName() override { return mTypeName; }
	IBindable* getBindableParent() override { return mParent; };

protected:
	IBindable*    mParent;
	std::string   mTypeName;
	GuiComponent* mComponent;
};

// AmberELEC: the <itemTemplate> root, exposed as {grid:label} / {grid:h} / {grid:y}
class GridTemplateBinding : public ComponentBinding
{
public:
	GridTemplateBinding(GuiComponent* comp, const std::string& label, IBindable* parent);
	BindableProperty getProperty(const std::string& name) override;

	std::string getBindableTypeName() override { return "grid"; }

protected:
	std::string   mLabel;
};

class BindingManager
{
public:
	static void          updateBindings(GuiComponent* comp, IBindable* bindable, bool recursive = true);

	// storyboard <animation enabled="..."> and other boolean conditions
	static bool          evaluateBoolean(const std::string& xp, IBindable* bindable);

	// value of an expression with {game:...} / {system:...} bindings (e.g. a <gameextras> path)
	static std::string   evaluateBindableExpression(const std::string& xp, IBindable* bindable);

private:
	static void          bindValues(IBindable* current, std::string& xp, bool showDefaultText, std::string& evaluableExpression);
	static std::string   updateBoundExpression(std::string& xp, IBindable* bindable, bool showDefaultText);
};

#endif // ES_CORE_BINDINGMANAGER_H
