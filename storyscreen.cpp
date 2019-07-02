#include "storyscreen.h"

#include <assert.h>

#include <prism/blitz.h>
#include <prism/stlutil.h>

#include "menuscreen.h"

using namespace std;

static struct {
	MugenDefScript mScript;
	MugenDefScriptGroup* mCurrentGroup;
	MugenSpriteFile mSprites;

	MugenAnimation* mOldAnimation;
	MugenAnimation* mAnimation;
	int mAnimationID;
	int mOldAnimationID;

	Position mOldAnimationBasePosition;
	Position mAnimationBasePosition;

	Position mTextPosition = makePosition(0,0,1);
	int mTextWidth = INF;

	int mSpeakerID;
	int mTextID;

	int mIsStoryOver;

	char mDefinitionPath[1024];
} gStoryScreenData;

static int isImageGroup() {
	string name = gStoryScreenData.mCurrentGroup->mName;
	char firstW[100];
	sscanf(name.data(), "%s", firstW);

	return !strcmp("Image", firstW);
}

static void increaseGroup() {
	gStoryScreenData.mCurrentGroup = gStoryScreenData.mCurrentGroup->mNext;
}

static void loadImageGroup() {
	if (gStoryScreenData.mOldAnimationID != -1) {
		removeMugenAnimation(gStoryScreenData.mOldAnimationID);
		destroyMugenAnimation(gStoryScreenData.mOldAnimation);
	}

	if (gStoryScreenData.mAnimationID != -1) {
		setMugenAnimationBasePosition(gStoryScreenData.mAnimationID, &gStoryScreenData.mOldAnimationBasePosition);
	}

	gStoryScreenData.mOldAnimationID = gStoryScreenData.mAnimationID;
	gStoryScreenData.mOldAnimation = gStoryScreenData.mAnimation;


	int group = getMugenDefNumberVariableAsGroup(gStoryScreenData.mCurrentGroup, "group");
	int item = getMugenDefNumberVariableAsGroup(gStoryScreenData.mCurrentGroup, "item");
	gStoryScreenData.mTextPosition = getMugenDefVectorOrDefaultAsGroup(gStoryScreenData.mCurrentGroup, "text.pos", makePosition(0, 0, 1));
	gStoryScreenData.mTextPosition.z = 2;
	gStoryScreenData.mTextWidth = getMugenDefIntegerOrDefaultAsGroup(gStoryScreenData.mCurrentGroup, "text.width", INF);
	gStoryScreenData.mAnimation = createOneFrameMugenAnimationForSprite(group, item);
	gStoryScreenData.mAnimationID = addMugenAnimation(gStoryScreenData.mAnimation, &gStoryScreenData.mSprites, makePosition(0, 0, 0));
	setMugenAnimationBasePosition(gStoryScreenData.mAnimationID, &gStoryScreenData.mAnimationBasePosition);

	increaseGroup();
}


static int isTextGroup() {
	string name = gStoryScreenData.mCurrentGroup->mName;
	char firstW[100];
	sscanf(name.data(), "%s", firstW);

	return !strcmp("Text", firstW);
}

static void loadTextGroup() {
	if (gStoryScreenData.mTextID != -1) {
		removeMugenText(gStoryScreenData.mTextID);
		removeMugenText(gStoryScreenData.mSpeakerID);
	}

	char* speaker = getAllocatedMugenDefStringVariableAsGroup(gStoryScreenData.mCurrentGroup, "speaker");
	char* text = getAllocatedMugenDefStringVariableAsGroup(gStoryScreenData.mCurrentGroup, "text");

	gStoryScreenData.mSpeakerID = addMugenText(speaker, gStoryScreenData.mTextPosition - makePosition(0, 20, 0), 4);

	gStoryScreenData.mTextID = addMugenText(text, gStoryScreenData.mTextPosition, 4);
	setMugenTextBuildup(gStoryScreenData.mTextID, 1);
	setMugenTextTextBoxWidth(gStoryScreenData.mTextID, gStoryScreenData.mTextWidth);

	freeMemory(speaker);
	freeMemory(text);

	increaseGroup();
}

static int isTitleGroup() {
	string name = gStoryScreenData.mCurrentGroup->mName;
	char firstW[100];
	sscanf(name.data(), "%s", firstW);

	return !strcmp("Title", firstW);
}

static void goToTitle(void* tCaller) {
	(void)tCaller;
	setNewScreen(getMenuScreen());
}

static void loadTitleGroup() {
	gStoryScreenData.mIsStoryOver = 1;

	addFadeOut(30, goToTitle, NULL);
}

static void loadNextStoryGroup() {
	int isRunning = 1;
	while (isRunning) {
		if (isImageGroup()) {
			loadImageGroup();
		}
		else if (isTextGroup()) {
			loadTextGroup();
			break;
		}
		else if (isTitleGroup()) {
			loadTitleGroup();
			break;
		}
		else {
			logError("Unidentified group type.");
			//logErrorString(gStoryScreenData.mCurrentGroup->mName);
			abortSystem();
		}
	}
}

static void findStartOfStoryBoard() {
	gStoryScreenData.mCurrentGroup = gStoryScreenData.mScript.mFirstGroup;

	while (gStoryScreenData.mCurrentGroup && "STORYSTART" != gStoryScreenData.mCurrentGroup->mName) {
		gStoryScreenData.mCurrentGroup = gStoryScreenData.mCurrentGroup->mNext;
	}

	assert(gStoryScreenData.mCurrentGroup);
	gStoryScreenData.mCurrentGroup = gStoryScreenData.mCurrentGroup->mNext;
	assert(gStoryScreenData.mCurrentGroup);

	gStoryScreenData.mAnimationID = -1;
	gStoryScreenData.mOldAnimationID = -1;
	gStoryScreenData.mTextID = -1;

	gStoryScreenData.mOldAnimationBasePosition = makePosition(0, 0, 1);
	gStoryScreenData.mAnimationBasePosition = makePosition(0, 0, 2);

	loadNextStoryGroup();
}



static void loadStoryScreen() {
	gStoryScreenData.mIsStoryOver = 0;

	stringstream scriptPath;
	scriptPath << "endings/" << gStoryScreenData.mDefinitionPath << ".def";
	loadMugenDefScript(&gStoryScreenData.mScript, scriptPath.str().data());

	scriptPath = stringstream();
	scriptPath << "endings/" << gStoryScreenData.mDefinitionPath << ".sff";
	gStoryScreenData.mSprites = loadMugenSpriteFileWithoutPalette(scriptPath.str());

	findStartOfStoryBoard();
}


static void updateText() {
	if (gStoryScreenData.mIsStoryOver) return;
	if (gStoryScreenData.mTextID == -1) return;

	if (hasPressedAFlankSingle(0) || hasPressedAFlankSingle(1)) {
		if (isMugenTextBuiltUp(gStoryScreenData.mTextID)) {
			loadNextStoryGroup();
		}
		else {
			setMugenTextBuiltUp(gStoryScreenData.mTextID);
		}
	}
}

static void updateStoryScreen() {

	updateText();
}


Screen gStoryScreen;

Screen* getStoryScreen() {
	gStoryScreen = makeScreen(loadStoryScreen, updateStoryScreen);
	return &gStoryScreen;
}

void setCurrentStoryDefinitionFile(const char* tPath) {
	strcpy(gStoryScreenData.mDefinitionPath, tPath);
}
