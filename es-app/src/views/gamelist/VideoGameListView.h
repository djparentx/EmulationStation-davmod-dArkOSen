#include <string>
#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_VIDEO_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_VIDEO_GAME_LIST_VIEW_H

#include "views/gamelist/BasicGameListView.h"
#include "views/gamelist/DetailedContainer.h"

class VideoGameListView : public BasicGameListView
{
public:
	VideoGameListView(Window* window, FolderData* root);
	virtual ~VideoGameListView();

	virtual void onShow() override;

	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme) override;

	virtual const char* getName() const override 
	{ 
		if (!mCustomThemeName.empty())
			return mCustomThemeName.c_str();

		return "video"; 
	}

	virtual void launch(FileData* game) override;

protected:
	virtual void update(int deltaTime) override;

private:
	void updateInfoPanel();

	DetailedContainer* mDetails = nullptr;

	bool		mVideoPlaying;

};

#endif // ES_APP_VIEWS_GAME_LIST_VIDEO_GAME_LIST_VIEW_H
