#include "enemyhandler.h"

#include <prism/blitz.h>
#include <prism/drawing.h>

#include "shothandler.h"
#include "collision.h"
#include "itemhandler.h"
#include "debug.h"
#include "player.h"
#include "uihandler.h"

typedef std::unique_ptr<ActiveEnemy> ActiveEnemyPtr;

static struct {
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	map<int, ActiveEnemyPtr> mEnemies;
	ActiveEnemy* mClosestEnemy;
} gEnemyHandler;

static void loadEnemyHandler(void* tData) {
	(void)tData;
	gEnemyHandler.mSprites = loadMugenSpriteFileWithoutPalette("level/ENEMY.sff");
	gEnemyHandler.mAnimations = loadMugenAnimationFile("level/ENEMY.air");

	gEnemyHandler.mEnemies.clear();
}

static void unloadEnemyHandler(void* tData) {
	(void)tData;	
	gEnemyHandler.mEnemies.clear();
}

static void setEnemyPosition(ActiveEnemy& tEnemy, Position tPos) {
	tPos.z = 10;
	setBlitzEntityPosition(tEnemy.mEntityID, tPos);
}

static Position getBezierPosition(vector<Position> tControlPoints, int tCurrentSection, double t);

static void updateEnemyPath(ActiveEnemy& tEnemy) {
	if (tEnemy.mCurrentPathPosition > int(tEnemy.mData->mControlPath.size()) - 2) {
		setEnemyPosition(tEnemy, tEnemy.mData->mPath.back().mPosition);
		return;
	}
	
	Position p = getBezierPosition(tEnemy.mData->mControlPath, tEnemy.mCurrentPathPosition, tEnemy.mPathSectionPosition);
	setEnemyPosition(tEnemy, p);

	tEnemy.mPathSectionPosition += 0.01 * tEnemy.mData->mSpeedFactor;
	if (tEnemy.mPathSectionPosition >= 1 - 1e-6) {
		tEnemy.mPathSectionPosition = 0;
		tEnemy.mCurrentPathPosition += 4;
	}
}

static void updateEnemyAddingShot(ActiveEnemy& tEnemy) {

	if (tEnemy.m_CurrentShotPosition == tEnemy.mData->mShots.size() - 1) return;

	auto& shot = tEnemy.mData->mShots[tEnemy.m_CurrentShotPosition + 1];

	if (tEnemy.m_DeltaTime >= shot.mDeltaTime) {
		tEnemy.m_CurrentShotPosition++;
		tEnemy.m_DeltaTime = 0;
		if (shot.m_shotFrequency.y) tEnemy.m_shotFrequencyNext = randfromInteger(shot.m_shotFrequency.x, shot.m_shotFrequency.y);
		else tEnemy.m_shotFrequencyNext = shot.m_shotFrequency.x;
		tEnemy.m_shotFrequencyNow = tEnemy.m_shotFrequencyNext == INF ? tEnemy.m_shotFrequencyNext : 0;
	}
	else {
		tEnemy.m_DeltaTime++;
	}
}

static void updateEnemyActiveShot(ActiveEnemy& tEnemy) {

	if (tEnemy.m_CurrentShotPosition == -1) return;
	auto& shot = tEnemy.mData->mShots[tEnemy.m_CurrentShotPosition];
	if (shot.mShotName.empty()) return;

	if (tEnemy.m_shotFrequencyNow >= tEnemy.m_shotFrequencyNext) {
		Position p = getBlitzEntityPosition(tEnemy.mEntityID);
		if (isInGameScreen(p)) {
			addShot(&tEnemy, p, shot.mShotName, getEnemyShotCollisionList());
		}
		if (shot.m_shotFrequency.y) tEnemy.m_shotFrequencyNext = randfromInteger(shot.m_shotFrequency.x, shot.m_shotFrequency.y);
		else tEnemy.m_shotFrequencyNext = shot.m_shotFrequency.x;
		tEnemy.m_shotFrequencyNow = 0;
	}
	else {
		tEnemy.m_shotFrequencyNow++;
	}
}

static void updateEnemyClosestPosition(ActiveEnemy& tEnemy) {
	
	if (!tEnemy.mIsInvincible && isInGameScreen(getBlitzEntityPosition(tEnemy.mEntityID)) && (!gEnemyHandler.mClosestEnemy || getBlitzEntityDistance2D(getPlayerEntity(), tEnemy.mEntityID) < vecLength2D(getBlitzEntityPosition(getPlayerEntity()) - getBlitzEntityPosition(gEnemyHandler.mClosestEnemy->mEntityID)))) {
		gEnemyHandler.mClosestEnemy = &tEnemy;
	}
}

static int getAnimationIDFromName(const std::string tName) {
	if (tName == "enemy1") return 10; // TODO: properly
	else if (tName == "enemy2") return 11;
	else if (tName == "enemyBig") return 12;
	else if (tName == "enemyTransparent") return 13;
	else return 12;
}

