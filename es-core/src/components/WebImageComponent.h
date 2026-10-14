#pragma once
#ifndef ES_CORE_COMPONENTS_WEBIMAGE_COMPONENT_H
#define ES_CORE_COMPONENTS_WEBIMAGE_COMPONENT_H

#include "components/ImageComponent.h"

class HttpReq;

// Batocera / AmberELEC <webimage> : an image whose <path> may be an http(s) url.
// Downloads are cached in ~/.emulationstation/tmp/<host>/<path> and kept.
class WebImageComponent : public ImageComponent
{
public:
	WebImageComponent(Window* window);
	virtual ~WebImageComponent();

	void setWebImage(const std::string& path);

	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;
	void setProperty(const std::string name, const ThemeData::ThemeElement::Property& value) override;

private:
	HttpReq*    mRequest;
	std::string mUrlToLoad;
	std::string mLocalFile;
	bool        mTile;
};

#endif // ES_CORE_COMPONENTS_WEBIMAGE_COMPONENT_H
