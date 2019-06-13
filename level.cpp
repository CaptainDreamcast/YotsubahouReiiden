#include "level.h"

#include <prism/blitz.h>

#include "enemyhandler.h"
#include "boss.h"
#include "debug.h"
#include "shothandler.h"
#include "levelintro.h"
#include "uihandler.h"
#include "player.h"
#include "gamescreen.h"
#include "dialoghandler.h"

#define LEVEL_DONE_Z 85

struct Section {
	vector<LevelActionPtr> mActions;
	int mCurrentAction = 0;
	int mTimeDelta = 0;
};

static struct {
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	vector<Section> mSections;

	int mCurrentSection;
	int mCurrentDeltaTime;
	int mCurrentLevel = 0;

	int mIsLevelEnding;
	int mLevelEndAnimationID;
	int mLevelEndText;
	int mLevelEndNow;
} gLevelData;

struct ClearAction : LevelAction {

	int mTime;

	ClearAction(MugenDefScriptGroup* tGroup) {
		mTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "time", 0);
	}

	virtual int isTime(int tDeltaTime) override {
		return tDeltaTime >= mTime;
	}
	virtual void handle() override {
		removeAllEnemies();
		removeEnemyBullets();
	}
};

struct IntroAction : LevelAction {

	int mTime;
	string mName;
	string mText;

	IntroAction(MugenDefScriptGroup* tGroup) {
		mTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "time", 0);
		mName = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "name", "dummy");
		mText = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "text", "text");
	}

	virtual int isTime(int tDeltaTime) override {
		return tDeltaTime >= mTime;
	}
	virtual void handle() override {
		addLevelIntro(mName, mText);
	}
};

struct IntroWaitAction : LevelAction {

	IntroWaitAction(MugenDefScriptGroup* tGroup) {
		(void)tGroup;
	}

	virtual int isTime(int tDeltaTime) override {
		return !isLevelIntroActive();
	}
	virtual void handle() override {}
};

static int isSectionGroup(MugenDefScriptGroup* tGroup) {
	return stringBeginsWithSubstringCaseIndependent(tGroup->mName.data(), "section");
}

static void handleSectionGroup(MugenDefScriptGroup* tGroup) {
	gLevelData.mSections.push_back(Section());
}

static int isEnemyGroup(MugenDefScriptGroup* tGroup) {
	return stringBeginsWithSubstringCaseIndependent(tGroup->mName.data(), "enemy ");
}

static void handleEnemyGroup(MugenDefScriptGroup* tGroup) {
	gLevelData.mSections.back().mActions.push_back(make_unique<LevelEnemy>(tGroup));
}

static int isBossGroup(MugenDefScriptGroup* tGroup) {
	return stringBeginsWithSubstringCaseIndependent(tGroup->mName.data(), "boss");
}

static void handleBossGroup(MugenDefScriptGroup* tGroup) {
	gLevelData.mSections.back().mActions.push_back(make_unique<LevelBoss>(tGroup));
}

static int isClearGroup(MugenDefScriptGroup* tGroup) {
	return stringBeginsWithSubstringCaseIndependent(tGroup->mName.data(), "clear");
}

static void handleClearGroup(MugenDefScriptGroup* tGroup) {
	gLevelData.mSections.back().mActions.push_back(make_unique<ClearAction>(tGroup));
}

static int isIntroGroup(MugenDefScriptGroup* tGroup) {
	return stringBeginsWithSubstringCaseIndependent(tGroup->mName.data(), "intro");
}

static void handleIntroGroup(MugenDefScriptGroup* tGroup) {
	gLevelData.mSections.back().mActions.push_back(make_unique<IntroAction>(tGroup));
}

static int isIntroWaitGroup(MugenDefScriptGroup* tGroup) {
	return stringBeginsWithSubstringCaseIndependent(tGroup->mName.data(), "waitintro");
}

static void handleIntroWaitGroup(MugenDefScriptGroup* tGroup) {
	gLevelData.mSections.back().mActions.push_back(make_unique<IntroWaitAction>(tGroup));
}

static void loadCurrentSection() {
	removeAllEnemies();
	gLevelData.mCurrentDeltaTime = 0;
}