static void updateEnemyChangeAnimation(ActiveEnemy& tEnemy) {
	tEnemy.mChangeAnimationNow++;
	if (tEnemy.mChangeAnimationNow != 1 && tEnemy.mChangeAnimationNow >= tEnemy.mData->mChangeAnimationTime) {
		changeBlitzMugenAnimation(tEnemy.mEntityID, getAnimationIDFromName(tEnemy.mData->mChangeAnimation));
		tEnemy.mIsInvincible = 0;
		tEnemy.mChangeAnimationNow = -1;
	}
}

static int updateSingleEnemy(ActiveEnemy& tEnemy) {
	updateEnemyPath(tEnemy);
	updateEnemyAddingShot(tEnemy);
	updateEnemyActiveShot(tEnemy);
	updateEnemyClosestPosition(tEnemy);
	updateEnemyChangeAnimation(tEnemy);
	return tEnemy.mCurrentPathPosition > int(tEnemy.mData->mControlPath.size()) - 2;
	return tEnemy.mCurrentPathPosition > int(tEnemy.mData->mControlPath.size()) - 2;
}

static void updateEnemyHandler(void* tData) {
	(void)tData;
	gEnemyHandler.mClosestEnemy = nullptr;
	stl_int_map_remove_predicate(gEnemyHandler.mEnemies, updateSingleEnemy);
	
}

static int drawSingleEnemy(ActiveEnemy& tEnemy) {
	for (int i = 0; i < int(tEnemy.mData->mControlPath.size()) - 2; i+=4) {
		for (double t = 0; t < 1; t += 0.01) {
			Position p = getBezierPosition(tEnemy.mData->mControlPath, i, t);
			drawColoredPoint(p, COLOR_RED);
		}
	}

	return 0;
}

static void drawEnemyHandler(void* tData) {
	(void)tData;
	if (!gGameVars.drawPaths) return;
	stl_int_map_remove_predicate(gEnemyHandler.mEnemies, drawSingleEnemy);

}

ActorBlueprint getEnemyHandler()
{
	return makeActorBlueprint(loadEnemyHandler, unloadEnemyHandler, updateEnemyHandler, drawEnemyHandler);
}

MugenSpriteFile * getEnemySprites()
{
	return &gEnemyHandler.mSprites;
}

MugenAnimations * getEnemyAnimations()
{
	return &gEnemyHandler.mAnimations;
}

void addActiveEnemy(LevelEnemy * tEnemy)
{
	int id = stl_int_map_get_id();
	gEnemyHandler.mEnemies[id] = make_unique<ActiveEnemy>(tEnemy, id);
}

int isActiveEnemy(ActiveEnemy * tEnemy)
{
	for (auto& enemyPair : gEnemyHandler.mEnemies) {
		if (enemyPair.second.get() == tEnemy) return 1;
	}
	return 0;
}

static int isEnemy(int tID) {
	return gEnemyHandler.mEnemies.find(tID) != gEnemyHandler.mEnemies.end();
}

static int addDamageToSingleEnemy(ActiveEnemy& tEnemy, int tDamage) {
	if (tEnemy.mIsInvincible) return 0;
	tEnemy.mLifeNow -= tDamage;
	if (tEnemy.mLifeNow <= 0) {
		Position p = getBlitzEntityPosition(tEnemy.mEntityID);
		addPowerItems(p, tEnemy.mData->mPower);
		addScoreItems(p, tEnemy.mData->mScore);
		if (tEnemy.mData->mDeathShot != "" && isInGameScreen(p)) {
			addShot(&tEnemy, p, tEnemy.mData->mDeathShot, getEnemyShotCollisionList());
		}
		return 1;
	}

	return 0;
}

static void activeEnemyHitCB(void* tCaller, void* tCollisionData) {
	ActiveEnemy* enemy = (ActiveEnemy*)tCaller;
	if (!isEnemy(enemy->m_id)) return;
	if (tCollisionData == nullptr) return;
	Position p = getBlitzEntityPosition(enemy->mEntityID);
	if (!isInGameScreen(p)) return;
	if (addDamageToSingleEnemy(*enemy, getShotDamage(tCollisionData))) {
		gEnemyHandler.mEnemies.erase(enemy->m_id);
	}
}

