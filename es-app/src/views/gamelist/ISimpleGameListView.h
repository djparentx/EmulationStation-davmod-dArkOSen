#include <string>
#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_ISIMPLE_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_ISIMPLE_GAME_LIST_VIEW_H

#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "views/gamelist/IGameListView.h"
#include <stack>

class ISimpleGameListView : public IGameListView
{
public:
	ISimpleGameListView(Window* window, FolderData* root);
	virtual ~ISimpleGameListView();

	// Called when a new file is added, a file is removed, a file's metadata changes, or a file's children are sorted.
	// NOTE: FILE_SORTED is only reported for the topmost FileData, where the sort started.
	//       Since sorts are recursive, that FileData's children probably changed too.
	virtual void onFileChanged(FileData* file, FileChangeType change);

	// Called whenever the theme changes.
	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme);

	virtual FileData* getCursor() = 0;
	virtual void setCursor(FileData*) = 0;

	virtual bool input(InputConfig* config, Input input) override;
	virtual void launch(FileData* game) = 0;

	virtual std::vector<std::string> getEntriesLetters() override;
	// virtual std::vector<FileData*> getFileDataEntries() = 0;

	virtual bool moveToLetter(const std::string& letter);
	virtual void moveToLetterByOffset(int offset);
	virtual void moveToNextLetter();
	virtual void moveToPreviousLetter();

	virtual void update(int deltaTime) override;
	virtual void onShow() override;

protected:

	virtual std::string getQuickSystemSelectRightButton() = 0;
	virtual std::string getQuickSystemSelectLeftButton() = 0;
	virtual void populateList(const std::vector<FileData*>& files) = 0;

	// resolve {game:xxx} / {system:xxx} theme bindings of the extras for the selected game
	void updateThemeExtrasBindings(FileData* file);

	// AmberELEC DetailedContainerHost: when the selected game changes, the outgoing extras play their
	// "deactivate" storyboards while a fresh set of extras, bound to the new game, plays "activate".
	// moveBy = cursor movement (0 = first display / refresh: "open" or the default storyboard)
	void updateThemeExtras(FileData* file, int moveBy);
	int  getMoveBy(int cursorIndex);

	// AmberELEC DetailedContainer::handleStoryBoard / hasActivationStoryboard (recursive to children)
	static void handleStoryboard(GuiComponent* comp, bool activate, int moveBy, FileData* file);
	static bool hasActivationStoryboard(GuiComponent* comp, bool checkActivate = true, bool checkDeactivate = true);

	TextComponent mHeaderText;
	ImageComponent mHeaderImage;
	ImageComponent mBackground;


	std::vector<GuiComponent*> mThemeExtras;

	std::stack<FileData*> mCursorStack;

	FileData* mExtrasFile = nullptr;   // game the current extras are bound to
	int       mLastCursorIndex = -1;

private:
	void clearDeactivatingExtras();

	std::vector<std::vector<GuiComponent*>> mDeactivatingExtras; // outgoing extras still playing "deactivate"
};

#endif // ES_APP_VIEWS_GAME_LIST_ISIMPLE_GAME_LIST_VIEW_H
