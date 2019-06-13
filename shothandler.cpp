#include "shothandler.h"

#include <assert.h>
#include <memory>
#include <prism/blitz.h>

#include "collision.h"
#include "debug.h"
#include "player.h"
#include "enemyhandler.h"
#include "boss.h"

#define PLAYER_SHOT_Z 25
#define ENEMY_SHOT_Z 26

static void shotCB(void* tCaller, void* tCollisionData);

struct ShotHandler {

	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	static ShotHandler* mSelf;

	struct ShotData;
	struct Shot;
	typedef int(*ShotGimmickFunction)(void* tCaller, Shot* tShot);
	struct ShotData {
		Position mOffset;
		int mHasAnimation;
		int mAnimation;
		int mDamage;
		double mCollisionRadius;
		ShotGimmickFunction mGimmick;
		vector<ShotData> mSubShots;

		ShotData() {
			assert(0);
		}

		ShotData(MugenDefScriptGroup* tGroup) {
			mOffset = makePosition(0, 0, 0);

			mHasAnimation = isMugenDefNumberVariableAsGroup(tGroup, "animation");
			if (mHasAnimation) {
				mAnimation = getMugenDefIntegerOrDefaultAsGroup(tGroup, "animation", 1);
				mDamage = getMugenDefIntegerOrDefaultAsGroup(tGroup, "damage", 1);
				mCollisionRadius = getMugenDefFloatOrDefaultAsGroup(tGroup, "radius", 1);
			}

			if (isMugenDefStringVariableAsGroup(tGroup, "gimmick")) {
				string gimmickName = getSTLMugenDefStringVariableAsGroup(tGroup, "gimmick");
				mGimmick = mSelf->getShotGimmick(gimmickName);
			}
			else {
				mGimmick = NULL;
			}

			for (int i = 0; i < 100; i++) {
				char baseName[20];
				sprintf(baseName, "subshot%d", i);

				if (isMugenDefStringVariableAsGroup(tGroup, (baseName + string(".name")).data())) {
					string name = getSTLMugenDefStringVariableAsGroup(tGroup, (baseName + string(".name")).data());
					turnStringLowercase(name);
					ShotData shotData = mSelf->getShotDataByName(name);
					shotData.mOffset = getMugenDefVectorOrDefaultAsGroup(tGroup, (baseName + string(".offset")).data(), makePosition(0, 0, 0));
					mSubShots.push_back(shotData);
				}

			}
		}
	};


	struct Shot {
		int mHasEntity;
		int mEntityID;
		int mCurrentFrame;
		void* mOwner;
		function<int(void*, Shot*)> mGimmick;
		uint8_t mUserData[100];
		int mRootID;
		int mCollisionList;
		int mDamage;

		Shot(void* tOwner, Position tPos, ShotData tData, int tCollisionList, int rootID)
			: Shot(tOwner, tPos, tData, tCollisionList, rootID, tData.mGimmick) {
		}

		Shot(void* tOwner, Position tPos, ShotData tData, int tCollisionList, int rootID, function<int(void*, Shot*)> tGimmick) {
			mRootID = rootID;
			mCurrentFrame = 0;
			tPos += tData.mOffset;
			mOwner = tOwner;
			mGimmick = tGimmick;
			mCollisionList = tCollisionList;
			mDamage = tData.mDamage;
			mHasEntity = tData.mHasAnimation;
			if (mHasEntity) {
				tPos.z = tCollisionList == getPlayerShotCollisionList() ? PLAYER_SHOT_Z : ENEMY_SHOT_Z;
				mEntityID = addBlitzEntity(tPos);
				addBlitzMugenAnimationComponent(mEntityID, &mSelf->mSprites, &mSelf->mAnimations, tData.mAnimation);
				addBlitzCollisionComponent(mEntityID);
				int collisionID = addBlitzCollisionCirc(mEntityID, tCollisionList, makeCollisionCirc(makePosition(0, 0, 0), tData.mCollisionRadius));
				addBlitzCollisionCB(mEntityID, collisionID, shotCB, this);
				setBlitzCollisionCollisionData(mEntityID, collisionID, this);
				addBlitzPhysicsComponent(mEntityID);
			}
			if (mGimmick) {
				mGimmick(mOwner, this);
			}

			for (auto& subShot : tData.mSubShots) {
				mSelf->addShot(tOwner, tPos, subShot, tCollisionList);
			}
		}

		~Shot() {
			if (mHasEntity) {
				removeBlitzEntity(mEntityID);
			}
		}
	};


	map<std::string, ShotData> mLoadedShots;
	map<std::string, void*> mGimmicks;
	map<int, std::unique_ptr<Shot>> mShots;

	ShotGimmickFunction getShotGimmick(string tName) {
		return (ShotGimmickFunction)mGimmicks[tName];
	}

	int isShotGroup(std::string tName) {
		turnStringLowercase(tName);
		return stringBeginsWithSubstring(tName.data(), "shot ");
	}

	void loadShotGroup(MugenDefScriptGroup* tGroup) {
		char shotPre[100], shotName[100];
		sscanf(tGroup->mName.data(), "%s %s", shotPre, shotName);
		turnStringLowercase(shotName);

		mLoadedShots.insert(make_pair(shotName, ShotData(tGroup)));
	}

	void loadShotsFromScript(MugenDefScript& tScript) {
		auto current = tScript.mFirstGroup;
		while (current) {
			if (isShotGroup(current->mName)) {
				loadShotGroup(current);
			}
			current = current->mNext;
		}
	}

	void addShot(void* tOwner, Position tPos, ShotHandler::ShotData tData, int tList) {
		int id = stl_int_map_get_id();
		mShots[id] = make_unique<ShotHandler::Shot>(tOwner, tPos, tData, tList, id);
	}

	void addShot(void* tOwner, Position tPos, ShotHandler::ShotData tData, int tList, function<int(void*, Shot*)> tGimmick) {
		int id = stl_int_map_get_id();
		mShots[id] = make_unique<ShotHandler::Shot>(tOwner, tPos, tData, tList, id, tGimmick);
	}

	static int yournamehereStraight(void* tCaller, ShotHandler::Shot* tShot) {
		(void)tCaller;
		if (!tShot->mCurrentFrame) {
			if (tShot->mHasEntity) addBlitzPhysicsVelocityY(tShot->mEntityID, -4);
		}

		return 0;
	}

	typedef struct {
		double mSpeed;
	} yournamehereStraightData;

	static int yournamehereStrong(void* tCaller, ShotHandler::Shot* tShot) {
		(void)tCaller;
		yournamehereStraightData* userData = (yournamehereStraightData*)tShot->mUserData;
		if (!tShot->mCurrentFrame) {
			userData->mSpeed = 4;
		}
		userData->mSpeed += 0.05;

		if (tShot->mHasEntity) setBlitzPhysicsVelocityY(tShot->mEntityID, -userData->mSpeed);
		return 0;
	}

	typedef struct {
		ActiveEnemy* mTarget;
		int mIsBoss;
	} yournamehereHomingData;