static void loadLevel(void* tData) {
	MugenDefScript script;
	stringstream ss;
	ss << "level/LEVEL" << gLevelData.mCurrentLevel << ".txt";
	loadMugenDefScript(&script, ss.str().data());
	ss = stringstream();
	ss << "level/LEVEL" << gLevelData.mCurrentLevel << ".sff";
	gLevelData.mSprites = loadMugenSpriteFileWithoutPalette(ss.str());
	ss = stringstream();
	ss << "level/LEVEL" << gLevelData.mCurrentLevel << ".air";
	gLevelData.mAnimations = loadMugenAnimationFile(ss.str());

	gLevelData.mSections.clear();
	MugenDefScriptGroup* current = script.mFirstGroup;
	while (current) {
		if (isSectionGroup(current)) {
			handleSectionGroup(current);
		}
		else if (isEnemyGroup(current)) {
			handleEnemyGroup(current);
		}
		else if (isBossGroup(current)) {
			handleBossGroup(current);
		}		
		else if (isClearGroup(current)) {
			handleClearGroup(current);
		}
		else if (isIntroGroup(current)) {
			handleIntroGroup(current);
		}
		else if (isIntroWaitGroup(current)) {
			handleIntroWaitGroup(current);
		}
		current = current->mNext;
	}

	gLevelData.mIsLevelEnding = 0;
	gLevelData.mCurrentSection = 0;
	loadCurrentSection();

}

static void unloadLevel(void* tData) {
	gLevelData.mSections.clear();
}

static void updateCurrentAction() {
	Section& currentSection = gLevelData.mSections[gLevelData.mCurrentSection];

	while (true) {
		if (currentSection.mCurrentAction >= int(currentSection.mActions.size())) break;

		auto& action = *currentSection.mActions[currentSection.mCurrentAction];
		if (action.isTime(currentSection.mTimeDelta)) {
			action.handle();
			currentSection.mCurrentAction++;
			currentSection.mTimeDelta = -1;
		}
		else break;
	}	
	currentSection.mTimeDelta++;
}

static void updateSectionOver() {
	Section& currentSection = gLevelData.mSections[gLevelData.mCurrentSection];

	int areLevelActionsOver = currentSection.mCurrentAction >= int(currentSection.mActions.size());
	int areEnemiesGone = !getActiveEnemyAmount() && !hasActiveBoss();
	if (areLevelActionsOver && areEnemiesGone && !isDialogActive() && !isLevelEnding()) {
		if (gLevelData.mCurrentSection == gLevelData.mSections.size() - 1) {
			endLevel();
		}
		else {
			gLevelData.mCurrentSection++;
		}
	}
}

static void updateCurrentSection() {
	updateCurrentAction();
	updateSectionOver();
}

static void gotoNextLevel(void* tCaller) {
	(void)tCaller;
	gLevelData.mCurrentLevel++;
	setNewScreen(getGameScreen());
}

static void updateLevelEnding() {
	if (!gLevelData.mIsLevelEnding) return;

	if (gLevelData.mLevelEndNow <= 30) {
		setMugenAnimationTransparency(gLevelData.mLevelEndAnimationID, gLevelData.mLevelEndNow / 30.0);
	}
	if (gLevelData.mLevelEndNow == 30) {
		int scoreBonus = (gLevelData.mCurrentLevel + 1) * 1000000;
		stringstream ss;
		ss << "LEVEL COMPLETE BONUS: " << scoreBonus;
		int id = addMugenTextMugenStyle(ss.str().data(), getScreenPositionFromGamePosition(0.5, 0.3, LEVEL_DONE_Z), makeVector3DI(4, 0, 0));
		setMugenTextScale(id, 0.6);
		addPlayerScore(scoreBonus);
	}

	if (gLevelData.mLevelEndNow == 500) {
		addFadeOut(20, gotoNextLevel, NULL);
	}

	gLevelData.mLevelEndNow++;
}

static void updateLevel(void* tData) {
	(void)tData;
	updateCurrentSection();
	updateLevelEnding();
}

ActorBlueprint getLevelHandler()
{
	return makeActorBlueprint(loadLevel, unloadLevel, updateLevel);
}

MugenSpriteFile* getLevelSprites()
{
	return &gLevelData.mSprites;
}

MugenAnimations* getLevelAnimations()
{
	return &gLevelData.mAnimations;
}

Position getScreenPositionFromGamePosition(Position tPosition)
{
	return gGameVars.gameScreenOffset + makePosition(gGameVars.gameScreen) * tPosition;
}

