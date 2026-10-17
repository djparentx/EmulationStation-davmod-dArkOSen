#include <string>
#include "views/gamelist/VideoGameListView.h"

#include "components/VideoComponent.h"
#include "utils/FileSystemUtil.h"
#include "views/ViewController.h"

VideoGameListView::VideoGameListView(Window* window, FolderData* root) :
	BasicGameListView(window, root),
	mVideoPlaying(false)
{
	const float padding = 0.01f;

	mList.setPosition(mSize.x() * (0.50f + padding), mList.getPosition().y());
	mList.setSize(mSize.x() * (0.50f - padding), mList.getSize().y());
	mList.setAlignment(TextListComponent<FileData*>::ALIGN_LEFT);
	mList.setCursorChangedCallback([&](const CursorState& /*state*/) { updateInfoPanel(); });

	mDetails = new DetailedContainer(this, &mList, window, DetailedContainer::VideoView);
}

VideoGameListView::~VideoGameListView()
{
	delete mDetails;
}

void VideoGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	BasicGameListView::onThemeChanged(theme);

	mDetails->onThemeChanged(theme);
}

void VideoGameListView::updateInfoPanel()
{
	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? NULL : mList.getSelected();
	const int moveBy = file != nullptr ? getMoveBy(mList.getCursorIndex()) : 0;
	updateThemeExtras(file, moveBy);

	Utils::FileSystem::removeFile(getTitlePath());

	mVideoPlaying = (file != nullptr);

	// We're clearing / populating : don't setup fade animations
	bool isClearing = (file == nullptr && mList.getObjects().size() == 0 && mList.getCursorIndex() == 0 && mList.getScrollingVelocity() == 0);

	mDetails->updateControls(file, isClearing, moveBy, false);
}

void VideoGameListView::launch(FileData* game)
{
	float screenWidth = (float) Renderer::getScreenWidth();
	float screenHeight = (float) Renderer::getScreenHeight();

	Vector3f target(screenWidth / 2.0f, screenHeight / 2.0f, 0);

	ImageComponent* marquee = mDetails->getMarquee();
	ImageComponent* image = mDetails->getImage();
	ImageComponent* thumbnail = mDetails->getThumbnail();
	VideoComponent* video = mDetails->getVideo();

	if(marquee->hasImage() &&
		(marquee->getPosition().x() < screenWidth && marquee->getPosition().x() > 0.0f &&
		 marquee->getPosition().y() < screenHeight && marquee->getPosition().y() > 0.0f))
	{
		target = Vector3f(marquee->getCenter().x(), marquee->getCenter().y(), 0);
	}
	else if(image != nullptr && image->hasImage() &&
		(image->getPosition().x() < screenWidth && image->getPosition().x() > 2.0f &&
		 image->getPosition().y() < screenHeight && image->getPosition().y() > 2.0f))
	{
		target = Vector3f(image->getCenter().x(), image->getCenter().y(), 0);
	}
	else if(mHeaderImage.hasImage() &&
		(mHeaderImage.getPosition().x() < screenWidth && mHeaderImage.getPosition().x() > 0.0f &&
		 mHeaderImage.getPosition().y() < screenHeight && mHeaderImage.getPosition().y() > 0.0f))
	{
		target = Vector3f(mHeaderImage.getCenter().x(), mHeaderImage.getCenter().y(), 0);
	}
	else if(video->getPosition().x() < screenWidth && video->getPosition().x() > 0.0f &&
		 video->getPosition().y() < screenHeight && video->getPosition().y() > 0.0f)
	{
		target = Vector3f(video->getCenter().x(), video->getCenter().y(), 0);
	}
	else if (thumbnail != nullptr && thumbnail->getPosition().x() < screenWidth && thumbnail->getPosition().x() > 0.0f &&
		thumbnail->getPosition().y() < screenHeight && thumbnail->getPosition().y() > 0.0f)
	{
		target = Vector3f(thumbnail->getCenter().x(), thumbnail->getCenter().y(), 0);
	}

	ViewController::get()->launch(game, target);
}

void VideoGameListView::update(int deltaTime)
{
	BasicGameListView::update(deltaTime);
	mDetails->getVideo()->update(deltaTime);
}

void VideoGameListView::onShow()
{
	mExtrasFile = nullptr; // replay "open" / default storyboards
	mLastCursorIndex = -1;
	GuiComponent::onShow();
	updateInfoPanel();
}
