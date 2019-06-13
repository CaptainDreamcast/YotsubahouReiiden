#pragma once

#include <memory>
#include <vector>

#include <prism/mugendefreader.h>
#include <prism/mugenanimationhandler.h>
#include <prism/actorhandler.h>

#define ENEMY_Z 9

struct LevelAction {

	virtual int isTime(int tDeltaTime) = 0;
	virtual void handle() = 0;
};

typedef std::unique_ptr<LevelAction> LevelActionPtr;

struct EnemyPathPoint {
	Position mPosition;
	int mDuration;
	int mAnimation;

	EnemyPathPoint(Position tPosition, int tDuration = 0) {
		tPosition.z = ENEMY_Z;
		mPosition = tPosition;
		mDuration = tDuration;
	}
};

struct EnemyShot {
	int mDeltaTime;
	std::string mShotName;
	Vector3DI m_shotFrequency;

	EnemyShot(int tDeltaTime, std::string& tName, Vector3DI t_shotFrequency);
};

struct LevelEnemy : LevelAction {

	int mTime;
	std::vector<EnemyPathPoint> mPath;
	std::vector<EnemyShot> mShots;
	std::vector<Position> mControlPath;
	std::string mName;
	double mSpeedFactor;
	double mRadius;
	int mPower;
	int mScore;
	int mStartsInvincible;
	std::string mChangeAnimation;
	int mChangeAnimationTime;
	int mLife;
	std::string mDeathShot;

	LevelEnemy(MugenDefScriptGroup* tGroup);

	virtual int isTime(int tDeltaTime) override;
	virtual void handle() override;
};

struct LevelBoss : LevelAction {

	int mTime;
	std::string mName;

	LevelBoss(MugenDefScriptGroup* tGroup);

	virtual int isTime(int tDeltaTime) override;
	virtual void handle() override;
};

ActorBlueprint getLevelHandler();

MugenSpriteFile* getLevelSprites();
MugenAnimations* getLevelAnimations();
Position getScreenPositionFromGamePosition(Position tPosition);
Position getScreenPositionFromGamePosition(double x, double y, double z);
int isInGameScreen(const Position& p);
void endLevel();
int isLevelEnding();