	static int yournamehereHoming(void* tCaller, ShotHandler::Shot* tShot) {
		yournamehereHomingData* userData = (yournamehereHomingData*)tShot->mUserData;
		Position shotPos = getBlitzEntityPosition(tShot->mEntityID);
		if (tShot->mCurrentFrame == 0) {
			userData->mTarget = getClosestEnemy();
			Position enemyPosition = userData->mTarget ? getBlitzEntityPosition(userData->mTarget->mEntityID) : makePosition(INF, INF, 1);
			userData->mIsBoss = 0;
			if (hasActiveBoss()) {
				Position bossPosition = getBossPosition();
				if (getDistance2D(shotPos, bossPosition) < getDistance2D(shotPos, enemyPosition)) userData->mIsBoss = 1;
			}

			if (!userData->mTarget && !userData->mIsBoss) {
				setBlitzPhysicsVelocity(tShot->mEntityID, makePosition(0, -4, 0));
			}
		}
		if (!userData->mTarget && !userData->mIsBoss) return 0;
		Position targetPos;
		if (userData->mIsBoss) {
			if (!hasActiveBoss()) {
				userData->mIsBoss = 0;
				userData->mTarget = nullptr;
				return 0;
			}
			targetPos = getBossPosition();
		}
		else {
			if (!isActiveEnemy(userData->mTarget)) {
				userData->mTarget = nullptr;
				return 0;
			}

			targetPos = getBlitzEntityPosition(userData->mTarget->mEntityID);
		}

		auto delta = targetPos - shotPos;
		delta.z = 0;
		if (vecLength(delta) < 0.2) return 0;

		delta = vecNormalize(delta);
		setBlitzPhysicsVelocity(tShot->mEntityID, 4 * delta);

		return 0;
	}

	static int enemyStraight(void* tCaller, ShotHandler::Shot* tShot) {
		(void)tCaller;
		if (!tShot->mCurrentFrame) {
			if (tShot->mHasEntity) addBlitzPhysicsVelocityY(tShot->mEntityID, 2);
		}

		return 0;
	}

	static void setShotNormalAngle(ShotHandler::Shot* tShot, int tAngleOffset, double tSpeed) {
		Position delta = makePosition(0, 1, 0);
		delta.z = 0;
		double angle = (tAngleOffset / 360.0) * 2 * M_PI;
		delta = vecRotateZ(delta, angle);
		setBlitzPhysicsVelocity(tShot->mEntityID, tSpeed * delta);
		setBlitzMugenAnimationAngle(tShot->mEntityID, 2 * M_PI - angle);
	}

	static void setShotNormalAngleBegin(ShotHandler::Shot* tShot, int tAngleOffset, double tSpeed) {
		if (!tShot->mCurrentFrame) {
			if (!tShot->mHasEntity) return;
			setShotNormalAngle(tShot, tAngleOffset, tSpeed);
		}
	}

	static int setShotAimedAngle(void* tCaller, ShotHandler::Shot* tShot, int tAngleOffset, double tSpeed = 2) {
			if (!tShot->mHasEntity) return 0;

			Position shooterPosition = getBlitzEntityPosition(tShot->mEntityID);
			Position playerPos = getBlitzEntityPosition(getPlayerEntity());
			Position delta = playerPos - shooterPosition;
			delta.z = 0;
			if (vecLength(delta) < 0.2) {
				setBlitzPhysicsVelocityY(tShot->mEntityID, tSpeed);
				return 0;
			}
			delta = vecRotateZ(delta, (tAngleOffset / 360.0) * 2 * M_PI);
			delta = vecNormalize(delta);
			setBlitzPhysicsVelocity(tShot->mEntityID, tSpeed * delta);
			setBlitzMugenAnimationAngle(tShot->mEntityID, getAngleFromDirection(delta) + M_PI / 2);

			return 180-int(((getAngleFromDirection(delta) + M_PI / 2) / (2* M_PI)) * 360);
	}

	static int setShotAimedAngleBegin(void* tCaller, ShotHandler::Shot* tShot, int tAngleOffset, double tSpeed = 2) {
		if (!tShot->mCurrentFrame) {
			return setShotAimedAngle(tCaller, tShot, tAngleOffset, tSpeed);
		}

		return 0;
	}

	static void setShotLowerHalf(void* tCaller, ShotHandler::Shot* tShot, int tAngleRange = 45, double tSpeed = 1) {
		if (!tShot->mCurrentFrame) {
			if (!tShot->mHasEntity) return;

			int angleInteger = randfromInteger(-tAngleRange, tAngleRange);
			Position delta = makePosition(0, 1, 0);
			delta.z = 0;
			double angle = (angleInteger / 360.0) * 2 * M_PI;
			delta = vecRotateZ(delta, angle);
			setBlitzPhysicsVelocity(tShot->mEntityID, tSpeed * delta);
			setBlitzMugenAnimationAngle(tShot->mEntityID, 2 * M_PI - angle);
		}
	}

