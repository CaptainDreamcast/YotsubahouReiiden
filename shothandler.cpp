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
		int mIsDoneAfterBeginning;

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
				mIsDoneAfterBeginning = mGimmick(mOwner, this);
			}
			else {
				mIsDoneAfterBeginning = 0;
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

	static void addKinomodAngled(Position tBase, Position tOffset, const std::string& tName, int tAmount, int tAngleSpan) {
		int start = 180 + -tAngleSpan / 2;
		for (int i = 0; i <= tAmount; i++) {
			int angle = start + i * (tAngleSpan / tAmount);
			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle, 4);
				return 0;
			};
			addDynamicSubShot(nullptr, tBase + vecRotateZ(tOffset, (angle / 360.0) * 2 * M_PI), tName, getPlayerShotCollisionList(), func);
		}
		addDynamicSubShot(nullptr, tBase, "kinomod_base", getPlayerShotCollisionList());
	}

	static int kinomodUnfocused1(void* tCaller, ShotHandler::Shot* tShot) {
		int amount = 4;
		int anglespan = 60;
		addKinomodAngled(getBlitzEntityPosition(getPlayerEntity()), makePosition(0, 1, 0), "kinomod_small", amount, anglespan);
		
		return 1;
	}

	static int kinomodUnfocused2(void* tCaller, ShotHandler::Shot* tShot) {
		int amount = 6;
		int anglespan = 60;
		addKinomodAngled(getBlitzEntityPosition(getPlayerEntity()), makePosition(0, 1, 0), "kinomod_small", amount, anglespan);

		return 1;
	}

	static int kinomodUnfocused3(void* tCaller, ShotHandler::Shot* tShot) {
		int amount = 8;
		int anglespan = 80;
		addKinomodAngled(getBlitzEntityPosition(getPlayerEntity()), makePosition(0, 1, 0), "kinomod_small", amount, anglespan);

		return 1;
	}

	static int kinomodUnfocused4(void* tCaller, ShotHandler::Shot* tShot) {
		int amount = 10;
		int anglespan = 90;
		addKinomodAngled(getBlitzEntityPosition(getPlayerEntity()), makePosition(0, 1, 0), "kinomod_small", amount, anglespan);

		return 1;
	}

	static int kinomodFocused1(void* tCaller, ShotHandler::Shot* tShot) {
		int amount = 4;
		int anglespan = 16;
		addKinomodAngled(getBlitzEntityPosition(getPlayerEntity()), makePosition(0, 1, 0), "kinomod_large", amount, anglespan);
		return 1;
	}

	static int kinomodFocused2(void* tCaller, ShotHandler::Shot* tShot) {
		int amount = 6;
		int anglespan = 18;
		addKinomodAngled(getBlitzEntityPosition(getPlayerEntity()), makePosition(0, 1, 0), "kinomod_large", amount, anglespan);
		return 1;
	}

	static int kinomodFocused3(void* tCaller, ShotHandler::Shot* tShot) {
		int amount = 8;
		int anglespan = 24;
		addKinomodAngled(getBlitzEntityPosition(getPlayerEntity()), makePosition(0, 1, 0), "kinomod_large", amount, anglespan);
		return 1;
	}

	static int kinomodFocused4(void* tCaller, ShotHandler::Shot* tShot) {
		int amount = 10;
		int anglespan = 30;
		addKinomodAngled(getBlitzEntityPosition(getPlayerEntity()), makePosition(0, 1, 0), "kinomod_large", amount, anglespan);
		return 1;
	}

	static void addAeroliteLaser(Position tPos, Position tOffset, int tAngle, const std::string& tName) {
		tAngle += 180;
		auto pos = tPos + vecRotateZ(tOffset, (tAngle / 360.0) * 2 * M_PI);

		function<int(void*, ShotHandler::Shot*)> func = [pos, tAngle, tName](void* tCaller, ShotHandler::Shot* tShot) {
			LaserData* data = (LaserData*)tShot->mUserData;

			if (tShot->mCurrentFrame == 0) {
				data->mPosition = pos;
				data->mDirection = vecRotateZ(makePosition(0, 1, 0), (tAngle / 360.0) * 2 * M_PI);
			}

			function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
				const auto x = getBlitzEntityPositionX(getPlayerEntity());
				setBlitzEntityPositionX(tShot->mEntityID, x);
				if (tShot->mCurrentFrame == 10) return 1;
				else return 0;
			};
			addDynamicSubShot(tCaller, data->mPosition, tName, getPlayerShotCollisionList(), func);

			data->mPosition += data->mDirection * 3;
			return !isInGameScreen(data->mPosition);
		};
		addDynamicSubShot(nullptr, pos, "invisible", getPlayerShotCollisionList(), func);
	}

	static int aeroliteUnfocused1(void* tCaller, ShotHandler::Shot* tShot) {
		auto pos = getBlitzEntityPosition(getPlayerEntity());
		addAeroliteLaser(pos, makePosition(0, 10, 0), 0, "aerolite_laser1");
		return 1;
	}

	static int aeroliteUnfocused2(void* tCaller, ShotHandler::Shot* tShot) {
		auto pos = getBlitzEntityPosition(getPlayerEntity());
		addAeroliteLaser(pos, makePosition(0, 10, 0), 0, "aerolite_laser2");

		return 1;
	}
	static int aeroliteUnfocused3(void* tCaller, ShotHandler::Shot* tShot) {
		auto pos = getBlitzEntityPosition(getPlayerEntity());
		addAeroliteLaser(pos, makePosition(-2, 10, 0), 0, "aerolite_laser3");
		return 1;
	}
	static int aeroliteUnfocused4(void* tCaller, ShotHandler::Shot* tShot) {
		auto pos = getBlitzEntityPosition(getPlayerEntity());
		addAeroliteLaser(pos, makePosition(-2, 10, 0), 0, "aerolite_laser4");
		return 1;
	}

	static int enemyStraight(void* tCaller, ShotHandler::Shot* tShot) {
		(void)tCaller;
		if (!tShot->mCurrentFrame) {
			if (tShot->mHasEntity) addBlitzPhysicsVelocityY(tShot->mEntityID, 2);
		}

		return 0;
	}



	static int setShotAimedTowardsPosition(void* tCaller, ShotHandler::Shot* tShot, int tAngleOffset, double tSpeed, Position tTarget) {
			if (!tShot->mHasEntity) return 0;

			Position shooterPosition = getBlitzEntityPosition(tShot->mEntityID);
			Position delta = tTarget - shooterPosition;
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

	static int setShotAimedAngle(void* tCaller, ShotHandler::Shot* tShot, int tAngleOffset, double tSpeed = 2) {
		if (!tShot->mHasEntity) return 0;
		Position playerPos = getBlitzEntityPosition(getPlayerEntity());
		return setShotAimedTowardsPosition(tCaller, tShot, tAngleOffset, tSpeed, playerPos);
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
	static int enemyAimedWide(void* tCaller, ShotHandler::Shot* tShot) {
		setShotAimedAngleBegin(tCaller, tShot, randfromInteger(-45, 45), 0.7);
		return 0;
	}
	static int enemyWideningAngle(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		int angle = std::min(90, shooter->m_DeltaTime / 4);
		setShotAimedAngleBegin(tCaller, tShot, randfromInteger(-angle, angle), 0.7);
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

	static void addAngledLaser(Position tPosition, int tAngle) {
		function<int(void*, ShotHandler::Shot*)> func = [tPosition, tAngle](void* tCaller, ShotHandler::Shot* tShot) {
			LaserData* data = (LaserData*)tShot->mUserData;

			if (tShot->mCurrentFrame == 0) {
				data->mPosition = tPosition;
				data->mDirection = vecRotateZ(makePosition(0, 1, 0), (tAngle / 360.0) * 2 * M_PI);
			}

			function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
				if (tShot->mCurrentFrame == 40) return 1;
				else return 0;
			};
			addDynamicSubShot(tCaller, data->mPosition, "enemy_laser", getEnemyShotCollisionList(), func);

			data->mPosition += data->mDirection * 3;
			return !isInGameScreen(data->mPosition) || isPlayerBombActive();
		};
		addDynamicSubShot(nullptr, tPosition, "enemy_laser", getEnemyShotCollisionList(), func);
	}

	static int enemyLaserCircle(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		int amount = 5;
		for (int i = 0; i < 5; i++) {
			addAngledLaser(getBlitzEntityPosition(shooter->mEntityID), (360 / amount) * i);
		}

		return 1;
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
		if (tShot->mCurrentFrame == 0) {
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
		}
		return 1;
	}

	static int enemyFiveAimedDifferent(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame == 0) {
			ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
			auto p = getBlitzEntityPosition(shooter->mEntityID);

			int amount = 10;
			for (int i = 0; i < amount; i++) {
				function<int(void*, ShotHandler::Shot*)> func = [i](void* tCaller, ShotHandler::Shot* tShot) {
					setShotAimedAngleBegin(tCaller, tShot, 0, 0.25 + i * 0.25);
					return 0;
				};

				addDynamicSubShot(tCaller, p, "enemy_slim", getEnemyShotCollisionList(), func);
			}
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

	static int enemyCircleSlim(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		auto p = getBlitzEntityPosition(shooter->mEntityID);

		int amount = 20;
		for (int i = 0; i < amount; i++) {
			int angle = i * (360 / amount);
			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle, 1);

				return 0;
			};

			addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
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

	static int enemyPCB2(void* tCaller, ShotHandler::Shot* tShot) {
		ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
		auto p = getBlitzEntityPosition(shooter->mEntityID);


		function<int(void*, ShotHandler::Shot*)> func;
		
		if (tShot->mCurrentFrame % 10 == 0)
		{
			func = [](void* tCaller, ShotHandler::Shot* tShot) {
				setShotAimedAngleBegin(tCaller, tShot, 0, 1);
				return 0;
			};
			addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
		}

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
		if (tShot->mCurrentFrame % 60 == 0) {
			Position p = getBossPosition();
			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				setShotAimedAngleBegin(nullptr, tShot, 0, 3);
				setBlitzEntityRotationZ(tShot->mEntityID, tShot->mCurrentFrame * 0.1);
				return 0;
			};
			addDynamicSubShot(tCaller, p, "shii_world", getEnemyShotCollisionList(), func);
		}

		return 0;
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

		if (tShot->mCurrentFrame % 300 == 0) {
			Position basePosition = getBossPosition();
			int amount = 5;
			for (int i = 0; i < amount; i++) {
				double offset = randfrom(-10, 10);
				double diff = gGameVars.gameScreen.x / double(amount);
				double x = gGameVars.gameScreenOffset.x + diff * i;
				function<int(void*, ShotHandler::Shot*)> func = [x](void* tCaller, ShotHandler::Shot* tShot) {
					LaserData* data = (LaserData*)tShot->mUserData;

					if (tShot->mCurrentFrame == 0) {
						data->mPosition = makePosition(x, gGameVars.gameScreenOffset.y, 0);
						data->mDirection = makePosition(0, 1, 0);
						data->mDirection.z = 0;
						data->mAngle = 0;
					}

					function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
						if (tShot->mCurrentFrame == 60) {
							if (randfromInteger(0, 2) == 0) {
								double speedDelta = randfrom(0.01, 0.02);
								int angle = randfromInteger(-1, 1);
								function<int(void*, ShotHandler::Shot*)> internalFunc = [speedDelta, angle](void* tCaller, ShotHandler::Shot* tShot) {
									AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
									if (!tShot->mCurrentFrame) {
										data->speed = 0;
									}

									if (tShot->mCurrentFrame > 60) {
										data->speed += speedDelta;
										setShotNormalAngle(tShot, angle, data->speed);
									}

									return 0;
								};
								auto pos = getBlitzEntityPosition(tShot->mEntityID);
								addDynamicSubShot(tCaller, pos, "enemy_mid", getEnemyShotCollisionList(), internalFunc);
							}
							return 1;
						}
						else return 0;
					};
					addDynamicSubShot(tCaller, data->mPosition, "enemy_laser", getEnemyShotCollisionList(), func);
					data->mPosition += vecRotateZ(data->mDirection, data->mAngle) * 2;
					return !isInGameScreen(data->mPosition) || isPlayerBombActive();
				};

				addDynamicSubShot(tCaller, basePosition, "enemy_laser", getEnemyShotCollisionList(), func);
			}

			for (int i = 0; i < amount; i++) {
				double offset = randfrom(-10, 10);
				double diff = gGameVars.gameScreen.y / double(amount);
				double y = gGameVars.gameScreenOffset.y + diff * i;
				function<int(void*, ShotHandler::Shot*)> func = [y](void* tCaller, ShotHandler::Shot* tShot) {
					LaserData* data = (LaserData*)tShot->mUserData;

					if (tShot->mCurrentFrame == 0) {
						data->mPosition = makePosition(gGameVars.gameScreenOffset.x, y, 0);
						data->mDirection = makePosition(1, 0, 0);
						data->mDirection.z = 0;
						data->mAngle = 0;
					}

					function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
						if (tShot->mCurrentFrame == 60) { 
							if (randfromInteger(0, 2) == 0) {
								double speedDelta = randfrom(0.01, 0.02);
								int angle = randfromInteger(-1, 1);
								function<int(void*, ShotHandler::Shot*)> internalFunc = [speedDelta, angle](void* tCaller, ShotHandler::Shot* tShot) {
									AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
									if (!tShot->mCurrentFrame) {
										data->speed = 0;
									}

									if (tShot->mCurrentFrame > 60) {
										data->speed += speedDelta;
										setShotNormalAngle(tShot, angle, data->speed);
									}

									return 0;
								};
								auto pos = getBlitzEntityPosition(tShot->mEntityID);
								addDynamicSubShot(tCaller, pos, "enemy_mid", getEnemyShotCollisionList(), internalFunc);
							}
							return 1; 
						}
						else return 0;
					};
					addDynamicSubShot(tCaller, data->mPosition, "enemy_laser", getEnemyShotCollisionList(), func);
					data->mPosition += vecRotateZ(data->mDirection, data->mAngle) * 2;
					return !isInGameScreen(data->mPosition) || isPlayerBombActive();
				};

				addDynamicSubShot(tCaller, basePosition, "enemy_laser", getEnemyShotCollisionList(), func);
			}
		}
		return 0;
	}

	static void addRandomWorldShot(Position p)
	{
		function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
			setShotNormalAngleBegin(tShot, randfromInteger(0, 359), randfrom(0.5, 2));
			return 0;
		};
		addDynamicSubShot(nullptr, p, "enemy_mid", getEnemyShotCollisionList(), func);
	}

	static int shiiS5(void* tCaller, ShotHandler::Shot* tShot) {
		Position p = getBossPosition();
		if (tShot->mCurrentFrame == 0) {
			Position p = getBossPosition();
			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				setShotAimedAngle(nullptr, tShot, 0, 0.2);
				setBlitzEntityRotationZ(tShot->mEntityID, tShot->mCurrentFrame * 0.2);
				addRandomWorldShot(getBlitzEntityPosition(tShot->mEntityID));
				return 0;
			};
			addDynamicSubShot(tCaller, p, "shii_world", getEnemyShotCollisionList(), func);
		}

		return 0;
	}

	typedef struct {
		double angle;

	} EnemyFinalData;

	static int enemyFinal1(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 20 == 0) {
			int amount = 40;
			ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
			if (!isActiveEnemy(shooter)) return 1;
			auto p = getBlitzEntityPosition(shooter->mEntityID);
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount) + tShot->mCurrentFrame / 10;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (!tShot->mCurrentFrame) {
						data->angle = angle;
					}
					data->angle += 0.3;
					setShotNormalAngle(tShot, int(data->angle), 1.5);

					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int enemyFinal2(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 15 == 0) {
			int amount = 40;
			ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
			if (!isActiveEnemy(shooter)) return 1;
			auto p = getBlitzEntityPosition(shooter->mEntityID);
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount) + tShot->mCurrentFrame / 10;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (!tShot->mCurrentFrame) {
						data->angle = angle;
					}
					data->angle += 0.5;
					setShotNormalAngle(tShot, int(data->angle), 1.5);

					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int enemyFinal3(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 10 == 0) {
			int amount = 40;
			ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
			if (!isActiveEnemy(shooter)) return 1;
			auto p = getBlitzEntityPosition(shooter->mEntityID);
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount) + tShot->mCurrentFrame / 10;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (!tShot->mCurrentFrame) {
						data->angle = angle;
					}
					data->angle += 0.7;
					setShotNormalAngle(tShot, int(data->angle), 1.5);

					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int shiiMidNS1(void* tCaller, ShotHandler::Shot* tShot) {

		int amount = 75;
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

	static int shiiMidS1(void* tCaller, ShotHandler::Shot* tShot) {

		if (tShot->mCurrentFrame % 10 == 0) {
			int amount = 5;
			auto p = getBossPosition();
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount) + tShot->mCurrentFrame / 10;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (!tShot->mCurrentFrame) {
						data->angle = angle;
					}
					data->angle += 0.7;
					setShotNormalAngle(tShot, int(data->angle), 1.5);

					return 0;
				};

				addDynamicSubShot(NULL, p, "shii_world", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int hiroNS1(void* tCaller, ShotHandler::Shot* tShot) {
		Position basePosition = getBossPosition();

		int angle = tShot->mCurrentFrame;
		angle *= 7;
		Position p = basePosition + vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);
		function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
			setShotNormalAngleBegin(tShot, angle, 1);
			return 0;
		};
		addDynamicSubShot(tCaller, basePosition, "enemy_slim", getEnemyShotCollisionList(), func);

		angle = tShot->mCurrentFrame - 45;
		angle *= 7;
		p = basePosition + vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);
		func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
			setShotNormalAngleBegin(tShot, angle, 1);
			return 0;
		};
		addDynamicSubShot(tCaller, basePosition, "enemy_slim", getEnemyShotCollisionList(), func);

		angle = tShot->mCurrentFrame - 180;
		angle *= 7;
		p = basePosition + vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);
		func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
			setShotNormalAngleBegin(tShot, angle, 2);
			return 0;
		};
		addDynamicSubShot(tCaller, basePosition, "enemy_mid", getEnemyShotCollisionList(), func);

		angle = tShot->mCurrentFrame - 225;
		angle *= 7;
		p = basePosition + vecRotateZ(makePosition(0, 10, 0), (angle / 360.0) * 2 * M_PI);
		func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
			setShotNormalAngleBegin(tShot, angle, 2);
			return 0;
		};
		addDynamicSubShot(tCaller, basePosition, "enemy_mid", getEnemyShotCollisionList(), func);

		return 0;
	}

	static int hiroS1(void* tCaller, ShotHandler::Shot* tShot) {
		Position basePosition = getBossPosition();

		int baseAngle = 0;

		if (tShot->mCurrentFrame > 100) {
			baseAngle = tShot->mCurrentFrame - 100;
		}

		if (tShot->mCurrentFrame % 120 == 0) {
			int amount = 10;
			for (int i = 0; i < amount; i++) {
				int angle = (360 / amount) * i + baseAngle;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					LaserData* data = (LaserData*)tShot->mUserData;

					if (tShot->mCurrentFrame == 0) {
						data->mPosition = getBossPosition();
						data->mDirection = vecRotateZ(makePosition(0, 1, 0), (angle / 360.0) * 2 * M_PI);
					}

					function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
						if (tShot->mCurrentFrame == 40) return 1;
						else return 0;
					};
					addDynamicSubShot(tCaller, data->mPosition, "enemy_laser", getEnemyShotCollisionList(), func);

					data->mPosition += data->mDirection * 3;
					return !isInGameScreen(data->mPosition) || isPlayerBombActive();
				};
				addDynamicSubShot(tCaller, basePosition, "enemy_laser", getEnemyShotCollisionList(), func);
			}
		}
		
		if (tShot->mCurrentFrame % 10 == 0) {
			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, randfromInteger(0, 359), 1);
				return 0;
			};
			addDynamicSubShot(tCaller, basePosition, "enemy_slim", getEnemyShotCollisionList(), func);
		}

		return 0;
	}

	static int hiroNS2(void* tCaller, ShotHandler::Shot* tShot) {

		if (tShot->mCurrentFrame % 300 < 60 && tShot->mCurrentFrame % 30 == 0) {
			Position basePosition = getBossPosition();

			int amount = 15;
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount) + (tShot->mCurrentFrame / 300) * 11;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					setShotNormalAngleBegin(tShot, angle, 2);
					if (tShot->mCurrentFrame >= 20 && tShot->mCurrentFrame % 30 == 0) {
						auto p = getBlitzEntityPosition(tShot->mEntityID);
						function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
							setShotNormalAngleBegin(tShot, angle+90, 1);
							if (tShot->mCurrentFrame >= 20 && tShot->mCurrentFrame % 30 == 0) {
								auto p = getBlitzEntityPosition(tShot->mEntityID);
								function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
									setShotNormalAngleBegin(tShot, angle+90+45, 1);
									return 0;
								};
								addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
							}

							return 0;
						};
						addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);

						p = getBlitzEntityPosition(tShot->mEntityID);
						func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
							setShotNormalAngleBegin(tShot, angle - 90, 1);
							if (tShot->mCurrentFrame >= 20 && tShot->mCurrentFrame % 30 == 0) {
								auto p = getBlitzEntityPosition(tShot->mEntityID);
								function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
									setShotNormalAngleBegin(tShot, angle - 90 - 45, 1);
									return 0;
								};
								addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
							}

							return 0;
						};
						addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
					}

					return 0;
				};

				addDynamicSubShot(NULL, basePosition, "enemy_large", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int hiroS2(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 1 == 0) {
			Position basePosition = getBossPosition();
			int angle = tShot->mCurrentFrame * 111;
			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle, 3);
				return 0;
			};
			addDynamicSubShot(NULL, basePosition, "enemy_large", getEnemyShotCollisionList(), func);

			basePosition = getBossPosition();
			angle = -tShot->mCurrentFrame * 111;
			func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle, 3);
				return 0;
			};
			addDynamicSubShot(NULL, basePosition, "enemy_large", getEnemyShotCollisionList(), func);
		}

		if (tShot->mCurrentFrame % 120 == 0) {
			Position basePosition = getBossPosition();
			function<int(void*, ShotHandler::Shot*)> func = [basePosition](void* tCaller, ShotHandler::Shot* tShot) {
				LaserShakeData* data = (LaserShakeData*)tShot->mUserData;

				if (tShot->mCurrentFrame == 0) {
					data->mPosition = basePosition;
					data->mAngle = ((randfromInteger(0, 359)) / 360.0) * 2 * M_PI;
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
				return !isInGameScreen(data->mPosition) || isPlayerBombActive();
			};

			addDynamicSubShot(tCaller, basePosition, "enemy_laser", getEnemyShotCollisionList(), func);
		}

		return 0;
	}

	static int hiroNS3(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 40 == 0) {
			int amount = 10;
			Position left = getScreenPositionFromGamePosition(-0.2, -0.05, 0.0);
			Position right = getScreenPositionFromGamePosition(1.2, -0.05, 0.0);
			for (int i = 0; i < amount; i++)
			{
				auto p = left + (right - left) * (i / double(amount));
				function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
					setShotNormalAngleBegin(tShot, -5, 1.5);
					return 0;
				};
				addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
			}
		}

		if (tShot->mCurrentFrame % 120 >= 60 && tShot->mCurrentFrame % 120 <= 100 && tShot->mCurrentFrame % 10 == 0) {
			Position basePosition = getBossPosition();
			Position target = getScreenPositionFromGamePosition(randfrom(0.1, 0.9), 0.6, 0);
			function<int(void*, ShotHandler::Shot*)> func = [target](void* tCaller, ShotHandler::Shot* tShot) {
				if (tShot->mCurrentFrame == 0) {
					setShotAimedTowardsPosition(NULL, tShot, 0, 1.5, target);
				} else if (tShot->mCurrentFrame == 40) {
					setShotAimedTowardsPosition(NULL, tShot, 0, 0, target);
				}
				else if (tShot->mCurrentFrame == 100) {
					Position p = getBlitzEntityPosition(getPlayerEntity());
					setShotAimedTowardsPosition(NULL, tShot, 0, 1.5, p);
				}
				return 0;
			};
			addDynamicSubShot(NULL, basePosition, "enemy_mid", getEnemyShotCollisionList(), func);
		}

		return 0;
	}

	typedef struct {
		double x;

	} HiroS3Data;

	static int hiroS3(void* tCaller, ShotHandler::Shot* tShot) {
		HiroS3Data* data = (HiroS3Data*)tShot->mUserData;
		if (tShot->mCurrentFrame == 0) {
			data->x = getBossPosition().x;
		}
		if (tShot->mCurrentFrame <= 50) {
			double y = getScreenPositionFromGamePositionY(tShot->mCurrentFrame / 50.0 * 0.5);
			std::string name;
			int rando = randfromInteger(0, 3);
			if (rando == 0) {
				name = "enemy_slim";
			}
			else if (rando == 1) {
				name = "enemy_mid";
			}
			else {
				name = "enemy_large";
			}
			double x = data->x;
			x += randfrom(-10, 10);

			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				if (tShot->mCurrentFrame == 100) {
					setShotNormalAngle(tShot, 0, randfrom(0.5, 2));
				}
				return 0;
			};
			addDynamicSubShot(NULL, makePosition(x, y, 0), name, getEnemyShotCollisionList(), func);
		}

		return tShot->mCurrentFrame > 50;
	}

	static int hiroNS4(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 40 < 10) {
			Position p = getBossPosition();
			function<int(void*, ShotHandler::Shot*)> func;

			int amount = 10;
			for(int i = 0; i < amount; i++)
			{
				int angle = randfromInteger(-90, 90);
				if (tShot->mCurrentFrame % 80 >= 40) {
					func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
						EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
						if (tShot->mCurrentFrame == 0) {
							data->angle = -angle;
						}
						if (tShot->mCurrentFrame > 30) {
							data->angle += 0.3;
						}

						setShotNormalAngle(tShot, int(data->angle), 1);
						return 0;
					};
				}
				else {
					func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
						EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
						if (tShot->mCurrentFrame == 0) {
							data->angle = angle;
						}
						if (tShot->mCurrentFrame > 30) {
							data->angle -= 0.3;
						}

						setShotNormalAngle(tShot, int(data->angle), 1);
						return 0;
					};
				}

				addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
			}
		}
		return 0;
	}

	static int hiroS4(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 30 == 0) {
			Position p = getBossPosition();
			int amount = 10;
			auto tempPos = p;
			for (int i = 0; i < amount; i++) {
				tempPos.x = getScreenPositionFromGamePositionX(1);
				function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (tShot->mCurrentFrame == 0) {
						data->angle = 45;
					}
					data->angle += 0.1 + ((tShot->mCurrentFrame / 30) % 10) * 0.2;

					setShotNormalAngle(tShot, int(data->angle), 0.6);
					return 0;
				};
				addDynamicSubShot(NULL, tempPos, "enemy_mid", getEnemyShotCollisionList(), func);
				tempPos.y += gGameVars.gameScreen.y*0.1;
			}

			tempPos = p;
			for (int i = 0; i < amount; i++) {
				tempPos.x = getScreenPositionFromGamePositionX(0);
				function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (tShot->mCurrentFrame == 0) {
						data->angle = 45;
					}
					data->angle += 0.1 + ((tShot->mCurrentFrame / 30) % 10) * 0.2;

					setShotNormalAngle(tShot, int(data->angle), 0.6);
					return 0;
				};
				addDynamicSubShot(NULL, tempPos, "enemy_mid", getEnemyShotCollisionList(), func);
				tempPos.y += gGameVars.gameScreen.y*0.1;
			}
		}

		if (tShot->mCurrentFrame % 30 == 5) {
			Position p = getBossPosition();
			int amount = 10;
			auto tempPos = p;
			for (int i = 0; i < amount; i++) {
				tempPos.x = getScreenPositionFromGamePositionX(0);
				function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (tShot->mCurrentFrame == 0) {
						data->angle = -45;
					}
					data->angle -= 0.1 + ((tShot->mCurrentFrame / 30) % 10) * 0.2;

					setShotNormalAngle(tShot, int(data->angle), 0.6);
					return 0;
				};
				addDynamicSubShot(NULL, tempPos, "enemy_mid", getEnemyShotCollisionList(), func);
				tempPos.y += gGameVars.gameScreen.y*0.1;
			}

			tempPos = p;
			for (int i = 0; i < amount; i++) {
				tempPos.x = getScreenPositionFromGamePositionX(0);
				function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (tShot->mCurrentFrame == 0) {
						data->angle = -45;
					}
					data->angle -= 0.1 + ((tShot->mCurrentFrame / 30) % 10) * 0.2;

					setShotNormalAngle(tShot, int(data->angle), 0.6);
					return 0;
				};
				addDynamicSubShot(NULL, tempPos, "enemy_mid", getEnemyShotCollisionList(), func);
				tempPos.y += gGameVars.gameScreen.y*0.1;
			}
		}

		return 0;
	}

	static int hiroNS5(void* tCaller, ShotHandler::Shot* tShot) {
		auto p = getBossPosition();
		int angle = tShot->mCurrentFrame * 5;
		double speedup = randfrom(0.01, 0.02);
		function<int(void*, ShotHandler::Shot*)> func = [angle, speedup](void* tCaller, ShotHandler::Shot* tShot) {
			AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
			if (tShot->mCurrentFrame == 0) {
				data->speed = 0;
			}
			data->speed += speedup;
			setShotNormalAngle(tShot, angle, data->speed);

			return 0;
		};
		addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);

		angle = -tShot->mCurrentFrame * 7;
		speedup = randfrom(0.01, 0.02);
		func = [angle, speedup](void* tCaller, ShotHandler::Shot* tShot) {
			AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
			if (tShot->mCurrentFrame == 0) {
				data->speed = 0;
			}
			data->speed += speedup;
			setShotNormalAngle(tShot, angle, data->speed);

			return 0;
		};
		addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);

		angle = tShot->mCurrentFrame * 11;
		speedup = randfrom(0.01, 0.02);
		func = [angle, speedup](void* tCaller, ShotHandler::Shot* tShot) {
			AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
			if (tShot->mCurrentFrame == 0) {
				data->speed = 0;
			}
			data->speed += speedup;
			setShotNormalAngle(tShot, angle, data->speed);

			return 0;
		};
		addDynamicSubShot(NULL, p, "enemy_large", getEnemyShotCollisionList(), func);

		angle = -tShot->mCurrentFrame * 10;
		speedup = randfrom(0.01, 0.02);
		func = [angle, speedup](void* tCaller, ShotHandler::Shot* tShot) {
			AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
			if (tShot->mCurrentFrame == 0) {
				data->speed = 0;
			}
			data->speed += speedup;
			setShotNormalAngle(tShot, angle, data->speed);

			return 0;
		};
		addDynamicSubShot(NULL, p, "enemy_large", getEnemyShotCollisionList(), func);


		return 0;
	}

	static int hiroS5(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 360 == 0) {
			Position p = getBossPosition();
			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
				if (tShot->mCurrentFrame == 0) {
					data->angle = M_PI;
				}

				auto direction = vecRotateZ(makePosition(0, 1, 0), data->angle);
				data->angle -= 0.01;

				function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
						return tShot->mCurrentFrame == 2;
				};
				Position p = getBossPosition();
				for (int i = 0; i < 100; i++) {
					addDynamicSubShot(NULL, p, "enemy_laser", getEnemyShotCollisionList(), func);
					p = p + (direction * 3);
				}

				return data->angle < -M_PI;
			};
			addDynamicSubShot(NULL, p, "enemy_laser", getEnemyShotCollisionList(), func);
		}

		if (tShot->mCurrentFrame % 20 == 0) {
			Position p = getBossPosition();
			int amount = 20;
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount);
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					setShotNormalAngleBegin(tShot, angle, 1);

					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_mid", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	typedef struct {
		Position mPosition;

	} HiroCircleData;

	static int hiroCircle(ShotHandler::Shot* tShot, double radius, double x, double speed, double angleSpeedFactor)
	{
		HiroCircleData* data = (HiroCircleData*)tShot->mUserData;
		if (!tShot->mCurrentFrame) {
			data->mPosition = makePosition(x, gGameVars.gameScreenOffset.y - radius, 0);
		}
	
		int amount = 30;
		for (int i = 0; i < amount; i++) {
			int angleInteger = int(i * (360 / amount) + tShot->mCurrentFrame * angleSpeedFactor);
			Position delta = makePosition(0, radius, 0);
			delta.z = 0;
			double angle = (angleInteger / 360.0) * 2 * M_PI;
			delta = vecRotateZ(delta, angle);
			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				return tShot->mCurrentFrame == 1;
			};
			addDynamicSubShot(NULL, data->mPosition + delta, "enemy_laser", getEnemyShotCollisionList(), func);
		}

		data->mPosition.y += speed;

		return 0;
	}

	static int hiroS6_1(void* tCaller, ShotHandler::Shot* tShot) {
		return hiroCircle(tShot, 200, getScreenPositionFromGamePositionX(0.2), 0.6, 0.7);
	}
	static int hiroS6_2(void* tCaller, ShotHandler::Shot* tShot) {
		return hiroCircle(tShot, 200, getScreenPositionFromGamePositionX(0.8), 0.6, 0.7);
	}
	static int hiroS6_3(void* tCaller, ShotHandler::Shot* tShot) {
		return hiroCircle(tShot, 100, getScreenPositionFromGamePositionX(0.5), 0.5, 1);
	}


	static int crisis(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 360 == 0 || tShot->mCurrentFrame % 360 == 10 || tShot->mCurrentFrame % 360 == 20 || tShot->mCurrentFrame % 360 == 30) {
			int amount = 10;
			for (int i = 0; i < amount; i++) {
				double t = i / double(amount);
				auto p = getScreenPositionFromGamePosition(t, 0, 0) + makePosition(randfrom(-10, 10), randfrom(-5, 5), 0);
				function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
					AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
					if (tShot->mCurrentFrame == 0) {
						data->speed = 0;
					}
					data->speed += 0.01;
					setShotNormalAngle(tShot, 0, data->speed);
					return 0;
				};
				addDynamicSubShot(NULL, p, "enemy_large", getEnemyShotCollisionList(), func);
			}
		}


		if (tShot->mCurrentFrame % 360 == 60) {
			Position p = getScreenPositionFromGamePosition(0.5, 0.5, 0);


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
		}

		if (tShot->mCurrentFrame % 360 == 120) {
			Position p = getScreenPositionFromGamePosition(0.5, 0.5, 0);


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
		}

		if (tShot->mCurrentFrame % 360 == 180) {
			for (int i = 0; i < 5; i++) {
				auto p = getScreenPositionFromGamePosition(0.5, 0.5, 0);
				function<int(void*, ShotHandler::Shot*)> func = [p](void* tCaller, ShotHandler::Shot* tShot) {
					LaserShakeData* data = (LaserShakeData*)tShot->mUserData;

					if (tShot->mCurrentFrame == 0) {
						data->mPosition = p;
						data->mAngle = ((randfromInteger(0, 359)) / 360.0) * 2 * M_PI;
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
					return !isInGameScreen(data->mPosition) || isPlayerBombActive();
				};

				addDynamicSubShot(tCaller, p, "enemy_laser", getEnemyShotCollisionList(), func);
			}
		}


		if (tShot->mCurrentFrame % 360 == 240 || tShot->mCurrentFrame % 360 == 250 || tShot->mCurrentFrame % 360 == 260 || tShot->mCurrentFrame % 360 == 270 || tShot->mCurrentFrame % 360 == 280) {
			Position p = getScreenPositionFromGamePosition(0.5, 0.5, 0);
			int amount = 70;
			for (int i = 0; i < amount; i++) {
				function<int(void*, ShotHandler::Shot*)> func;

				int angle = i * (360 / amount) + (tShot->mCurrentFrame % 20) + 180;
				func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (tShot->mCurrentFrame == 0) {
						data->angle = angle;
					}
					data->angle += 0.1;

					setShotNormalAngle(tShot, int(data->angle), 1);
					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}
	static int enemyFinal4(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 10 == 0) {
			int amount = 40;
			ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
			if (!isActiveEnemy(shooter)) return 1;
			auto p = getBlitzEntityPosition(shooter->mEntityID);
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount) + tShot->mCurrentFrame / 10;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (!tShot->mCurrentFrame) {
						data->angle = angle;
					}
					if (tShot->mCurrentFrame % 60 < 30) {
						data->angle += 3;
					}
					else {
						data->angle -= 3;
					}
					setShotNormalAngle(tShot, int(data->angle), 1.5);

					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}
	
	static int enemyFinal5(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 10 == 0) {
			int amount = 40;
			ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
			if (!isActiveEnemy(shooter)) return 1;
			auto p = getBlitzEntityPosition(shooter->mEntityID);
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount) + tShot->mCurrentFrame / 5;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (!tShot->mCurrentFrame) {
						data->angle = angle;
					}
					if (tShot->mCurrentFrame % 60 < 30) {
						data->angle += 3;
					}
					else {
						data->angle -= 3;
					}
					setShotNormalAngle(tShot, int(data->angle), 1.5);

					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	static int enemyFinal6(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 10 == 0) {
			int amount = 40;
			ActiveEnemy* shooter = (ActiveEnemy*)tCaller;
			if (!isActiveEnemy(shooter)) return 1;
			auto p = getBlitzEntityPosition(shooter->mEntityID);
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount) + tShot->mCurrentFrame;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
					if (!tShot->mCurrentFrame) {
						data->angle = angle;
					}
					if (tShot->mCurrentFrame % 60 < 30) {
						data->angle += 3;
					}
					else {
						data->angle -= 3;
					}
					setShotNormalAngle(tShot, int(data->angle), 1.5);

					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
			}
		}

		return 0;
	}

	typedef struct {
		int mStartStage;
		double mSpeed;
		int mAngle;
	} AlternativeNS1Data;

	static int alternativeNS1(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 60 == 0) {
			int amount = 40;
			auto p = getBossPosition();
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount) + tShot->mCurrentFrame;
				
				auto originalShot = tShot;
				function<int(void*, ShotHandler::Shot*)> func = [originalShot](void* tCaller, ShotHandler::Shot* tShot) {
					AlternativeNS1Data* data = (AlternativeNS1Data*)tShot->mUserData;
					int stage = originalShot->mCurrentFrame / 60;
					if (!tShot->mCurrentFrame) {
						data->mStartStage = stage;
						data->mSpeed = randfrom(0.5, 2);
						data->mAngle = randfromInteger(0, 359);
					}
					if (stage % 3 == data->mStartStage % 3) {
						setShotNormalAngle(tShot, data->mAngle, data->mSpeed);
					}
					else {
						setShotNormalAngle(tShot, data->mAngle, 0);
					}

					return 0;
				};

				addDynamicSubShot(NULL, p, "enemy_slim", getEnemyShotCollisionList(), func);
			}

			amount = 5;
			for (int i = 0; i < amount; i++) {
				addAngledLaser(p, (360 / amount) * i + tShot->mCurrentFrame * 11);
			}
		}

		return 0;
	}

	typedef struct {
		double mSpeed;
		double mDelta;

	} BulletWallData;

	static void addBulletWall(double x, int angle) {
		for (int i = 0; i < 70; i++) {
			double y = getScreenPositionFromGamePositionY(randfrom(0, 1));
			std::string name;
			int rando = randfromInteger(0, 3);
			if (rando == 0) {
				name = "enemy_slim";
			}
			else if (rando == 1) {
				name = "enemy_mid";
			}
			else {
				name = "enemy_large";
			}
			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				BulletWallData* data = (BulletWallData*)tShot->mUserData;
				if (!tShot->mCurrentFrame) {
					data->mSpeed = 0;
					data->mDelta = randfrom(0.01, 0.02);
				}
				data->mSpeed += data->mDelta;
				setShotNormalAngle(tShot, angle, data->mSpeed);

				return 0;
			};
			addDynamicSubShot(NULL, makePosition(x, y, 0), name, getEnemyShotCollisionList(), func);
		}


	}

	static int alternativeS11(void* tCaller, ShotHandler::Shot* tShot) {
		addBulletWall(getScreenPositionFromGamePositionY(0), -90);
		return 1;
	}

	static int alternativeS12(void* tCaller, ShotHandler::Shot* tShot) {
		addBulletWall(getScreenPositionFromGamePositionY(1), 90);
		return 1;
	}

	static void addBouncyLaser(int angle) {
		auto pos = getBossPosition();
		function<int(void*, ShotHandler::Shot*)> func = [pos, angle](void* tCaller, ShotHandler::Shot* tShot) {
			LaserData* data = (LaserData*)tShot->mUserData;

			if (tShot->mCurrentFrame == 0) {
				data->mPosition = pos;
				data->mDirection = vecRotateZ(makePosition(0, 1, 0), (angle / 360.0) * 2 * M_PI);
			}

			function<int(void*, ShotHandler::Shot*)> func = [data](void* tCaller, ShotHandler::Shot* tShot) {
				if (tShot->mCurrentFrame == 40) return 1;
				else return 0;
			};
			addDynamicSubShot(tCaller, data->mPosition, "enemy_laser", getEnemyShotCollisionList(), func);

			if (data->mPosition.x < getScreenPositionFromGamePositionX(0.0) || data->mPosition.x > getScreenPositionFromGamePositionX(1.0)) data->mDirection.x *= -1;
			if (data->mPosition.y < getScreenPositionFromGamePositionY(0.0)) data->mDirection.y *= -1;

			data->mPosition += data->mDirection * 3;
			return data->mPosition.y > getScreenPositionFromGamePositionY(1.0) || isPlayerBombActive();
		};
		addDynamicSubShot(nullptr, pos, "enemy_laser", getEnemyShotCollisionList(), func);
	}

	static void addRoundShot(ShotHandler::Shot* tShot) {
		auto pos = getBossPosition();

		int angle = tShot->mCurrentFrame * 111;
		function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
			EnemyFinalData* data = (EnemyFinalData*)tShot->mUserData;
			if (tShot->mCurrentFrame == 0) {
				data->angle = (double)angle;
			}
			data->angle += 0.1;

			setShotNormalAngle(tShot, int(data->angle), 2);

			return 0;
		};
		addDynamicSubShot(nullptr, pos, "enemy_mid", getEnemyShotCollisionList(), func);
	}

	static int alternativeS2(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 50 == 0) {
			int amount = 11;
			for (int i = 0; i < amount; i++) {
				int angle = ((360 / amount) * i + tShot->mCurrentFrame * 11) % 360;
				if (angle > 70 && angle < 110) continue;
				if (angle > 70 + 180 && angle < 110 + 180) continue;
				addBouncyLaser(angle);
			}
		}

		addRoundShot(tShot);
		return 0;
	}

	static void addMootExplosion(int tAngle)
	{
		Position p = getBossPosition();
		int amount = 10;
		for (int i = 0; i < amount; i++) {
			Position offset = vecRotateZ(makePosition(0, 25, 0), (tAngle / 360.0) * 2 * M_PI);
			int angle = (360 / amount) * i + tAngle;
			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle, 2);

				return 0;
			};
			addDynamicSubShot(nullptr, p + offset, "enemy_slim", getEnemyShotCollisionList(), func);
		}
	}

	static int mootNS1(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 4 == 0) {
			addMootExplosion(tShot->mCurrentFrame + tShot->mCurrentFrame / 10);
		}

		return 0;
	}

	static std::string getRandomShot() {
		std::string name;
		int rando = randfromInteger(0, 3);
		if (rando == 0) {
			name = "enemy_slim";
		}
		else if (rando == 1) {
			name = "enemy_mid";
		}
		else {
			name = "enemy_large";
		}
		return name;
	}

	static void addHammerAnimation(Position tPos, int tAnimation) {
		// TODO: add hammeranimation
	}

	static void updateHammer(Position tPos, int tDeltaFrame, int tAngleDelta) {
		int angle = tDeltaFrame;
		if (tAngleDelta == -1) angle += 90;
		Position offset = vecRotateZ(makePosition(100, 0, 0), (angle / 360.0) * 2 * M_PI);
	
		int amount = 1;
		for (int i = 0; i < amount; i++) {
			double t = randfrom(0, 1);
			function<int(void*, ShotHandler::Shot*)> func = [angle, tAngleDelta](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle + ((tAngleDelta == -1) ? 180+90 : 180+90), randfrom(0.5, 2));
				return 0;
			};
			addDynamicSubShot(nullptr, tPos + offset * t, getRandomShot(), getEnemyShotCollisionList(), func);
		}

	}

	static int mootS1(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 400 == 60) {
			addHammerAnimation(getScreenPositionFromGamePosition(0, 0, 0), 3000);
		}
		if (tShot->mCurrentFrame % 400 < 90) {
			updateHammer(getScreenPositionFromGamePosition(0, 0, 0), tShot->mCurrentFrame % 400, 1);
		}

		if (tShot->mCurrentFrame % 400 == 100) {
			addHammerAnimation(getScreenPositionFromGamePosition(1, 0, 0), 3001);
		}
		if (tShot->mCurrentFrame % 400 >= 100 && tShot->mCurrentFrame % 400 < 190) {
			updateHammer(getScreenPositionFromGamePosition(1, 0, 0), tShot->mCurrentFrame % 400 - 100, -1);
		}

		if (tShot->mCurrentFrame % 400 == 200) {
			addHammerAnimation(getScreenPositionFromGamePosition(0.5, 0, 0), 3002);
		}

		return 0;
	}

	static void addMootExplosion2(int tAngle, int tDelta)
	{
		Position p = getBossPosition();
		int amount = 20;
		for (int i = 0; i < amount; i++) {
			Position offset = vecRotateZ(makePosition(0, 10, 0), (tAngle / 360.0) * 2 * M_PI);
			int angle = (360 / amount) * i + tAngle;;
			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle, 1.5);
				return 0;
			};
			addDynamicSubShot(nullptr, p + offset, "enemy_mid", getEnemyShotCollisionList(), func);
		}
	}

	static int mootNS2(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 10 == 0) {
			addMootExplosion2(tShot->mCurrentFrame + tShot->mCurrentFrame / 10, tShot->mCurrentFrame / 10);
		}

		return 0;
	}

	static void addStopStartShot(Position tPos) {
		function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
			if (tShot->mCurrentFrame == 0) {
				setShotNormalAngle(tShot, randfromInteger(0, 359), 1);
			}
			else if (tShot->mCurrentFrame % 60 == 0) {
				setShotAimedAngle(nullptr, tShot, 0, 1);
			}
			
			return tShot->mCurrentFrame > 200;
		};
		addDynamicSubShot(nullptr, tPos, "enemy_mid", getEnemyShotCollisionList(), func);
	}

	static int mootS2(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 10 == 0) {
			int amount = 3;
			auto pos = getBossPosition();
			for (int i = 0; i < amount; i++) {
				addStopStartShot(pos);
			}
		}
		return 0;
	}

	static void addRecursiveExplodeShot(Position tPos, int tDelta, int depth = 0) {
		int amount = 3;
		for (int i = 0; i < amount; i++) {
			int angle = (360 / amount) * i + tDelta;
			function<int(void*, ShotHandler::Shot*)> func = [angle, tDelta, depth](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle, 1);
				if (depth < 4 && tShot->mCurrentFrame == 40) {
					addRecursiveExplodeShot(getBlitzEntityPosition(tShot->mEntityID), tDelta, depth+1);
					return 1;
				}
				return 0;
			};
			addDynamicSubShot(nullptr, tPos, "enemy_mid", getEnemyShotCollisionList(), func);
		}
	}

	static int mootNS3(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 30 == 0) {
			addRecursiveExplodeShot(getBossPosition(), (tShot->mCurrentFrame / 30) * 11);
		}
		return 0;
	}

	static void addMootCircularShot(int tDelta) {
		int amount = 20;
		Position pos = getBossPosition();
		for (int i = 0; i < amount; i++) {
			int angle = (360 / amount) * i + tDelta;
			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle, 1);
				return 0;
			};
			addDynamicSubShot(nullptr, pos, "enemy_mid", getEnemyShotCollisionList(), func);
		}
	}

	static int mootS3(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 50 == 0) {
			addMootCircularShot((tShot->mCurrentFrame / 50) * 11);
		}
		return 0;
	}

	static void addSnacksCircularShot() {
		int amount = 16;
		Position pos = getSnacksPosition();
		for (int i = 0; i < amount; i++) {
			int angle = (360 / amount) * i;
			function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, angle, 2);
				return 0;
			};
			addDynamicSubShot(nullptr, pos, "enemy_mid", getEnemyShotCollisionList(), func);
		}
	}

	static int snacksS3(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 50 == 0) {
			addSnacksCircularShot();
		}
		return 0;
	}

	static int mootS4(void* tCaller, ShotHandler::Shot* tShot) {
		int amount = 2;
		Position pos = getSnacksPosition();
		for (int i = 0; i < amount; i++) {
			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				setShotAimedAngleBegin(nullptr, tShot, randfromInteger(-60, 60), randfrom(0.5, 2.5));
				return 0;
			};
			addDynamicSubShot(nullptr, pos, getRandomShot(), getEnemyShotCollisionList(), func);
		}

		return 1;
	}

	static int mootS5(void* tCaller, ShotHandler::Shot* tShot) {
		Position target = getAnonPosition(tCaller);
		Position p = getBossPosition();
		function<int(void*, ShotHandler::Shot*)> func = [target](void* tCaller, ShotHandler::Shot* tShot) {
			if (tShot->mCurrentFrame == 0) {
				setShotAimedTowardsPosition(nullptr, tShot, 0, 2, target);
			}
			return 0;
		};
		addDynamicSubShot(nullptr, p, getRandomShot(), getEnemyShotCollisionList(), func);
		return 1;
	}

	static void addShowerShot(int tDelta, double tPosition, int tAngle) {
		int amount = 20;
		for (int i = 0; i < amount; i++) {
			Position pos = getScreenPositionFromGamePosition(tPosition, i / double(amount), 0) + makePosition(0, (tDelta % 20) - 10, 0);
			function<int(void*, ShotHandler::Shot*)> func = [tAngle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, tAngle, 0.5);
				return 0;
			};
			addDynamicSubShot(nullptr, pos, "enemy_slim", getEnemyShotCollisionList(), func);
		}

	}

	static void addBlockingShot(int tDelta, double tPosition, int tAngle) {
		int amount = 10;
		for (int i = 0; i < amount; i++) {
			Position pos = getScreenPositionFromGamePosition(tPosition, (i / double(amount)) * 0.5, 0) + makePosition(0, (tDelta % 20) - 10, 0);
			function<int(void*, ShotHandler::Shot*)> func = [tAngle](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, tAngle, 0.5);
				return 0;
			};
			addDynamicSubShot(nullptr, pos, "enemy_slim", getEnemyShotCollisionList(), func);
		}

	}

	static int mootNS6(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 30 == 0) {
			addShowerShot(tShot->mCurrentFrame / 30, 0, -45);
			addShowerShot(tShot->mCurrentFrame / 30, 1, 45);
			addBlockingShot(tShot->mCurrentFrame / 30, 0, -90);
		}
		return 0;
	}

	static int mootS6(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 40 == 0) {
			addMootExplosion2(tShot->mCurrentFrame + tShot->mCurrentFrame / 40, tShot->mCurrentFrame / 40);
		}

		return 0;
	}

	static int alternativeS6(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 60 == 0) {
			addAngledLaser(getBlitzEntityPosition(tShot->mEntityID), randfromInteger(0, 359));
		}
		return 0;
	}

	static int yournamehereS6(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 30 == 0) {
			Position pos = getBlitzEntityPosition(tShot->mEntityID);
			int amount = 5;
			for (int i = 0; i < amount; i++) {
				function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
					setShotAimedAngleBegin(nullptr, tShot, randfromInteger(-45, 45), randfrom(0.5, 1.0));
					return 0;
				};
				addDynamicSubShot(nullptr, pos, "enemy_slim", getEnemyShotCollisionList(), func);
			}
		}
		return 0;
	}

	static int kinomodS6(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 30 == 0) {
			Position pos = getBlitzEntityPosition(tShot->mEntityID);
			int amount = 5;
			for (int i = 0; i < amount; i++) {
				int angle = -45 + (90 / amount) * i;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					setShotNormalAngleBegin(tShot, angle, 1.5);
					return 0;
				};
				addDynamicSubShot(nullptr, pos, getRandomShot(), getEnemyShotCollisionList(), func);
			}
		}
		return 0;
	}

	static int aeroliteS6(void* tCaller, ShotHandler::Shot* tShot) {
		Position pos = getBlitzEntityPosition(tShot->mEntityID);
		int amount = 1;
		for (int i = 0; i < amount; i++) {
			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				setShotNormalAngleBegin(tShot, randfromInteger(-80, 90), randfrom(0.5, 1.0));
				return 0;
			};
			addDynamicSubShot(nullptr, pos, getRandomShot(), getEnemyShotCollisionList(), func);
		}
		return 0;
	}

	static int mootNS7(void* tCaller, ShotHandler::Shot* tShot) {
		int freq = 60;
		if (tShot->mCurrentFrame % freq == 0) {
			int amount = 50;
			int doesTurn = randfromInteger(0, 3) <= 2;
			int delta = tShot->mCurrentFrame / freq;
			Position pos = getBossPosition();
			for (int i = 0; i < amount; i++) {
				int angle = (360 / amount) * i + delta + 180;
				function<int(void*, ShotHandler::Shot*)> func = [doesTurn, angle](void* tCaller, ShotHandler::Shot* tShot) {
					AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
					if (tShot->mCurrentFrame == 0) {
						data->angle = angle;
					}

					const int when = 80;
					if (doesTurn && tShot->mCurrentFrame >= when && tShot->mCurrentFrame < when + 10) {
						data->angle += 18;
						data->speed -= 0.1;
					}
					setShotNormalAngle(tShot, data->angle, 2);

					return 0;
				};
				addDynamicSubShot(nullptr, pos, getRandomShot(), getEnemyShotCollisionList(), func);
			}
		}
		return 0;
	}

	static int isBossInCorner(Position tPos) {
		return fabs(tPos.x - getScreenPositionFromGamePositionX(0)) < 1 || fabs(tPos.x - getScreenPositionFromGamePositionX(1)) < 1;
	}

	static void addMootS7CornerShot(ShotHandler::Shot* tShot) {
		int freq = 40;
		if (tShot->mCurrentFrame % freq == 0) {
			int amount = 20;
			int delta = tShot->mCurrentFrame / freq;
			Position pos = getBossPosition();
			for (int i = 0; i < amount; i++) {
				int angle = (360 / amount) * i + delta;
				function<int(void*, ShotHandler::Shot*)> func = [angle](void* tCaller, ShotHandler::Shot* tShot) {
					AusNS1Data* data = (AusNS1Data*)tShot->mUserData;
					if (tShot->mCurrentFrame == 0) {
						data->speed = randfrom(1, 2);
					}
					if (tShot->mCurrentFrame >= 120 && tShot->mCurrentFrame < 1000) {
						data->speed = 0;
					}
					if (tShot->mCurrentFrame == 1000) {
						data->speed = 0;
					}
					else {
						data->speed = randfrom(1, 2);
					}
					setShotNormalAngleBegin(tShot, angle, data->speed);

					return 0;
				};
				addDynamicSubShot(nullptr, pos, getRandomShot(), getEnemyShotCollisionList(), func);
			}
		}
	}

	static void addMootS7MovementShot() {
		Position pos = getBossPosition();
		int amount = 2;
		for (int i = 0; i < amount; i++) {
			function<int(void*, ShotHandler::Shot*)> func = [](void* tCaller, ShotHandler::Shot* tShot) {
				setShotAimedAngleBegin(nullptr, tShot, randfromInteger(-70, 70), randfrom(1, 2));
				return 0;
			};
			addDynamicSubShot(nullptr, pos, "enemy_slim", getEnemyShotCollisionList(), func);
		}
	}

	static int mootS7(void* tCaller, ShotHandler::Shot* tShot) {
		Position pos = getBossPosition();
		if (isBossInCorner(pos)) {
			addMootS7CornerShot(tShot);
		}
		else {
			addMootS7MovementShot();
		}


		return 0;
	}

	static void addOneFrameShot(const string& tName, Position tPosition, int tAngle) {
		function<int(void*, ShotHandler::Shot*)> func = [tAngle](void* tCaller, ShotHandler::Shot* tShot) {
			setShotNormalAngleBegin(tShot, tAngle, 0);
			return 1;
		};
		addDynamicSubShot(nullptr, tPosition, tName, getEnemyShotCollisionList(), func);
	}

	static Position vecRotateZ360Game(Position tPosition, int tAngle) {
		double angle = (tAngle / 360.0) * 2 * M_PI;
		return vecRotateZ(tPosition, angle);
	}

	static void createCloverSide(Position tPosition, int tAngle, int length) {
		Position direction;
		direction = vecRotateZ360Game(makePosition(0, length, 0), tAngle);
		addOneFrameShot("enemy_mid", tPosition + direction, tAngle);

		direction = vecRotateZ360Game(makePosition(0, length * 0.75, 0), tAngle - 20);
		addOneFrameShot("enemy_mid", tPosition + direction, tAngle);

		direction = vecRotateZ360Game(makePosition(0, length * 0.75, 0), tAngle + 20);
		addOneFrameShot("enemy_mid", tPosition + direction, tAngle);

		direction = vecRotateZ360Game(makePosition(0, length * 0.5, 0), tAngle - 40);
		addOneFrameShot("enemy_mid", tPosition + direction, tAngle);

		direction = vecRotateZ360Game(makePosition(0, length * 0.5, 0), tAngle - 40);
		addOneFrameShot("enemy_mid", tPosition + direction, tAngle);

		direction = vecRotateZ360Game(makePosition(0, length * 0.25, 0), tAngle - 20);
		addOneFrameShot("enemy_mid", tPosition + direction, tAngle);

		direction = vecRotateZ360Game(makePosition(0, length * 0.25, 0), tAngle + 20);
		addOneFrameShot("enemy_mid", tPosition + direction, tAngle);

		direction = vecRotateZ360Game(makePosition(0, length * 0.1, 0), tAngle);
		addOneFrameShot("enemy_mid", tPosition + direction, tAngle);
	}

	static void createClover(Position tPosition, int tAngle, int tLength = 100) {
		createCloverSide(tPosition, tAngle, tLength);
		createCloverSide(tPosition, tAngle + 90, tLength);
		createCloverSide(tPosition, tAngle + 180, tLength);
		createCloverSide(tPosition, tAngle + 270, tLength);
	}

	typedef struct {
		Position mTargetPosition;
		Position mDirection;
		Position mPosition;
		int mAngle;
	} RotatingClover;

	static void addRotatingClover(Position tStartPosition, Position tEndPosition, int tStartAngle = 0) {
		function<int(void*, ShotHandler::Shot*)> func = [tStartPosition, tEndPosition, tStartAngle](void* tCaller, ShotHandler::Shot* tShot) {
			RotatingClover* data = (RotatingClover*)tShot->mUserData;
			if (tShot->mCurrentFrame == 0) {
				data->mTargetPosition = tEndPosition;
				data->mPosition = tStartPosition;
				data->mDirection = tEndPosition - tStartPosition;
				data->mDirection.z = 0;
				data->mDirection = vecNormalize(data->mDirection);
				data->mAngle = tStartAngle;
			}

			createClover(data->mPosition, data->mAngle);
			data->mPosition += data->mDirection;
			data->mAngle += 2;
			int isClose = getDistance2D(data->mPosition, data->mTargetPosition) < 4;
			return isClose || isPlayerBombActive();
		};
		addDynamicSubShot(nullptr, makePosition(100, 100, 0), "invisible", getEnemyShotCollisionList(), func);
	}

	static int mootS8(void* tCaller, ShotHandler::Shot* tShot) {
		if (tShot->mCurrentFrame % 3000 == 60 * 3) {
			addRotatingClover(getScreenPositionFromGamePosition(-0.2, 0.5, 0), getScreenPositionFromGamePosition(1.2, 0.5, 0));
		}

		if (tShot->mCurrentFrame % 3000 == 180 * 3) {
			addRotatingClover(getScreenPositionFromGamePosition(-0.2, -0.2, 0), getScreenPositionFromGamePosition(1.2, 1.2, 0));
			addRotatingClover(getScreenPositionFromGamePosition(1.2, -0.2, 0), getScreenPositionFromGamePosition(-0.2, 1.2, 0));
		}

		if (tShot->mCurrentFrame % 3000 == 360 * 3) {
			int amount = 4;
			for (int i = 0; i <= amount; i++) {
				double y = i / double(amount);
				addRotatingClover(getScreenPositionFromGamePosition(-0.2, y, 0), getScreenPositionFromGamePosition(1.2, y, 0));
			}
		}

		if (tShot->mCurrentFrame%3000 == 470 * 3) {
			int amount = 4;
			for (int i = 0; i <= amount; i++) {
				double x = i / double(amount);
				addRotatingClover(getScreenPositionFromGamePosition(x, -0.2, 0), getScreenPositionFromGamePosition(x, 1.2, 0));
			}
		}

		if (tShot->mCurrentFrame % 3000 == 600 * 3) {
			addRotatingClover(getScreenPositionFromGamePosition(-0.2, -0.2, 0), getScreenPositionFromGamePosition(1.2, 1.2, 0));
			addRotatingClover(getScreenPositionFromGamePosition(1.2, -0.2, 0), getScreenPositionFromGamePosition(-0.2, 1.2, 0));
			addRotatingClover(getScreenPositionFromGamePosition(1.2, 1.2, 0), getScreenPositionFromGamePosition(-0.2, -0.2, 0));
			addRotatingClover(getScreenPositionFromGamePosition(-0.2, 1.2, 0), getScreenPositionFromGamePosition(1.2, -0.2, 0));

			addRotatingClover(getScreenPositionFromGamePosition(-0.2, 0.5, 0), getScreenPositionFromGamePosition(1.2, 0.5, 0));
			addRotatingClover(getScreenPositionFromGamePosition(0.5, -0.2, 0), getScreenPositionFromGamePosition(0.5, 1.2, 0));
		}

		if (tShot->mCurrentFrame % 3000 == 720*3) {
			int amount = 3;
			for (int i = 0; i <= amount; i++) {
				double y = i / double(amount);
				addRotatingClover(getScreenPositionFromGamePosition(-0.2, y, 0), getScreenPositionFromGamePosition(1.2, y, 0));
			}

			for (int i = 0; i <= amount; i++) {
				double x = i / double(amount);
				addRotatingClover(getScreenPositionFromGamePosition(x, -0.2, 0), getScreenPositionFromGamePosition(x, 1.2, 0));
			}
		}

		return 0;
	}

	typedef struct {
		double scaleDirection;
		double scale;
		int mAngle;
	} ScaleClover;

	static void addScalingClover()
	{
		Position pos = getBossPosition();
		function<int(void*, ShotHandler::Shot*)> func = [pos](void* tCaller, ShotHandler::Shot* tShot) {
			ScaleClover* data = (ScaleClover*)tShot->mUserData;
			if (tShot->mCurrentFrame == 0) {
				data->scaleDirection = 1;
				data->scale = 1;
				data->mAngle = 0;
			}

			createClover(pos + makePosition(0,0,0), data->mAngle, int(data->scale * 10));
			data->scale += data->scaleDirection * 0.1;
			if (data->scaleDirection > 0 && data->scale > 20) data->scaleDirection *= -1;
			if (data->scaleDirection < 0 && data->scale < 0) data->scaleDirection *= -1;

			data->mAngle += 2;
			return 0;
		};
		addDynamicSubShot(nullptr, makePosition(100, 100, 0), "invisible", getEnemyShotCollisionList(), func);
	}

	static int mootS9(void* tCaller, ShotHandler::Shot* tShot) {
		if(tShot->mCurrentFrame < 1250 && tShot->mCurrentFrame % 250 == 0)
		{
			addScalingClover();
		}

		return 0;
	}

	void loadShotGimmicks() {
		mGimmicks["yournamehereStraight"] = (void*)yournamehereStraight;
		mGimmicks["yournamehereStrong"] = (void*)yournamehereStrong;
		mGimmicks["yournamehereHoming"] = (void*)yournamehereHoming;
		mGimmicks["kinomodUnfocused1"] = (void*)kinomodUnfocused1;
		mGimmicks["kinomodUnfocused2"] = (void*)kinomodUnfocused2;
		mGimmicks["kinomodUnfocused3"] = (void*)kinomodUnfocused3;
		mGimmicks["kinomodUnfocused4"] = (void*)kinomodUnfocused4;
		mGimmicks["kinomodFocused1"] = (void*)kinomodFocused1;
		mGimmicks["kinomodFocused2"] = (void*)kinomodFocused2;
		mGimmicks["kinomodFocused3"] = (void*)kinomodFocused3;
		mGimmicks["kinomodFocused4"] = (void*)kinomodFocused4;
		mGimmicks["aeroliteUnfocused1"] = (void*)aeroliteUnfocused1;
		mGimmicks["aeroliteUnfocused2"] = (void*)aeroliteUnfocused2;
		mGimmicks["aeroliteUnfocused3"] = (void*)aeroliteUnfocused3;
		mGimmicks["aeroliteUnfocused4"] = (void*)aeroliteUnfocused4;
		mGimmicks["enemyStraight"] = (void*)enemyStraight;
		mGimmicks["enemyAimed"] = (void*)enemyAimed;
		mGimmicks["enemyAimedWide"] = (void*)enemyAimedWide;
		mGimmicks["enemyWideningAngle"] = (void*)enemyWideningAngle;
		mGimmicks["enemyAimedPlus10"] = (void*)enemyAimed10;
		mGimmicks["enemyAimedPlus20"] = (void*)enemyAimed20;
		mGimmicks["enemyAimedPlus30"] = (void*)enemyAimed30;
		mGimmicks["enemyAimedPlus40"] = (void*)enemyAimed40;
		mGimmicks["enemyAimedPlus50"] = (void*)enemyAimed50;
		mGimmicks["enemyAimedPlus60"] = (void*)enemyAimed60;
		mGimmicks["enemyAimedPlus70"] = (void*)enemyAimed70;
		mGimmicks["enemyAimedPlus80"] = (void*)enemyAimed80;
		mGimmicks["enemyAimedPlus90"] = (void*)enemyAimed90;
		mGimmicks["enemyAimedPlus100"] = (void*)enemyAimed100;
		mGimmicks["enemyAimedPlus110"] = (void*)enemyAimed110;
		mGimmicks["enemyAimedPlus120"] = (void*)enemyAimed120;
		mGimmicks["enemyAimedPlus130"] = (void*)enemyAimed130;
		mGimmicks["enemyAimedPlus140"] = (void*)enemyAimed140;
		mGimmicks["enemyAimedPlus150"] = (void*)enemyAimed150;
		mGimmicks["enemyAimedPlus160"] = (void*)enemyAimed160;
		mGimmicks["enemyAimedPlus170"] = (void*)enemyAimed170;
		mGimmicks["enemyAimedPlus180"] = (void*)enemyAimed180;
		mGimmicks["enemyAimedPlus190"] = (void*)enemyAimed190;
		mGimmicks["enemyAimedPlus200"] = (void*)enemyAimed200;
		mGimmicks["enemyAimedPlus210"] = (void*)enemyAimed210;
		mGimmicks["enemyAimedPlus220"] = (void*)enemyAimed220;
		mGimmicks["enemyAimedPlus230"] = (void*)enemyAimed230;
		mGimmicks["enemyAimedPlus240"] = (void*)enemyAimed240;
		mGimmicks["enemyAimedPlus250"] = (void*)enemyAimed250;
		mGimmicks["enemyAimedPlus260"] = (void*)enemyAimed260;
		mGimmicks["enemyAimedPlus270"] = (void*)enemyAimed270;
		mGimmicks["enemyAimedPlus280"] = (void*)enemyAimed280;
		mGimmicks["enemyAimedPlus290"] = (void*)enemyAimed290;
		mGimmicks["enemyAimedPlus300"] = (void*)enemyAimed300;
		mGimmicks["enemyAimedPlus310"] = (void*)enemyAimed310;
		mGimmicks["enemyAimedPlus320"] = (void*)enemyAimed320;
		mGimmicks["enemyAimedPlus330"] = (void*)enemyAimed330;
		mGimmicks["enemyAimedPlus340"] = (void*)enemyAimed340;
		mGimmicks["enemyAimedPlus350"] = (void*)enemyAimed350;
		mGimmicks["enemyLaserAimed"] = (void*)enemyLaserAimed;
		mGimmicks["enemyLaserAimedSlow"] = (void*)enemyLaserAimedSlow;
		mGimmicks["enemyLaserStraight"] = (void*)enemyLaserStraight;
		mGimmicks["enemyLaserCircle"] = (void*)enemyLaserCircle;
		mGimmicks["enemyLowerHalf"] = (void*)enemyLowerHalf;
		mGimmicks["enemyLowerHalfSlow"] = (void*)enemyLowerHalfSlow;
		mGimmicks["enemyRandom"] = (void*)enemyRandom;
		mGimmicks["enemyLilyWhite"] = (void*)enemyLilyWhite;
		mGimmicks["barney1"] = (void*)barney1;
		mGimmicks["ack1"] = (void*)ack1;
		mGimmicks["enemyFiveAimedSlow"] = (void*)enemyFiveAimedSlow;
		mGimmicks["enemyFiveAimedDifferent"] = (void*)enemyFiveAimedDifferent;
		mGimmicks["enemyCircleMid"] = (void*)enemyCircleMid;
		mGimmicks["enemyCircleSlim"] = (void*)enemyCircleSlim;
		mGimmicks["enemyPCB"] = (void*)enemyPCB;
		mGimmicks["enemyPCB2"] = (void*)enemyPCB2;
		mGimmicks["enemySlightDownLeft"] = (void*)enemySlightDownLeft;
		mGimmicks["enemySlightDownRight"] = (void*)enemySlightDownRight;
		mGimmicks["accMid"] = (void*)accMid;
		mGimmicks["accNS1"] = (void*)accNS1;
		mGimmicks["accS1"] = (void*)accS1;
		mGimmicks["accNS2"] = (void*)accNS2;
		mGimmicks["accS2"] = (void*)accS2;
		mGimmicks["woaznNS"] = (void*)woaznNS;
		mGimmicks["woaznS"] = (void*)woaznS;
		mGimmicks["ausNS1"] = (void*)ausNS1;
		mGimmicks["ausS1"] = (void*)ausS1;
		mGimmicks["ausNS2"] = (void*)ausNS2;
		mGimmicks["ausS2"] = (void*)ausS2;
		mGimmicks["ausNS3"] = (void*)ausNS3;
		mGimmicks["ausS31"] = (void*)ausS31;
		mGimmicks["ausS33"] = (void*)ausS33;
		mGimmicks["ausS34"] = (void*)ausS34;
		mGimmicks["ausS4"] = (void*)ausS4;
		mGimmicks["shiiNS1"] = (void*)shiiNS1;
		mGimmicks["shiiS1"] = (void*)shiiS1;
		mGimmicks["shiiNS2"] = (void*)shiiNS2;
		mGimmicks["shiiS2"] = (void*)shiiS2;
		mGimmicks["shiiNS3"] = (void*)shiiNS3;
		mGimmicks["shiiS3"] = (void*)shiiS3;
		mGimmicks["shiiNS4"] = (void*)shiiNS4;
		mGimmicks["shiiS4"] = (void*)shiiS4;
		mGimmicks["shiiS5"] = (void*)shiiS5;
		mGimmicks["enemyFinal1"] = (void*)enemyFinal1;
		mGimmicks["enemyFinal2"] = (void*)enemyFinal2;
		mGimmicks["enemyFinal3"] = (void*)enemyFinal3;
		mGimmicks["shiiMidNS1"] = (void*)shiiMidNS1;
		mGimmicks["shiiMidS1"] = (void*)shiiMidS1;
		mGimmicks["hiroNS1"] = (void*)hiroNS1;
		mGimmicks["hiroS1"] = (void*)hiroS1;
		mGimmicks["hiroNS2"] = (void*)hiroNS2;
		mGimmicks["hiroS2"] = (void*)hiroS2;
		mGimmicks["hiroNS3"] = (void*)hiroNS3;
		mGimmicks["hiroS3"] = (void*)hiroS3;
		mGimmicks["hiroNS4"] = (void*)hiroNS4;
		mGimmicks["hiroS4"] = (void*)hiroS4;
		mGimmicks["hiroNS5"] = (void*)hiroNS5;
		mGimmicks["hiroS5"] = (void*)hiroS5;
		mGimmicks["hiroS61"] = (void*)hiroS6_1;
		mGimmicks["hiroS62"] = (void*)hiroS6_2;
		mGimmicks["hiroS63"] = (void*)hiroS6_3;
		mGimmicks["crisis"] = (void*)crisis;
		mGimmicks["enemyFinal4"] = (void*)enemyFinal4;
		mGimmicks["enemyFinal5"] = (void*)enemyFinal5;
		mGimmicks["enemyFinal6"] = (void*)enemyFinal6;
		mGimmicks["alternativeNS1"] = (void*)alternativeNS1;
		mGimmicks["alternativeS11"] = (void*)alternativeS11;
		mGimmicks["alternativeS12"] = (void*)alternativeS12;
		mGimmicks["alternativeS2"] = (void*)alternativeS2;
		mGimmicks["mootNS1"] = (void*)mootNS1;
		mGimmicks["mootS1"] = (void*)mootS1;
		mGimmicks["mootNS2"] = (void*)mootNS2;
		mGimmicks["mootS2"] = (void*)mootS2;
		mGimmicks["mootNS3"] = (void*)mootNS3;
		mGimmicks["mootS3"] = (void*)mootS3;
		mGimmicks["snacksS3"] = (void*)snacksS3;
		mGimmicks["mootS4"] = (void*)mootS4;
		mGimmicks["mootS5"] = (void*)mootS5;
		mGimmicks["mootNS6"] = (void*)mootNS6;
		mGimmicks["alternativeS6"] = (void*)alternativeS6;
		mGimmicks["yournamehereS6"] = (void*)yournamehereS6;
		mGimmicks["kinomodS6"] = (void*)kinomodS6;
		mGimmicks["aeroliteS6"] = (void*)aeroliteS6;
		mGimmicks["mootS6"] = (void*)mootS6;
		mGimmicks["mootNS7"] = (void*)mootNS7;
		mGimmicks["mootS7"] = (void*)mootS7;
		mGimmicks["mootS8"] = (void*)mootS8;
		mGimmicks["mootS9"] = (void*)mootS9;
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
		const int delta = 15;
		return p.x < getScreenPositionFromGamePositionX(0) - delta || p.x > getScreenPositionFromGamePositionX(1) + delta || p. y < getScreenPositionFromGamePositionY(0) - delta || p.y > getScreenPositionFromGamePositionY(1) + delta;
	}

	int updateSingleShot(Shot& tShot) {
		if (tShot.mIsDoneAfterBeginning) return 1;

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

