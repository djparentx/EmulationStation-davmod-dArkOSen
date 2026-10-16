#include <string>
#include "views/gamelist/DetailedGameListView.h"
#include "components/VideoComponent.h"
#include "views/ViewController.h"

DetailedGameListView::DetailedGameListView(Window* window, FolderData* root) :
	BasicGameListView(window, root)
{
	const float padding = 0.01f;

	mList.setPosition(mSize.x() * (0.50f + padding), mList.getPosition().y());
	mList.setSize(mSize.x() * (0.50f - padding), mList.getSize().y());
	mList.setAlignment(TextListComponent<FileData*>::ALIGN_LEFT);
	mList.setCursorChangedCallback([&](const CursorState& /*state*/) { updateInfoPanel(); });

	mDetails = new DetailedContainer(this, &mList, window, DetailedContainer::DetailedView);
}

DetailedGameListView::~DetailedGameListView()
{
	delete mDetails;
}

void DetailedGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	BasicGameListView::onThemeChanged(theme);

	mDetails->onThemeChanged(theme);

	mStoryboardFile = nullptr; // extras were rebuilt - start their storyboards
	updateInfoPanel();
}

void DetailedGameListView::updateInfoPanel()
{
	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? NULL : mList.getSelected();
	const int moveBy = (file != nullptr) ? getMoveBy(mList.getCursorIndex()) : 0;

	// IList fires CURSOR_STOPPED on key press and again on release - only replay
	// activate storyboards when the selected game actually changes
	const bool restartStoryboards = (file != nullptr && file != mStoryboardFile);
	mStoryboardFile = file;

	if (file != nullptr)
		updateThemeExtras(file, moveBy);

	// We're clearing / populating : don't setup fade animations
	bool isClearing = (file == nullptr && mList.getObjects().size() == 0 && mList.getCursorIndex() == 0 && mList.getScrollingVelocity() == 0);

	mDetails->updateControls(file, isClearing, moveBy, restartStoryboards);
}

void DetailedGameListView::launch(FileData* game)
{
	Vector3f target(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0);

	VideoComponent* video = mDetails->getVideo();
	ImageComponent* image = mDetails->getImage();
	ImageComponent* thumbnail = mDetails->getThumbnail();

	if (video != nullptr)
		target = Vector3f(video->getCenter().x(), video->getCenter().y(), 0);
	else if (image != nullptr && image->hasImage())
		target = Vector3f(image->getCenter().x(), image->getCenter().y(), 0); 
	else if (thumbnail != nullptr && thumbnail->hasImage())
		target = Vector3f(thumbnail->getCenter().x(), thumbnail->getCenter().y(), 0);

	ViewController::get()->launch(game, target);
}

void DetailedGameListView::onShow()
{
	BasicGameListView::onShow();
	mStoryboardFile = nullptr; // replay storyboards when the view is shown again
	updateInfoPanel();
}