ActiveEnemy::ActiveEnemy(LevelEnemy* tEnemy, int tID) {
	mData = tEnemy;
	m_id = tID;
	mEntityID = addBlitzEntity(tEnemy->mPath[0].mPosition);
	mCurrentPathPosition = 0;
	mPathSectionPosition = 0;
	m_CurrentShotPosition = -1;
	m_DeltaTime = 0;
	m_shotFrequencyNow = 0;
	mIsInvincible = tEnemy->mStartsInvincible;
	mChangeAnimationNow = 0;
	mLifeNow = mData->mLife;

	addBlitzMugenAnimationComponent(mEntityID, &gEnemyHandler.mSprites, &gEnemyHandler.mAnimations, getAnimationIDFromName(tEnemy->mName));
	addBlitzCollisionComponent(mEntityID);
	int collisionID = addBlitzCollisionCirc(mEntityID, getEnemyCollisionList(), makeCollisionCirc(makePosition(0, 0, 0), tEnemy->mRadius));
	addBlitzCollisionCB(mEntityID, collisionID, activeEnemyHitCB, this);
}

ActiveEnemy::~ActiveEnemy()
{
	int animation = getBlitzMugenAnimationAnimationNumber(mEntityID);
	int explodeAnimation = animation == 12 ? 15 : 14;
	int id = addMugenAnimation(getMugenAnimation(getUIAnimations(), explodeAnimation), getUISprites(), getBlitzEntityPosition(mEntityID)); 
	setMugenAnimationNoLoop(id);

	removeBlitzEntity(mEntityID);
}

static Position center(Position p1, Position p2) {

	return makePosition(
		(p1.x + p2.x) / 2,
		(p1.y + p2.y) / 2,
		(p1.z + p2.z) / 2

	);

}

vector<Position> getFullControlPointsForEnemyPath(const vector<EnemyPathPoint>& originalControls) {
	vector<Position> controlPoints;
	vector<Position> controls;

	for (size_t i = 0; i < originalControls.size(); i++) {
		if (originalControls[i].mDuration) {
			for (int j = 0; j < 5; j++) controls.push_back(originalControls[i].mPosition);
		}
		else controls.push_back(originalControls[i].mPosition);
	}

	//generate the end and control points
	for (size_t i = 1; i < controls.size() - 1; i += 2) {
		if(i == 1) controlPoints.push_back(controls[i - 1]);
		else controlPoints.push_back(center(controls[i - 1], controls[i]));
		controlPoints.push_back(controls[i]);
		controlPoints.push_back(controls[i + 1]);

		if (i + 2 < controls.size() - 1) {
			controlPoints.push_back(center(controls[i + 1], controls[i + 2]));
		}
	}
	return controlPoints;
}

void removeAllEnemies()
{
	gEnemyHandler.mEnemies.clear();
}

int getActiveEnemyAmount()
{
	return gEnemyHandler.mEnemies.size();
}

typedef struct {
	int mDamage;

} AddEnemyDamageCaller;

static int addDamageToSingleEnemyCB(AddEnemyDamageCaller* tCaller, ActiveEnemy& tEnemy) {
	return addDamageToSingleEnemy(tEnemy, tCaller->mDamage);
}

void addDamageToAllEnemies(int tDamage)
{
	AddEnemyDamageCaller caller;
	caller.mDamage = tDamage;
	stl_int_map_remove_predicate(gEnemyHandler.mEnemies, addDamageToSingleEnemyCB, &caller);
}

ActiveEnemy* getClosestEnemy()
{
	return gEnemyHandler.mClosestEnemy;
}

static double quadBezierPoint(double a0, double a1, double a2, double t) {
	return pow(1 - t, 2) * a0 + 2 * (1 - t) * t * a1 + pow(t, 2) * a2;
}

static Position quadBezier(Position p1, Position p2, Position p3, double t) {
	return makePosition(
		quadBezierPoint(p1.x, p2.x, p3.x, t),
		quadBezierPoint(p1.y, p2.y, p3.y, t),
		quadBezierPoint(p1.z, p2.z, p3.z, t));

}

static double cubicBezierPoint(double a0, double a1, double a2, double a3, double t) {
	return pow(1 - t, 3) * a0 + 3 * pow(1 - t, 2) * t * a1 + 3 * (1 - t) * pow(t, 2) * a2 + pow(t, 3) * a3;
}

static Position cubicBezier(Position p1, Position p2, Position p3, Position p4, double t) {
	return makePosition(
		cubicBezierPoint(p1.x, p2.x, p3.x, p4.x, t),
		cubicBezierPoint(p1.y, p2.y, p3.y, p4.y, t),
		cubicBezierPoint(p1.z, p2.z, p3.z, p4.z, t));
}

static Position getBezierPosition(vector<Position> tControlPoints, int tCurrentSection, double t) {
	//Generate the detailed points. 
	Position a0, a1, a2, a3;
	a0 = tControlPoints[tCurrentSection];
	a1 = tControlPoints[tCurrentSection + 1];
	a2 = tControlPoints[tCurrentSection + 2];
	if (tCurrentSection + 3 > int(tControlPoints.size()) - 1) {
		return quadBezier(a0, a1, a2, t);
	}
	else {
		a3 = tControlPoints[tCurrentSection + 3];
		return cubicBezier(a0, a1, a2, a3, t);

	}
}