	static int enemyAimed(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 0);
		return 0;
	}
	static int enemyAimed10(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 10);
		return 0;
	}
	static int enemyAimed20(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 20);
		return 0;
	}
	static int enemyAimed30(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 30);
		return 0;
	}
	static int enemyAimed40(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 40);
		return 0;
	}
	static int enemyAimed50(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 50);
		return 0;
	}
	static int enemyAimed60(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 60);
		return 0;
	}
	static int enemyAimed70(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 70);
		return 0;
	}
	static int enemyAimed80(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 80);
		return 0;
	}
	static int enemyAimed90(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 90);
		return 0;
	}
	static int enemyAimed100(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 100);
		return 0;
	}
	static int enemyAimed110(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 110);
		return 0;
	}
	static int enemyAimed120(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 120);
		return 0;
	}
	static int enemyAimed130(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 130);
		return 0;
	}
	static int enemyAimed140(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 140);
		return 0;
	}
	static int enemyAimed150(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 150);
		return 0;
	}
	static int enemyAimed160(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 160);
		return 0;
	}
	static int enemyAimed170(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 170);
		return 0;
	}
	static int enemyAimed180(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 180);
		return 0;
	}
	static int enemyAimed190(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 190);
		return 0;
	}
	static int enemyAimed200(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 200);
		return 0;
	}
	static int enemyAimed210(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 210);
		return 0;
	}
	static int enemyAimed220(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 220);
		return 0;
	}
	static int enemyAimed230(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 230);
		return 0;
	}
	static int enemyAimed240(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 240);
		return 0;
	}
	static int enemyAimed250(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 250);
		return 0;
	}
	static int enemyAimed260(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 260);
		return 0;
	}
	static int enemyAimed270(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 270);
		return 0;
	}
	static int enemyAimed280(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 280);
		return 0;
	}
	static int enemyAimed290(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 290);
		return 0;
	}
	static int enemyAimed300(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 300);
		return 0;
	}
	static int enemyAimed310(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 310);
		return 0;
	}
	static int enemyAimed320(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 320);
		return 0;
	}
	static int enemyAimed330(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 330);
		return 0;
	}
	static int enemyAimed340(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 340);
		return 0;
	}
	static int enemyAimed350(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, 350);
		return 0;
	}

	typedef struct {
		Position mPosition;
		Position mDirection;
		double mAngle;
	} LaserData;

	static int enemyLaserAimed(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		LaserData* data = (LaserData*)tShot->mUserData;

		if (tShot->mCurrentFrame == 0) {
			data->mPosition = getBlitzEntityPosition(shooter->mEntityID);
			data->mDirection = getBlitzEntityPosition(getPlayerEntity()) - data->mPosition;
			data->mDirection.z = 0;
			if (vecLength(data->mDirection) < 0.1) data->mDirection = makePosition(0, 1, 0);
			else data->mDirection = vecNormalize(data->mDirection);
		}

		function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
			if (tShot->mCurrentFrame == 40) return 1;
			else return 0;
		};
		addDynamicSubShot(tCaller, data->mPosition, "enemy_laser", getEnemyShotCollisionList(), func);

		data->mPosition += data->mDirection * 3;
		return !isInGameScreen(data->mPosition) || isPlayerBombActive();
	}

	static int enemyLaserAimedSlow(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		LaserData* data = (LaserData*)tShot->mUserData;

		if (tShot->mCurrentFrame == 0) {
			data->mPosition = getBlitzEntityPosition(shooter->mEntityID);
			data->mDirection = getBlitzEntityPosition(getPlayerEntity()) - data->mPosition;
			data->mDirection.z = 0;
			if (vecLength(data->mDirection) < 0.1) data->mDirection = makePosition(0, 1, 0);
			else data->mDirection = vecNormalize(data->mDirection);
		}

		function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
			if (tShot->mCurrentFrame == 60) return 1;
			else return 0;
		};
		addDynamicSubShot(tCaller, data->mPosition, "enemy_laser", getEnemyShotCollisionList(), func);

		data->mPosition += data->mDirection * 2;
		return !isInGameScreen(data->mPosition) || isPlayerBombActive();
	}

	static int enemyLaserStraight(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		LaserData* data = (LaserData*)tShot->mUserData;

		if (tShot->mCurrentFrame == 0) {
			data->mPosition = getBlitzEntityPosition(shooter->mEntityID);
			data->mDirection = makePosition(0, 1, 0);
		}

		function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
			if (tShot->mCurrentFrame == 40) return 1;
			else return 0;
		};
		addDynamicSubShot(tCaller, data->mPosition, "enemy_laser", getEnemyShotCollisionList(), func);

		data->mPosition += data->mDirection * 3;
		return !isInGameScreen(data->mPosition) || isPlayerBombActive();
	}

	static int enemyLowerHalf(void* tCaller, ShotHandler::Shot* tShot) {
		setShotLowerHalf(tCaller, tShot);
		return 0;
	}
	static int enemyLowerHalfSlow(void* tCaller, ShotHandler::Shot* tShot) {
		setShotLowerHalf(tCaller, tShot, 45, 0.4);
		return 0;
	}

	static int enemyRandom(void* tCaller, ShotHandler::Shot* tShot) {
		setShotNormalAngleBegin(tShot, randfromInteger(0, 359), 0.5);
		return 0;
	}

	static void addDynamicSubShot(void* tCaller, Position tPosition, const string& tName, int mCollisionList, function<int(void*, ShotHandler::Shot*)> tFunc) {

		auto shotData = mSelf->mLoadedShots[tName];
		int id = stl_int_map_get_id();
		mSelf->addShot(tCaller, tPosition, shotData, mCollisionList, tFunc);
	}

	static void addDynamicSubShot(void* tCaller, Position tPosition, const string& tName, int mCollisionList) {

		auto shotData = mSelf->mLoadedShots[tName];
		int id = stl_int_map_get_id();
		mSelf->addShot(tCaller, tPosition, shotData, mCollisionList);
	}

	static void addAngledShot(void* tCaller, Position tPosition, const string& tName, int tCollisionList, int tAngle = 0, double tSpeed = 2) {
		function<int(void*, ShotHandler::Shot*)> angleFunc = [tAngle, tSpeed](void* tCaller, ShotHandler::Shot* tShot) {
			setShotNormalAngleBegin(tShot, tAngle, tSpeed);
			return 0;
		};
		addDynamicSubShot(tCaller, tPosition, tName, tCollisionList, angleFunc);
	}

	static void addAimedShot(void* tCaller, Position tPosition, const string& tName, int tCollisionList, int tAngle = 0, double tSpeed = 2) {
		function<int(void*, ShotHandler::Shot*)> angleFunc = [tAngle, tSpeed](void* tCaller, ShotHandler::Shot* tShot) {
			setShotAimedAngleBegin(tCaller, tShot, tAngle, tSpeed);
			return 0;
		};
		addDynamicSubShot(tCaller, tPosition, tName, tCollisionList, angleFunc);
	}

	static int enemyLilyWhite(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		if (!isActiveEnemy(shooter)) return 1;
		auto p = getBlitzEntityPosition(shooter->mEntityID);

		if (tShot->mCurrentFrame % 5 == 0) {
			int type = (tShot->mCurrentFrame * 2) % 360;
			addAngledShot(tCaller, p, "enemy_mid", tShot->mCollisionList, type);

			auto type2 = 360 - type;
			if (type2 != type) {
				addAngledShot(tCaller, p, "enemy_mid", tShot->mCollisionList, type2);
			}
		}

		if (tShot->mCurrentFrame % 20 == 0) {
			addDynamicSubShot(tCaller, p, "enemy_five_aimed", tShot->mCollisionList);
		}

		return 0;
	}

	static int barney1(void* tCaller, ShotHandler::Shot* tShot) {

		auto p = getBossPosition();

		if (tShot->mCurrentFrame % 6 == 0) {
			addAngledShot(tCaller, getScreenPositionFromGamePosition(randfrom(0.0, 1.0), -0.05, p.z), "enemy_slim", tShot->mCollisionList, 0, 0.5);
		}

		if (tShot->mCurrentFrame % 20 == 0) {
			for (int i = 0; i < 5; i++) addAimedShot(tCaller, p, "enemy_mid", tShot->mCollisionList, -20 + 10 * i, 1);
		}

		return 0;
	}

	static int ack1(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 20 == 0) {
			int amount = 20;
			Position p = getBossPosition();
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount);
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					setShotNormalAngleBegin(tShot, angle, 1);

					if (tShot->mCurrentFrame % 60 == 0) {
						setShotNormalAngle(tShot, randfromInteger(angle - 10, angle + 10), 1);
					}

					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int enemyFiveAimedSlow(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		auto p = getBlitzEntityPosition(shooter->mEntityID);

		int amount = 5;
		for (int i = 0; i < amount; i++) {
			int angle = -20 + i * 10;
			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotAimedAngleBegin(tCaller, tShot, angle, 0.5);
				return 0;
			};

			addDynamicSubShot(tCaller, p, "enemy_mid", getEnemyShotCollisionList(), func);
		}
		return 1;
	}

	static int enemyCircleMid(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		auto p = getBlitzEntityPosition(shooter->mEntityID);

		int amount = 20;
		for (int i = 0; i < amount; i++) {
			int angle = i * (360 / amount);
			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle, 1);

				return 0;
			};

			addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
		}

		return 1;
	}

	static int enemySlightDownLeft(void* tCaller, ShotHandler::Shot* tShot) {
		setShotNormalAngleBegin(tShot, 80, 1);
		return 0;
	}

	static int enemySlightDownRight(void* tCaller, ShotHandler::Shot* tShot) {
		setShotNormalAngleBegin(tShot, -80, 1);
		return 0;
	}

	typedef struct {
		double speed;
		int angle;

	} AusNS1Data;

	static int enemyPCB(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		auto p = getBlitzEntityPosition(shooter->mEntityID);

	
		function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				setShotAimedAngleBegin(tCaller, tShot, 0, 1);
				return 0;
		};
		addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);

		func = [](void* tCaller, ShotHandler::Shot* tShot) {
			AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
			if (tShot->mCurrentFrame == 0) {
				data->angle = 0;
			}

			if (tShot->mCurrentFrame % 60 == 30) {
				data->angle--;
			}
			else {
				data->angle++;
			}

			setShotNormalAngle(tShot, data->angle, 1);

			return 0;
		};
		addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);

		func = [](void* tCaller, ShotHandler::Shot* tShot) {
			AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
			if (tShot->mCurrentFrame == 0) {
				data->angle = 0;
			}

			if (tShot->mCurrentFrame % 60 < 30) {
				data->angle++;
			}
			else {
				data->angle--;
			}

			setShotAimedAngle(tCaller, tShot, data->angle, 1);

			return tShot->mCurrentFrame > 400;
		};
		addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);

		return 1;
	}

	static int accMid(void* tCaller, ShotHandler::Shot* tShot) {

		if (tShot->mCurrentFrame % 10 == 0) {
			Position basePosition = getBossPosition();
			int amount = 20;
			for (int i = 0; i < 20; i++) {

				int angle = int(i * (360.0 / amount)) + tShot->mCurrentFrame;
				Position offset = vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);
				Position p = basePosition + offset;

				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

					if (tShot->mCurrentFrame % 60 == 0) {
						data->angle = angle;
					}
					else if (tShot->mCurrentFrame % 60 == 30) {
						data->angle = angle + 30;
					}

					setShotNormalAngle(tShot, data->angle, 1);

					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int accNS1(void* tCaller, ShotHandler::Shot* tShot) {

		if (tShot->mCurrentFrame % 300 < 200) {
			Position basePosition = getBossPosition();
			if (tShot->mCurrentFrame % 5 == 0) {
				int amount = 20;
				for (int i = 0; i < 20; i++) {

					int angle = int(i * (360.0 / amount)) + tShot->mCurrentFrame / 5;
					Position offset = vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);
					Position p = basePosition + offset;

					function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
						AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

						if (tShot->mCurrentFrame % 60 == 0) {
							data->angle = angle;
						}
						else if (tShot->mCurrentFrame % 60 == 30) {
							data->angle = angle + 15;
						}

						setShotNormalAngle(tShot, data->angle, 1);

						return 0;
					};

					addDynamicSubShot(tCaller, p, "enemy_slim", getEnemyShotCollisionList(), func);
				}
			}

			if (tShot->mCurrentFrame % 2 == 0) {
				mSelf->addShot(tCaller, basePosition, mSelf->mLoadedShots["enemy_slim_lower_half"], getEnemyShotCollisionList());
			}
		}


		return 0;
	}

	typedef struct {
		int mHasChanged;
		int angle;

	} AccS1Data;

	static int accS1(void* tCaller, ShotHandler::Shot* tShot) {
		Position basePosition = getBossPosition();
		if (tShot->mCurrentFrame % 2 == 0) {
			int angle = tShot->mCurrentFrame;
			Position offset = vecRotateZ(makePosition(0, 20, 0), (angle / 360.0) * 2 * M_PI);
			Position p = basePosition + offset;

			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				AccS1Data* data = (AccS1Data*)tShot->mUserData;

				if (tShot->mCurrentFrame == 0) {
					data->angle = angle + 90;
					data->mHasChanged = 0;
				}


				
				setShotNormalAngle(tShot, data->angle, 3);

				if (!data->mHasChanged) {
					data->angle += 7;

					if (tShot->mCurrentFrame > 200 && randfrom(0, 1) < 0.02 && data->angle%360 > 45 && data->angle % 360 < 135) {
						data->angle -= 90;
						data->mHasChanged = 1;

					}
				}

				return 0;
			};

			addDynamicSubShot(tCaller, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		return 0;
	}

	static int accNS2(void* tCaller, ShotHandler::Shot* tShot) {

		if (tShot->mCurrentFrame % 300 < 200) {
			Position basePosition = getBossPosition();
			if (tShot->mCurrentFrame % 10 == 0) {
				int amount = 20;
				for (int i = 0; i < 20; i++) {

					int angle = int(i * (360.0 / amount)) + tShot->mCurrentFrame / 5;
					Position offset = vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);
					Position p = basePosition + offset;

					function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
						AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

						if (tShot->mCurrentFrame % 60 == 0) {
							data->angle = angle;
						}
						else if (tShot->mCurrentFrame % 60 == 30) {
							data->angle = angle + 45;
						}

						setShotNormalAngle(tShot, data->angle, 1);

						return 0;
					};

					addDynamicSubShot(tCaller, p, "enemy_slim", getEnemyShotCollisionList(), func);
				}
			}
		}
		return 0;
	}
	
	static int accS2(void* tCaller, ShotHandler::Shot* tShot) {
		Position basePosition = getBossPosition();
		if (tShot->mCurrentFrame % 1 == 0) {
			int angle = randfromInteger(-80, 80);
			Position p = basePosition;

			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				AccS1Data* data = (AccS1Data*)tShot->mUserData;
				if (tShot->mCurrentFrame == 0) {
					data->angle = angle;
				}

				setShotNormalAngleBegin(tShot, angle, 1);

				Position* p = getBlitzEntityPositionReference(tShot->mEntityID);
				Velocity* v = getBlitzPhysicsVelocityReference(tShot->mEntityID);

				if (p->y < 0) {
					data->angle = data->angle + 180;
					setShotNormalAngle(tShot, data->angle, 1);
				}
				if (p->x < gGameVars.gameScreenOffset.x && v->x < 0) {
					data->angle = data->angle - 90;
					setShotNormalAngle(tShot, data->angle, 1);
				}
				if (p->x > gGameVars.gameScreenOffset.x + gGameVars.gameScreen.x && v->x > 0) {
					data->angle = data->angle + 90;
					setShotNormalAngle(tShot, data->angle, 1);
				}

				return 0;
			};

			addDynamicSubShot(tCaller, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		if (tShot->mCurrentFrame % 1 == 0) {
			int angle = randfromInteger(0, 360);
			Position p = basePosition;

			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				AccS1Data* data = (AccS1Data*)tShot->mUserData;
				if (tShot->mCurrentFrame == 0) {
					data->angle = angle;
				}

				setShotNormalAngleBegin(tShot, angle, 1);

				Position* p = getBlitzEntityPositionReference(tShot->mEntityID);
				Velocity* v = getBlitzPhysicsVelocityReference(tShot->mEntityID);

				if (p->y < 0) {
					data->angle = data->angle + 180;
					setShotNormalAngle(tShot, data->angle, 1);
				}
				if (p->x < gGameVars.gameScreenOffset.x && v->x < 0) {
					data->angle = data->angle - 90;
					setShotNormalAngle(tShot, data->angle, 1);
				}
				if (p->x > gGameVars.gameScreenOffset.x + gGameVars.gameScreen.x && v->x > 0) {
					data->angle = data->angle + 90;
					setShotNormalAngle(tShot, data->angle, 1);
				}

				return 0;
			};

			addDynamicSubShot(tCaller, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		return 0;
	}

	static int woaznNS(void* tCaller, ShotHandler::Shot* tShot) {
		Position basePosition = getBossPosition();
		if (tShot->mCurrentFrame % 10 == 0) {
			int amount = 20;
			for (int i = 0; i < amount; i++) {

				int angle = int((360.0 / amount) * i);
				Position p = basePosition + vecRotateZ(makePosition(10, 0, 0), (angle / 360.0) * 2 * M_PI);

				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					AccS1Data* data = (AccS1Data*)tShot->mUserData;
					if (tShot->mCurrentFrame == 0) {
						data->angle = angle;
					}

					setShotNormalAngle(tShot, data->angle, 1);
					
					if (tShot->mCurrentFrame < 100) {
						data->angle += 3;
					}
					else if(tShot->mCurrentFrame < 300) {
						data->angle += 1;
					}
					else if(tShot->mCurrentFrame == 300){
						data->angle = randfromInteger(0, 360);
					}

					return 0;
				};

				addDynamicSubShot(tCaller, p, "enemy_slim", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int woaznS(void* tCaller, ShotHandler::Shot* tShot) {
		Position p = getBossPosition();
		int angle = randfromInteger(-60, 60);

		function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
			setShotNormalAngleBegin(tShot, angle, 1);

			Position p = getBlitzEntityPosition(tShot->mEntityID);
			auto testPos = getScreenPositionFromGamePosition(makePosition(0, 1.0, 0));
			if (p.y > testPos.y) {
				function<int(void*, ShotHandler::Shot*)> func2 = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					setShotNormalAngleBegin(tShot, 0, 0.5);
					return 0;
				};

				p.y = getScreenPositionFromGamePosition(makePosition(0, 0, 0)).y;
				addDynamicSubShot(tCaller, p, "enemy_slim", getEnemyShotCollisionList(), func2);
				return 1;
			}

			return 0;
		};

		addDynamicSubShot(tCaller, p, "enemy_mid", getEnemyShotCollisionList(), func);

		return 0;
	}

	static int ausNS1(void* tCaller, ShotHandler::Shot* tShot) {

		Position basePosition = getBossPosition();
		{
			int angle = int(tShot->mCurrentFrame * 3.6);
			Position offset = vecRotateZ(makePosition(0, 5, 0), (angle / 360.0) * 2 * M_PI);
			Position p = basePosition + offset;

			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

				if (tShot->mCurrentFrame == 0) {
					data->angle = angle;
					data->speed = 0;
				}

				setShotNormalAngle(tShot, data->angle, data->speed);

				data->speed += 0.01;

				return 0;
			};

			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		if (tShot->mCurrentFrame > 300) {
			int angle = -int(tShot->mCurrentFrame * 5);
			Position offset = vecRotateZ(makePosition(0, 5, 0), (angle / 360.0) * 2 * M_PI);
			Position p = basePosition + offset;

			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

				if (tShot->mCurrentFrame == 0) {
					data->angle = angle;
					data->speed = 0;
				}

				setShotNormalAngle(tShot, data->angle, data->speed);

				data->speed += 0.01;

				return 0;
			};

			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		return 0;
	}

	static int ausS1(void* tCaller, ShotHandler::Shot* tShot) {
		Position basePosition = getBossPosition();
		if (tShot->mCurrentFrame % 360 < 300 && tShot->mCurrentFrame % 2 == 0) {
			int amount = 10;
			for (int i = 0; i < amount; i++) {

				int angle = int((360.0 / amount) * i);// + (tShot->mCurrentFrame / 10);
				Position p = basePosition + vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);

				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					setShotAimedAngleBegin(tCaller, tShot, angle, 2);
					return 0;
				};

				addDynamicSubShot(tCaller, p, "enemy_mid", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int ausNS2(void* tCaller, ShotHandler::Shot* tShot) {
		Position basePosition = getBossPosition();
		{
			int angle = int(tShot->mCurrentFrame * 3.6);
			Position offset = vecRotateZ(makePosition(0, 5, 0), (angle / 360.0) * 2 * M_PI);
			Position p = basePosition + offset;

			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

				if (tShot->mCurrentFrame == 0) {
					data->angle = angle;
					data->speed = 0;
				}

				setShotNormalAngle(tShot, data->angle, data->speed);

				data->speed += 0.01;

				return 0;
			};

			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		{
			int angle = -int(tShot->mCurrentFrame * 5);
			Position offset = vecRotateZ(makePosition(0, 5, 0), (angle / 360.0) * 2 * M_PI);
			Position p = basePosition + offset;

			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

				if (tShot->mCurrentFrame == 0) {
					data->angle = angle;
					data->speed = 0;
				}

				setShotNormalAngle(tShot, data->angle, data->speed);

				data->speed += 0.01;

				return 0;
			};

			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		if (tShot->mCurrentFrame > 300) {
			int angle = int(tShot->mCurrentFrame * 7.3);
			Position offset = vecRotateZ(makePosition(0, 5, 0), (angle / 360.0) * 2 * M_PI);
			Position p = basePosition + offset;

			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

				if (tShot->mCurrentFrame == 0) {
					data->angle = angle;
					data->speed = 0;
				}

				setShotNormalAngle(tShot, data->angle, data->speed);

				data->speed += 0.01;

				return 0;
			};

			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		return 0;
	}

	typedef struct {
		int isDisappearing;

	} AusS2Data;

	static int ausS2(void* tCaller, ShotHandler::Shot* tShot) {
		Position basePosition = getBossPosition();
		if (tShot->mCurrentFrame % 50 == 0) {
			int amount = 120;
			int offset = randfromInteger(0, 359);
			for (int i = 0; i < amount; i++) {

				int angle = int((360.0 / amount) * i) + offset;
				Position p = basePosition + vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);

				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					AusS2Data* data = (AusS2Data*)tShot->mUserData;
					if (tShot->mCurrentFrame == 0) {
						data->isDisappearing = randfromInteger(0, 4);
					}

					if (data->isDisappearing == 0) {
						int first = 60;
						int last = 120;

						if (tShot->mCurrentFrame == first) {
							removeAllBlitzCollisions(tShot->mEntityID);
						}
						if (tShot->mCurrentFrame >= first && tShot->mCurrentFrame <= last) {
							double t = (tShot->mCurrentFrame - first) / double(last - first);
							setBlitzMugenAnimationColor(tShot->mEntityID, 1 - t, 1 - t, 1 - t);
							setBlitzMugenAnimationTransparency(tShot->mEntityID, 1 - t);
						}
					}

					setShotNormalAngleBegin(tShot, angle, 1);
					return 0;
				};

				addDynamicSubShot(tCaller, p, "enemy_mid", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int ausNS3(void* tCaller, ShotHandler::Shot* tShot) {

		Position basePosition = getBossPosition();
		{
			int angle = int(tShot->mCurrentFrame * 3.6);
			Position offset = vecRotateZ(makePosition(0, 5, 0), (angle / 360.0) * 2 * M_PI);
			Position p = basePosition + offset;

			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

				if (tShot->mCurrentFrame == 0) {
					data->angle = angle;
					data->speed = 0;
				}

				setShotNormalAngle(tShot, data->angle, data->speed);

				data->speed += 0.0025;

				return 0;
			};

			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		{
			int angle = -int(tShot->mCurrentFrame * 5);
			Position offset = vecRotateZ(makePosition(0, 5, 0), (angle / 360.0) * 2 * M_PI);
			Position p = basePosition + offset;

			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

				if (tShot->mCurrentFrame == 0) {
					data->angle = angle;
					data->speed = 0;
				}

				setShotNormalAngle(tShot, data->angle, data->speed);

				data->speed += 0.0025;

				return 0;
			};

			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		if(tShot->mCurrentFrame % 40 == 0){
			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngle(tShot, 180, 0.2);
				return 0;
			};
			auto range = std::min(tShot->mCurrentFrame / 5, 180);
			double left = gGameVars.gameScreenOffset.x;
			double right = gGameVars.gameScreenOffset.x + gGameVars.gameScreen.x;
			Position p = makePosition(randfrom(left, left + range), gGameVars.gameScreenOffset.y + gGameVars.gameScreen.y, 0);
			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
			p = makePosition(randfrom(right - range, right), gGameVars.gameScreenOffset.y + gGameVars.gameScreen.y, 0);
			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		return 0;
	}

	static int ausS31(void* tCaller, ShotHandler::Shot* tShot) {
		function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {		
			int angle = randfromInteger(-45, 45);
			setShotNormalAngleBegin(tShot, angle, randfrom(0.5, 2));
			return 0;
		};
		Position p = getBossPosition();
		if (randfromInteger(0, 1)) {
			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
		}
		else {
			addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
		}

		return 1;
	}
	static int ausS33(void* tCaller, ShotHandler::Shot* tShot) {
		Position p = getBossPosition();

		for(int i = 0; i < 6; i++) addAimedShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), 0, 2 - 0.25 * i);

		return 1;
	}

	static int ausS34(void* tCaller, ShotHandler::Shot* tShot) {
		Position p = getBossPosition();

		
			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
				if (!tShot->mCurrentFrame) {
					data->speed = randfrom(-3, -2);
				}
				setShotNormalAngleBegin(tShot, randfromInteger(-75, 75), data->speed);
				addBlitzPhysicsVelocityY(tShot->mEntityID, 0.1);

				return 0;
			};
			for (int i = 0; i < 50; i++) addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
		

		return 1;
	}

	static int ausS4(void* tCaller, ShotHandler::Shot* tShot) {
		AusNS1Data* data = (AusNS1Data*)tShot->mUserData;

		function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
			AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
			if (!tShot->mCurrentFrame) {
				data->angle = -30;
			}
			else if (tShot->mCurrentFrame % 120 <= 60) {
				data->angle++;
			}
			else {
				data->angle--;
			}
			setShotNormalAngle(tShot, data->angle, 2);

			auto p = getBlitzEntityPosition(tShot->mEntityID);
			if (p.y > gGameVars.gameScreenOffset.y + gGameVars.gameScreen.y) {
				function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
					AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
					if (!tShot->mCurrentFrame) {
						data->speed = randfrom(-2, -1.5);
					}
					setShotNormalAngleBegin(tShot, randfromInteger(-75, 75), data->speed);
					addBlitzPhysicsVelocityY(tShot->mEntityID, 0.1);

					return 0;
				};
				for (int i = 0; i < 3; i++) addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
				return 1;
			}

			return 0;
		};

		if (!tShot->mCurrentFrame) {
			data->speed = randfrom(-5, 10);
		}
		if (tShot->mCurrentFrame % 10 == 0) {
			int amount = 10;
			for (int i = 0; i < amount; i++) {
				Position p = makePosition(gGameVars.gameScreenOffset.x + (gGameVars.gameScreen.x / ((double)amount) * i) + data->speed, gGameVars.gameScreenOffset.y - 5, 0);
				addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
			}
		}


		return tShot->mCurrentFrame > 300;
	}

	static int shiiNS1(void* tCaller, ShotHandler::Shot* tShot) {

		int amount = 90;
		int step = tShot->mCurrentFrame / 300;

		int now = tShot->mCurrentFrame % 300;
		int nowStage = now / 15;
		if (now % 15 == 0 && step > nowStage - 3) {

			double diff = (360.0 / amount);
			double offset = (nowStage % 2 == 0) ? (diff / 2) : 0;
			Position basePosition = getBossPosition();
				for (int i = 0; i < amount; i++) {
					int angle = int(diff * i + offset);
					Position p = basePosition + vecRotateZ(makePosition(10, 0, 0), (angle / 360.0) * 2 * M_PI);
					function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
						setShotNormalAngleBegin(tShot, angle, 1);
						return 0;
					};

					addDynamicSubShot(tCaller, p, "enemy_mid", getEnemyShotCollisionList(), func);
				}
		}

		return 0;
	}

	static int shiiS1(void* tCaller, ShotHandler::Shot* tShot) {
		return 1;
	}

	static int shiiNS2(void* tCaller, ShotHandler::Shot* tShot) {

		if (tShot->mCurrentFrame % 10 == 0) {
			Position leftTop = getScreenPositionFromGamePosition(0.1, 0.1, 0);
			Position rightBottom = getScreenPositionFromGamePosition(0.9, 0.5, 0);
			Position p = randPositionInGeoRectangle(makeGeoRectangle(leftTop.x, leftTop.y, rightBottom.x - leftTop.x, rightBottom.y - leftTop.y));

			double baseSpeed = randfrom(0.3, 3);
			for (int i = 0; i < 20; i++) {
				std::string name = (randfromInteger(0, 1)) ? "enemy_mid" : "enemy_slim";
				double speed = randfrom(baseSpeed - 0.2, baseSpeed + 0.2);
				Position finalPos = p + randPositionInGeoRectangle(makeGeoRectangle(-10, -10, 20, 20));
				addAimedShot(NULL, finalPos, name, getEnemyShotCollisionList(), 0, speed);
			}
		}
		return 0;
	}

	static int shiiS2(void* tCaller, ShotHandler::Shot* tShot) {
		Position basePosition = getBossPosition();

		if (tShot->mCurrentFrame % 300 <= 60 && tShot->mCurrentFrame % 20 == 0) {
			int amount = 70;
			double diff = (360.0 / amount);
			int offset = randfromInteger(0, 359);
			for (int i = 0; i < amount; i++) {
				int angle = int((360.0 / amount) * i + offset);
				Position p = basePosition + vecRotateZ(makePosition(10, 0, 0), (angle / 360.0) * 2 * M_PI);
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
					if (!tShot->mCurrentFrame) {
						data->angle = angle;
					}

					if (tShot->mCurrentFrame % 3 == 0) {
						data->angle++;
					}
					setShotNormalAngle(tShot, data->angle, 1.5);
					return 0;
				};

				addDynamicSubShot(tCaller, p, "enemy_mid", getEnemyShotCollisionList(), func);
			}
		}
		if (tShot->mCurrentFrame % 300 >= 100 && tShot->mCurrentFrame % 300 <= 120) {
			double baseSpeed = randfrom(0.3, 3);
			for (int i = 0; i < 20; i++) {
				std::string name = (randfromInteger(0, 1)) ? "enemy_mid" : "enemy_slim";
				double speed = randfrom(baseSpeed - 0.2, baseSpeed + 0.2);
				Position finalPos = basePosition + randPositionInGeoRectangle(makeGeoRectangle(-10, -10, 20, 20));
				addAimedShot(NULL, finalPos, name, getEnemyShotCollisionList(), randfromInteger(-2, 2), speed);
			}
		}


		if (tShot->mCurrentFrame % 300 == 180) {
			int amount = 10;
			double diff = (360.0 / amount);
			int offset = randfromInteger(0, 359);
			for (int i = 0; i < amount; i++) {
				int angle = int((360.0 / amount) * i + offset);
				Position direction = vecRotateZ(makePosition(0, 1, 0), (angle / 360.0) * 2 * M_PI);
				Position p = basePosition + (direction * 10);
				function<int(void*, ShotHandler::Shot*)> func = [angle, basePosition, direction](void* tCaller, ShotHandler::Shot* tShot) {
					LaserData* data = (LaserData*)tShot->mUserData;

					if (tShot->mCurrentFrame == 0) {
						data->mPosition = basePosition;
						data->mDirection = direction;
						data->mDirection.z = 0;
						data->mAngle = 0;
					}

					function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
						if (tShot->mCurrentFrame == 60) return 1;
						else return 0;
					};
					addDynamicSubShot(tCaller, data->mPosition, "enemy_laser", getEnemyShotCollisionList(), func);

					if (tShot->mCurrentFrame % 60 < 30) {
						data->mAngle += 0.05;
					}
					else {
						data->mAngle -= 0.05;
					}

					data->mPosition += vecRotateZ(data->mDirection, data->mAngle) * 2;
					return !isInGameScreen(data->mPosition) || isPlayerBombActive();
				};

				addDynamicSubShot(tCaller, p, "enemy_laser", getEnemyShotCollisionList(), func);
			}
		}




		

		return 0;
	}

	static int shiiNS3(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 30 == 0) {
			Position basePosition = getBossPosition();
			double speed;
			if (tShot->mCurrentFrame % 90 == 0) speed = 2;
			else if (tShot->mCurrentFrame % 90 == 0) speed = 1.5;
			else speed = 1;
			int amount = 60;
			double diff = (360.0 / amount);
			int offset = randfromInteger(0, 359);
			for (int i = 0; i < amount; i++) {
				int angle = int((360.0 / amount) * i + offset);
				Position p = basePosition + vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);
				addAimedShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), angle, speed);
			}
		}

		return 0;
	}

	typedef struct {
		Position mPosition;
		Position mDirection;
		double mAngle;
		int mHasEarthQuake;
	} LaserShakeData;

	static int shiiS3(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 120 == 0) {
			Position basePosition = getBossPosition();
				function<int(void*, ShotHandler::Shot*)> func = [basePosition](void* tCaller, ShotHandler::Shot* tShot) {
					LaserShakeData* data = (LaserShakeData*)tShot->mUserData;

					if (tShot->mCurrentFrame == 0) {
						data->mPosition = basePosition;

						data->mAngle = ((setShotAimedAngleBegin(tCaller, tShot, 0, 0) - 45) / 360.0) * 2 * M_PI;
						data->mDirection = vecRotateZ(makePosition(0, 1, 0), 0);
						data->mDirection.z = 0;
						data->mHasEarthQuake = 0;
					}

					function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
						if (tShot->mCurrentFrame == 60) return 1;
						else return 0;
					};
					addDynamicSubShot(tCaller, data->mPosition, "enemy_laser", getEnemyShotCollisionList(), func);

					if (tShot->mCurrentFrame % 60 < 30) {
						data->mAngle += 0.05;
					}
					else {
						data->mAngle -= 0.05;
					}

					data->mPosition += vecRotateZ(data->mDirection, data->mAngle) * 2;
					if (!data->mHasEarthQuake && !isInGameScreen(data->mPosition)) {
					
						int amount = 100;
						for (int i = 0; i < 100; i++) {
							double x = randfrom(0, 1);
							double speedDelta = randfrom(0.01, 0.03);

							function<int(void*, ShotHandler::Shot*)> funcInternal = [speedDelta](void* tCaller, ShotHandler::Shot* tShot) {
								AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
								if (!tShot->mCurrentFrame) {
									data->speed = 0;
								}

								data->speed += speedDelta;
								setShotNormalAngle(tShot, 0, data->speed);

								return 0;
							};
							addDynamicSubShot(tCaller, getScreenPositionFromGamePosition(x, -0.05, 0), "enemy_mid", getEnemyShotCollisionList(), funcInternal);
						}

						data->mHasEarthQuake = 0;
					}

					return !isInGameScreen(data->mPosition) || isPlayerBombActive();
				};

				addDynamicSubShot(tCaller, basePosition, "enemy_laser", getEnemyShotCollisionList(), func);
		}
		return 0;
	}

	static int shiiNS4(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 30 == 0) {
			Position basePosition = getBossPosition();
			int amount = 60;
			double diff = (360.0 / amount);
			int offset = randfromInteger(0, 359);
			for (int i = 0; i < amount; i++) {
				int angle = int((360.0 / amount) * i + offset);
				Position p = basePosition + vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					if (tShot->mCurrentFrame == 0) {
						setShotNormalAngle(tShot, angle, 3);
					}
					else if (tShot->mCurrentFrame == 30) {
						setShotNormalAngle(tShot, angle, 0);
					}
					if (tShot->mCurrentFrame == 50) {
						setShotNormalAngle(tShot, angle, 3);
					}
					return 0;
				};
				addDynamicSubShot(tCaller, p, "enemy_mid", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int shiiS4(void* tCaller, ShotHandler::Shot* tShot) {
		return 1;
	}

	static int shiiS5(void* tCaller, ShotHandler::Shot* tShot) {
		return 1;
	}

	void loadShotGimmicks() {
		mGimmicks["yournamehereStraight"] = yournamehereStraight;
		mGimmicks["yournamehereStrong"] = yournamehereStrong;
		mGimmicks["yournamehereHoming"] = yournamehereHoming;
		mGimmicks["enemyStraight"] = enemyStraight;
		mGimmicks["enemyAimed"] = enemyAimed;
		mGimmicks["enemyAimedPlus10"] = enemyAimed10;
		mGimmicks["enemyAimedPlus20"] = enemyAimed20;
		mGimmicks["enemyAimedPlus30"] = enemyAimed30;
		mGimmicks["enemyAimedPlus40"] = enemyAimed40;
		mGimmicks["enemyAimedPlus50"] = enemyAimed50;
		mGimmicks["enemyAimedPlus60"] = enemyAimed60;
		mGimmicks["enemyAimedPlus70"] = enemyAimed70;
		mGimmicks["enemyAimedPlus80"] = enemyAimed80;
		mGimmicks["enemyAimedPlus90"] = enemyAimed90;
		mGimmicks["enemyAimedPlus100"] = enemyAimed100;
		mGimmicks["enemyAimedPlus110"] = enemyAimed110;
		mGimmicks["enemyAimedPlus120"] = enemyAimed120;
		mGimmicks["enemyAimedPlus130"] = enemyAimed130;
		mGimmicks["enemyAimedPlus140"] = enemyAimed140;
		mGimmicks["enemyAimedPlus150"] = enemyAimed150;
		mGimmicks["enemyAimedPlus160"] = enemyAimed160;
		mGimmicks["enemyAimedPlus170"] = enemyAimed170;
		mGimmicks["enemyAimedPlus180"] = enemyAimed180;
		mGimmicks["enemyAimedPlus190"] = enemyAimed190;
		mGimmicks["enemyAimedPlus200"] = enemyAimed200;
		mGimmicks["enemyAimedPlus210"] = enemyAimed210;
		mGimmicks["enemyAimedPlus220"] = enemyAimed220;
		mGimmicks["enemyAimedPlus230"] = enemyAimed230;
		mGimmicks["enemyAimedPlus240"] = enemyAimed240;
		mGimmicks["enemyAimedPlus250"] = enemyAimed250;
		mGimmicks["enemyAimedPlus260"] = enemyAimed260;
		mGimmicks["enemyAimedPlus270"] = enemyAimed270;
		mGimmicks["enemyAimedPlus280"] = enemyAimed280;
		mGimmicks["enemyAimedPlus290"] = enemyAimed290;
		mGimmicks["enemyAimedPlus300"] = enemyAimed300;
		mGimmicks["enemyAimedPlus310"] = enemyAimed310;
		mGimmicks["enemyAimedPlus320"] = enemyAimed320;
		mGimmicks["enemyAimedPlus330"] = enemyAimed330;
		mGimmicks["enemyAimedPlus340"] = enemyAimed340;
		mGimmicks["enemyAimedPlus350"] = enemyAimed350;
		mGimmicks["enemyLaserAimed"] = enemyLaserAimed;
		mGimmicks["enemyLaserAimedSlow"] = enemyLaserAimedSlow;
		mGimmicks["enemyLaserStraight"] = enemyLaserStraight;
		mGimmicks["enemyLowerHalf"] = enemyLowerHalf;
		mGimmicks["enemyLowerHalfSlow"] = enemyLowerHalfSlow;
		mGimmicks["enemyRandom"] = enemyRandom;
		mGimmicks["enemyLilyWhite"] = enemyLilyWhite;
		mGimmicks["barney1"] = barney1;
		mGimmicks["ack1"] = ack1;
		mGimmicks["enemyFiveAimedSlow"] = enemyFiveAimedSlow;
		mGimmicks["enemyCircleMid"] = enemyCircleMid;
		mGimmicks["enemyPCB"] = enemyPCB;
		mGimmicks["enemySlightDownLeft"] = enemySlightDownLeft;
		mGimmicks["enemySlightDownRight"] = enemySlightDownRight;
		mGimmicks["accMid"] = accMid;
		mGimmicks["accNS1"] = accNS1;
		mGimmicks["accS1"] = accS1;
		mGimmicks["accNS2"] = accNS2;
		mGimmicks["accS2"] = accS2;
		mGimmicks["woaznNS"] = woaznNS;
		mGimmicks["woaznS"] = woaznS;
		mGimmicks["ausNS1"] = ausNS1;
		mGimmicks["ausS1"] = ausS1;
		mGimmicks["ausNS2"] = ausNS2;
		mGimmicks["ausS2"] = ausS2;
		mGimmicks["ausNS3"] = ausNS3;
		mGimmicks["ausS31"] = ausS31;
		mGimmicks["ausS33"] = ausS33;
		mGimmicks["ausS34"] = ausS34;
		mGimmicks["ausS4"] = ausS4;
		mGimmicks["shiiNS1"] = shiiNS1;
		mGimmicks["shiiS1"] = shiiS1;
		mGimmicks["shiiNS2"] = shiiNS2;
		mGimmicks["shiiS2"] = shiiS2;
		mGimmicks["shiiNS3"] = shiiNS3;
		mGimmicks["shiiS3"] = shiiS3;
		mGimmicks["shiiNS4"] = shiiNS4;
		mGimmicks["shiiS4"] = shiiS4;

	}

	ShotHandler() {
		mSelf = this;
		mSprites = loadMugenSpriteFileWithoutPalette("data/SHOTS.sff");
		mAnimations = loadMugenAnimationFile("data/SHOTS.air");

		mLoadedShots.clear();
		mShots.clear();
		mGimmicks.clear();
		loadShotGimmicks();

		MugenDefScript script;
		loadMugenDefScript(&script, "data/SHOTS.def");
		loadShotsFromScript(script);

		unloadMugenDefScript(script);
	}

	~ShotHandler() {
		mLoadedShots.clear();
		mShots.clear();
		mGimmicks.clear();
	}

	int isShotOutOfBounds(Shot& tShot) {
		Position p = getBlitzEntityPosition(tShot.mEntityID);
		return p.x < -30 || p.x > 360 || p.y < -30 || p.y > 270;
	}

	int updateSingleShot(Shot& tShot) {
		tShot.mCurrentFrame++;

		int ret = 0;
		if (tShot.mGimmick) {
			ret |= tShot.mGimmick(tShot.mOwner, &tShot);
		}

		if (tShot.mHasEntity) {
			ret |= isShotOutOfBounds(tShot);
		}
		else if (!tShot.mGimmick) {
			return 1;
		}

		return ret;
	}

	void update() {
		stl_int_map_remove_predicate(*this, mShots, &ShotHandler::updateSingleShot);
	}

	int removeSingleEnemyBullet(Shot& shot) {
		return shot.mCollisionList == getEnemyShotCollisionList();
	}

	int removeSingleEnemyBulletExceptComplex(Shot& shot) {
		if (shot.mCollisionList != getEnemyShotCollisionList()) return 0;

		return (shot.mHasEntity || !shot.mGimmick);
	}

	ShotData getShotDataByName(string tName) {
		return mLoadedShots[tName];
	}
};

ShotHandler* ShotHandler::mSelf = nullptr;

EXPORT_ACTOR_CLASS(ShotHandler);

void removeEnemyBullets()
{
	stl_int_map_remove_predicate(*gShotHandler, gShotHandler->mShots, &ShotHandler::removeSingleEnemyBullet);
}

void removeEnemyBulletsExceptComplex()
{
	stl_int_map_remove_predicate(*gShotHandler, gShotHandler->mShots, &ShotHandler::removeSingleEnemyBulletExceptComplex);
}

int getShotDamage(void * tCollisionData)
{
	ShotHandler::Shot* shot = (ShotHandler::Shot*)tCollisionData;
	return shot->mDamage;
}

void addShot(void* tOwner, Position tPos, std::string tName, int tList)
{
	ShotHandler::ShotData data = gShotHandler->mLoadedShots[tName];
	gShotHandler->addShot(tOwner, tPos, data, tList);
}

void addAngledShot(void * tCaller, Position tPosition, const string & tName, int tCollisionList, int tAngle, double tSpeed)
{
	gShotHandler->addAngledShot(tCaller, tPosition, tName, tCollisionList, tAngle, tSpeed);
}

void addAimedShot(void * tCaller, Position tPosition, const string & tName, int tCollisionList, int tAngleOffset, double tSpeed)
{
	gShotHandler->addAimedShot(tCaller, tPosition, tName, tCollisionList, tAngleOffset, tSpeed);
}

static void shotCB(void* tCaller, void* tCollisionData) {
	ShotHandler::Shot* shot = (ShotHandler::Shot*)tCaller;
	gShotHandler->mShots.erase(shot->mRootID);
}