Position getScreenPositionFromGamePosition(double x, double y, double z)
{
	return getScreenPositionFromGamePosition(makePosition(x, y, z));
}

int isInGameScreen(const Position& p)
{
	return p.x > gGameVars.gameScreenOffset.x && p.x < gGameVars.gameScreenOffset.x + gGameVars.gameScreen.x && p.y > gGameVars.gameScreenOffset.y && p.y < gGameVars.gameScreenOffset.y + gGameVars.gameScreen.y;
}

void endLevel()
{
	gLevelData.mLevelEndAnimationID = addMugenAnimation(getMugenAnimation(getUIAnimations(), 4), getUISprites(), getScreenPositionFromGamePosition(0.5, 0.2, LEVEL_DONE_Z));
	setMugenAnimationTransparency(gLevelData.mLevelEndAnimationID, 0);
	gLevelData.mLevelEndNow = 0;
	gLevelData.mIsLevelEnding = 1;
}

int isLevelEnding()
{
	return gLevelData.mIsLevelEnding;
}

LevelEnemy::LevelEnemy(MugenDefScriptGroup* tGroup) {
	mTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "time", 0);
	mName = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "name", "enemy1");
	mSpeedFactor = getMugenDefFloatOrDefaultAsGroup(tGroup, "speed", 1);
	mRadius = getMugenDefFloatOrDefaultAsGroup(tGroup, "radius", 1);
	mPower = getMugenDefIntegerOrDefaultAsGroup(tGroup, "power", 0);
	mScore = getMugenDefIntegerOrDefaultAsGroup(tGroup, "score", 0);
	mLife = getMugenDefIntegerOrDefaultAsGroup(tGroup, "life", 0);
	mStartsInvincible = getMugenDefIntegerOrDefaultAsGroup(tGroup, "invincible", 0);
	mChangeAnimation = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "change.name", "");
	mChangeAnimationTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "change.time", INF);
	mDeathShot = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "death.shot", "");

	mPath.clear();
	for (int i = 0; i < 100; i++) {
		stringstream ss;
		ss << "p" << i << ".pos";
		if (!isMugenDefVectorVariableAsGroup(tGroup, ss.str().data())) continue;
		Position p = getMugenDefVectorOrDefaultAsGroup(tGroup, ss.str().data(), makePosition(0,0,0));
		p = makePosition(gGameVars.gameScreenOffset) + p * makePosition(gGameVars.gameScreen);

		ss = stringstream();
		ss << "p" << i << ".duration";
		int duration = getMugenDefIntegerOrDefaultAsGroup(tGroup, ss.str().data(), 0);

		mPath.push_back(EnemyPathPoint(p, duration));
	}

	for (int i = 0; i < 100; i++) {
		stringstream ss;
		ss << "s" << i << ".time";
		if (!isMugenDefNumberVariableAsGroup(tGroup, ss.str().data())) continue;
		int deltaTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, ss.str().data(), 0);

		ss = stringstream();
		ss << "s" << i << ".name";
		string name = getSTLMugenDefStringOrDefaultAsGroup(tGroup, ss.str().data(), "");

		ss = stringstream();
		ss << "s" << i << ".frequency";
		Vector3DI shotFrequency = getMugenDefVectorIOrDefaultAsGroup(tGroup, ss.str().data(), makeVector3DI(INF, 0, 0));

		mShots.push_back(EnemyShot(deltaTime, name, shotFrequency));
	}
	mControlPath = getFullControlPointsForEnemyPath(mPath);
}

int LevelEnemy::isTime(int tDeltaTime) {
	return tDeltaTime >= mTime;
}

void LevelEnemy::handle() {
	addActiveEnemy(this);
}

EnemyShot::EnemyShot(int tDeltaTime, std::string & tName, Vector3DI t_shotFrequency)
{
	mDeltaTime = tDeltaTime;
	mShotName = tName;
	m_shotFrequency = t_shotFrequency;
}

LevelBoss::LevelBoss(MugenDefScriptGroup * tGroup)
{
	mTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "time", 0);
	mName = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "name", "dummy");
}

int LevelBoss::isTime(int tDeltaTime)
{
	return tDeltaTime >= mTime;
}

void LevelBoss::handle()
{
	addBoss(mName);
}
