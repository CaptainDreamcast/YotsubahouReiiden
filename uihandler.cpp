#include "uihandler.h"

#include <prism/blitz.h>

#include "player.h"
#include "dialoghandler.h"

#define UI_BASE_Z 85

static struct {
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	int mBGID;
	int mHeartAnimations[8];
	int mBombAnimations[8];

	int mHighScoreTextID;
	int mScoreTextID;
	int mPowerTextID;
	int mPointTextID;

	uint64_t mHighScore;
} gUIHandler;

static string getPointText()
{
	stringstream ss;
	ss << getCollectedItemAmount() << "/" << getNextItemLifeUpAmount();
	return ss.str();
}

static void loadUIHandler(void* tData) {
	(void)tData;
	gUIHandler.mSprites = loadMugenSpriteFileWithoutPalette("data/UI.sff");
	gUIHandler.mAnimations = loadMugenAnimationFile("data/UI.air");

	gUIHandler.mBGID = addMugenAnimation(getMugenAnimation(&gUIHandler.mAnimations, 1), &gUIHandler.mSprites, makePosition(0, 0, UI_BASE_Z));

	for (int i = 0; i < 8; i++) {
		gUIHandler.mHeartAnimations[i] = addMugenAnimation(getMugenAnimation(&gUIHandler.mAnimations, 2), &gUIHandler.mSprites, makePosition(259 + i * 6, 50, UI_BASE_Z + 2));
		setMugenAnimationVisibility(gUIHandler.mHeartAnimations[i], 0);
		gUIHandler.mBombAnimations[i] = addMugenAnimation(getMugenAnimation(&gUIHandler.mAnimations, 3), &gUIHandler.mSprites, makePosition(259 + i * 6, 69, UI_BASE_Z + 2));
		setMugenAnimationVisibility(gUIHandler.mBombAnimations[i], 0);
	}

	gUIHandler.mHighScoreTextID = addMugenTextMugenStyle("232,124,241", makePosition(315, 29, UI_BASE_Z + 2), makeVector3DI(2, 0, -1));
	setMugenTextScale(gUIHandler.mHighScoreTextID, 0.7);
	gUIHandler.mScoreTextID = addMugenTextMugenStyle("0", makePosition(315, 40, UI_BASE_Z + 2), makeVector3DI(2, 0, -1));
	setMugenTextScale(gUIHandler.mScoreTextID, 0.7);
	gUIHandler.mPowerTextID = addMugenTextMugenStyle("1.00/4.00", makePosition(310, 98, UI_BASE_Z + 2), makeVector3DI(2, 0, -1));
	setMugenTextScale(gUIHandler.mPowerTextID, 0.65);
	setMugenTextColorRGB(gUIHandler.mPowerTextID, 1, 160 / 255.0, 17 / 255.0);
	gUIHandler.mPointTextID = addMugenTextMugenStyle("0/50", makePosition(310, 109, UI_BASE_Z + 2), makeVector3DI(2, 0, -1));
	setMugenTextScale(gUIHandler.mPointTextID, 0.65);
	setMugenTextColorRGB(gUIHandler.mPointTextID, 75/ 255.0, 214 / 255.0, 1.0);
}

static void updateLives() {
	int lifeAmount = getPlayerLife();
	int i;
	for (i = 0; i < lifeAmount; i++) {
		setMugenAnimationVisibility(gUIHandler.mHeartAnimations[i], 1);
	}
	for (; i < 8; i++) {
		setMugenAnimationVisibility(gUIHandler.mHeartAnimations[i], 0);
	}
}

static void updateBombs() {
	int bombAmount = getPlayerBombs();
	int i;
	for (i = 0; i < bombAmount; i++) {
		setMugenAnimationVisibility(gUIHandler.mBombAnimations[i], 1);
	}
	for (; i < 8; i++) {
		setMugenAnimationVisibility(gUIHandler.mBombAnimations[i], 0);
	}
}

string scoreToString(uint64_t tScore) {
	string ret;
	stringstream ss;

	while (tScore >= 1000) {
		int value = tScore % 1000;
		tScore /= 1000;
		ss = stringstream();
		ss << ",";
		if (value < 100) ss << "0";
		if (value < 10) ss << "0";
		ss << value;
		ret = ss.str() + ret;
	}

	ss = stringstream();
	ss << tScore;
	ret = ss.str() + ret;
	return ret;
}

static void updateScore() {
	if (isDialogActive()) return;
	addPlayerScore(1234);
	changeMugenText(gUIHandler.mScoreTextID, scoreToString(getPlayerScore()).data());
}

static void updatePower() {
	int power = getPlayerPower();
	stringstream ss;
	ss << power / 100 << ".";
	if (power % 100 < 10) ss << "0";
	ss << power % 100;
	changeMugenText(gUIHandler.mPowerTextID, ss.str().data());
}

static void updateItemText() {
	changeMugenText(gUIHandler.mPointTextID, getPointText().data());
}

static void updateHighScore()
{
	const auto score = getPlayerScore();
	gUIHandler.mHighScore = std::max(score, gUIHandler.mHighScore);
	changeMugenText(gUIHandler.mHighScoreTextID, scoreToString(gUIHandler.mHighScore).data());
}

static void updateUIHandler(void* tData) {
	(void)tData;
	updateLives();
	updateBombs();
	updateScore();
	updatePower();
	updateItemText();
	updateHighScore();
}

ActorBlueprint getUIHandler()
{
	return makeActorBlueprint(loadUIHandler, NULL, updateUIHandler);
}

MugenSpriteFile* getUISprites()
{
	return &gUIHandler.mSprites;
}

MugenAnimations* getUIAnimations()
{
	return &gUIHandler.mAnimations;
}

