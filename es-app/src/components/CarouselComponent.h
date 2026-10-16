#pragma once
#ifndef ES_APP_COMPONENTS_CAROUSEL_COMPONENT_H
#define ES_APP_COMPONENTS_CAROUSEL_COMPONENT_H

#include "components/IList.h"
#include "GuiComponent.h"
#include <functional>
#include <memory>

class FileData;
class ThemeData;

// Batocera / AmberELEC CarouselComponent: the <gamecarousel> element of the "gamecarousel" gamelist view.
// Logos come from the game media (<imageSource>), or a text (<gamecarouselLogoText>) when the game has none.
// Logos play the theme storyboards of <gamecarouselLogo> / <gamecarouselLogoText>: "activate" (selected),
// "deactivate" (unselected), "scroll" (others while scrolling) and the default storyboard.

struct CarouselComponentData
{
	std::shared_ptr<GuiComponent> logo;
};

enum class CarouselImageSource
{
	TEXT,
	THUMBNAIL,
	IMAGE,
	MARQUEE,
	FANART,
	TITLESHOT,
	BOXART,
	CARTRIDGE,
	BOXBACK,
	MIX,
	WHEEL
};

enum class GameCarouselType : unsigned int
{
	HORIZONTAL = 0,
	VERTICAL = 1,
	VERTICAL_WHEEL = 2,
	HORIZONTAL_WHEEL = 3
};

class CarouselComponent : public IList<CarouselComponentData, FileData*>
{
public:
	CarouselComponent(Window* window);
	~CarouselComponent();

	virtual void onShow() override;

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	std::vector<HelpPrompt> getHelpPrompts() override;

	void add(const std::string& name, FileData* obj);
	void clearEntries();

	inline void setCursorChangedCallback(const std::function<void(CursorState state)>& func) { mCursorChangedCallback = func; }
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	int getLastCursor() { return mLastCursor; }
	void resetLastCursor() { mLastCursor = -1; }

	bool isHorizontalCarousel() { return mType == GameCarouselType::HORIZONTAL || mType == GameCarouselType::HORIZONTAL_WHEEL; }

	std::shared_ptr<GuiComponent> getLogo(int index);

protected:
	void onCursorChanged(const CursorState& state) override;

private:
	int  moveCursorFast(bool forward = true);

	void getCarouselFromTheme(const ThemeData::ThemeElement* elem);
	void renderCarousel(const Transform4x4f& trans);
	void ensureLogo(IList<CarouselComponentData, FileData*>::Entry& entry);
	void playStoryboards(int oldCursor);

	static bool storyboardHasProperty(GuiComponent* comp, const std::string& event, const std::string& propertyName);

	// unit is list index
	float mCamOffset;
	bool  mWasRendered;
	int   mLastCursor;

	std::function<void(CursorState state)> mCursorChangedCallback;

	std::shared_ptr<ThemeData> mTheme;
	std::string                mThemeViewName;

	GameCarouselType        mType;
	CarouselImageSource mImageSource;

	float       mLogoScale;
	float       mLogoRotation;
	Vector2f    mLogoRotationOrigin;
	Alignment   mLogoAlignment;
	int         mMaxLogoCount;
	Vector2f    mLogoSize;
	Vector2f    mLogoPos;
	std::string mDefaultTransition;
	std::string mScrollSound;
	float       mTransitionSpeed;
	float       mMinLogoOpacity;
	float       mScaledSpacing;

	bool        mAnyLogoHasScaleStoryboard;
	bool        mAnyLogoHasOpacityStoryboard;

	unsigned int mColor;
	unsigned int mColorEnd;
	bool         mColorGradientHorizontal;
};

#endif // ES_APP_COMPONENTS_CAROUSEL_COMPONENT_H
