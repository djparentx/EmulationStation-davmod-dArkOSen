#include <string>
#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_DETAILED_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_DETAILED_GAME_LIST_VIEW_H

#include "views/gamelist/BasicGameListView.h"
#include "views/gamelist/DetailedContainer.h"

class DetailedGameListView : public BasicGameListView
{
public:
	DetailedGameListView(Window* window, FolderData* root);
	~DetailedGameListView();

	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme) override;
	virtual void onShow() override;

	virtual const char* getName() const override 
	{ 
		if (!mCustomThemeName.empty())
			return mCustomThemeName.c_str();

		return "detailed"; 
	}

	virtual void launch(FileData* game) override;

private:
	void updateInfoPanel();

	DetailedContainer* mDetails = nullptr;

	FileData* mStoryboardFile = nullptr; // game whose activate storyboards are currently running
};

#endif // ES_APP_VIEWS_GAME_LIST_DETAILED_GAME_LIST_VIEW_H
