#include "boss.h"

#include <prism/blitz.h>

#include "shothandler.h"
#include "collision.h"
#include "level.h"
#include "uihandler.h"
#include "dialoghandler.h"
#include "itemhandler.h"
#include "enemyhandler.h"
#include "debug.h"

#define BOSS_Z 20

static void bossHitCB(void* tCaller, void* tCollisionData);
static void customBossCBCaller(void* tCaller);
static void removeBoss();

struct Boss {
	int mEntityID;
	int mCurrentFrame;
	int mLife;
	int mLifeMax;
	int mPhasesLeft;

	int mNameTextID;
	int mNumberTextID;
	int mHealthBarBGID;
	int mHealthBarID;

	int mStage = -1;
	int mStageAmount = 10;
	int mSpellCard = 0;
	int mTimeInCard = -1;

	Boss() {
		mEntityID = addBlitzEntity(makePosition(0,0,0));
		mCurrentFrame = -1;
		mLife = 10;
		mLifeMax = 10;
		mPhasesLeft = 0;
		mNameTextID = addMugenTextMugenStyle("Dummy", makePosition(65, 20, BOSS_Z + 2), makeVector3DI(3, 0, 0));
		setMugenTextScale(mNameTextID, 0.6);
		setMugenTextColorRGB(mNameTextID, 0, 0, 0);
		mNumberTextID = addMugenTextMugenStyle("0", makePosition(26, 30, BOSS_Z + 2), makeVector3DI(2, 0, 1));
		Position pos = gGameVars.gameScreenOffset + makePosition(27, 15, BOSS_Z + 1);
		mHealthBarBGID = addMugenAnimation(getMugenAnimation(getUIAnimations(), 20), getUISprites(), pos);
		mHealthBarID = addMugenAnimation(getMugenAnimation(getUIAnimations(), 21), getUISprites(), pos + makePosition(0, 0, 1));
	}

	virtual ~Boss() {
		int id = addMugenAnimation(getMugenAnimation(getUIAnimations(), 16), getUISprites(), getBlitzEntityPosition(mEntityID) + makePosition(0, 0, 1));
		setMugenAnimationNoLoop(id);
		removeBlitzEntity(mEntityID);
		removeMugenAnimation(mHealthBarID);
		removeMugenAnimation(mHealthBarBGID);
		removeMugenText(mNumberTextID);
		removeMugenText(mNameTextID);
	}

	virtual Position getPosition() {
		return getBlitzEntityPosition(mEntityID);
	}

	void updateHealthBar() {
		setMugenAnimationRectangleWidth(mHealthBarID, int(getAnimationFirstElementSpriteSize(getMugenAnimation(getUIAnimations(), 21), getUISprites()).x * (mLife / double(mLifeMax))));
	}

	void updateTime() {
		mTimeInCard++;
	}

	void setLife(int tValue) {
		mLife = mLifeMax = tValue;
	}

	void updateStageDisplay() {
		int stagesLeft = mStageAmount - mStage - 1;
		stringstream ss;
		ss << stagesLeft;
		changeMugenText(mNumberTextID, ss.str().data());
	}

	void increaseStage(int tLife, int tPowerItems = 0, int tScoreItems = 0) {
		addPowerItems(getBlitzEntityPosition(mEntityID), tPowerItems);
		addScoreItems(getBlitzEntityPosition(mEntityID), tScoreItems);
		increaseStage();
		setLife(tLife);
		updateStageDisplay();
	}	
	
	void increaseStage() {
		mStage++;
		mSpellCard = 0;
		mTimeInCard = -1;
		removeEnemyBullets();
	}

	void increaseSpellcard(double tHealthFactor = 1) {
		mSpellCard++;
		mTimeInCard = -1;
		mLife = int(mLife*tHealthFactor);
		mLifeMax = int(mLifeMax*tHealthFactor);
		removeEnemyBullets();
	}

	void setBossName(const std::string& tName) {
		changeMugenText(mNameTextID, tName.data());
	}

	void hideHealthBar() {
		setMugenAnimationVisibility(mHealthBarID, 0);
		setMugenAnimationVisibility(mHealthBarBGID, 0);
		setMugenTextVisibility(mNumberTextID, 0);
		setMugenTextVisibility(mNameTextID, 0);
	}

	void reshowHealthBar() {
		setMugenAnimationVisibility(mHealthBarID, 1);
		setMugenAnimationVisibility(mHealthBarBGID, 1);
		setMugenTextVisibility(mNumberTextID, 1);
		setMugenTextVisibility(mNameTextID, 1);
	}

	virtual void update() = 0;
	virtual void hitCB() = 0;
	virtual void customCB() {};

	static int updateToPositionEntity(int tEntityID, Position tPos, double tSpeed) {
		Position* posReference = getBlitzEntityPositionReference(tEntityID);
		Position dx = tPos - getBlitzEntityPosition(tEntityID);
		if (vecLength(dx) < std::max(5.0, tSpeed * 2)) {
			*posReference += 0.2 * dx;
		}
		else {
			dx = vecNormalize(dx);
			*posReference += dx * tSpeed;
		}

		return getDistance2D(*posReference, tPos) < 1e-6;
	}

	int updateToPosition(Position tPos, double tSpeed) {
		return updateToPositionEntity(mEntityID, tPos, tSpeed);
	}

	void createAtPosition(Position tPos, int tAnimation, double tRadius) {
		setBlitzEntityPosition(mEntityID, tPos);
		addBlitzMugenAnimationComponent(mEntityID, getLevelSprites(), getLevelAnimations(), tAnimation);
		addBlitzCollisionComponent(mEntityID);
		int collisionID = addBlitzCollisionCirc(mEntityID, getEnemyCollisionList(), makeCollisionCirc(makePosition(0, 0, 0), tRadius));
		addBlitzCollisionCB(mEntityID, collisionID, bossHitCB, NULL);
	}

	void introToPosition(void(*tFunc)(void*), int tNow) {
		if (tNow == 0) {
			int id = addMugenAnimation(getMugenAnimation(getUIAnimations(), 100), getUISprites(), makePosition(96 + 16, 62 + 8, BOSS_Z));
			setMugenAnimationNoLoop(id);
			setMugenAnimationCallback(id, tFunc, this);
		}
		if (tNow == 80) {
			setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		}
	}

	void addDamage(int tDamage) {
		mLife = std::max(0, mLife - 1);
	}

	void hitCBStandard() {
		addDamage(1);
	}
};

static struct {
	int mIsActive;
	unique_ptr<Boss> mBoss;
} gBossData;

static void loadBossHandler(void* tData) {
	(void)tData;
	gBossData.mIsActive = 0;
	gBossData.mBoss = nullptr;
}

static void unloadBossHandler(void* tData) {
	(void)tData;
	gBossData.mBoss = nullptr;
}

static void updateBossHandler(void* tData) {
	(void)tData;
	if (!gBossData.mIsActive) return;

	gBossData.mBoss->mCurrentFrame++;
	gBossData.mBoss->updateHealthBar();
	gBossData.mBoss->updateTime();
	gBossData.mBoss->update();
}

ActorBlueprint getBossHandler()
{
	return makeActorBlueprint(loadBossHandler, unloadBossHandler, updateBossHandler);
}

static void bossHitCB(void* tCaller, void* tCollisionData) {
	(void)tCaller;
	(void)tCollisionData;
	if (!gBossData.mIsActive) return;
	gBossData.mBoss->hitCB();
}

static void removeBoss() {
	gBossData.mIsActive = 0;
	gBossData.mBoss = nullptr;
}

static void customBossCBCaller(void* tCaller) {
	(void)tCaller;
	if (!gBossData.mIsActive) return;
	gBossData.mBoss->customCB();
}

struct DummyBoss : Boss {

	DummyBoss() {
		createAtPosition(makePosition(-100, -100, BOSS_Z), 1000, 5);
		mStage = 0;
		hideHealthBar();
	}

	virtual void customCB() override {
		if(mStage == 0)mStage = 1;
		else if (mStage == 2) mStage = 4;
	}

	virtual void update() override {
		if (mStage == 0) {
			introToPosition(customBossCBCaller, mCurrentFrame);
		}
		else if(mStage == 1){
			reshowHealthBar();
			startPreDialog();
			mStage = 2;
		}
		else if(mStage == 4){
			removeBoss();
		}

		if (mLife == 0) removeBoss();
	}

	virtual void hitCB() override {
		hitCBStandard();
	}

};

struct BarneyBoss : Boss {
	

	BarneyBoss() {
		createAtPosition(getScreenPositionFromGamePosition(makePosition(-0.1, 0.1, BOSS_Z)), 1001, 13);
		hideHealthBar();
		setBossName("Barney");
		mStageAmount = 1;
	}

	~BarneyBoss() {
		addPowerItems(getBlitzEntityPosition(mEntityID), 38);
		addScoreItems(getBlitzEntityPosition(mEntityID), 17);
		addBombItem(getBlitzEntityPosition(mEntityID));
	}

	virtual void customCB() override {
		if (mStage == 0)mStage = 1;
		else if (mStage == 2) mStage = 4;
	}

	void updateSpellcard0() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "barney1", getEnemyShotCollisionList());
		}
	}

	int mSpellcard1Stage;
	int mSpellTime;
	void updateSpellcard1() {
		if (mTimeInCard == 0) {
			mSpellcard1Stage = 0;
		}

		Position p = getBlitzEntityPosition(mEntityID);
		//p += vecRotateZ(makePosition(10, 0, 0), mTimeInCard);
		if (mSpellcard1Stage == 0) {
			if (updateToPosition(getScreenPositionFromGamePosition(0.3, 0.2, 20), 3)) {
				mSpellcard1Stage = 1;
				mSpellTime = 0;
			}
		} else if (mSpellcard1Stage == 1) {
			if (mSpellTime == 200) {
				mSpellTime = 0;
				mSpellcard1Stage = 2;
			}
		} else if (mSpellcard1Stage == 2) {
			if (updateToPosition(getScreenPositionFromGamePosition(0.7, 0.2, 20), 3)) {
				mSpellcard1Stage = 3;
				mSpellTime = 0;
			}
		}
		else if (mSpellcard1Stage == 3) {
			if (mSpellTime == 200) {
				mSpellTime = 0;
				mSpellcard1Stage = 0;
			}
		}

		mSpellTime++;
		if (mSpellcard1Stage < 2) {	
			if(mTimeInCard % 10 == 0) addAimedShot(this, p, "enemy_mid", getEnemyShotCollisionList(), randfromInteger(-45, 45), 0.75);
		}
		else {
			if (mTimeInCard % 15 == 0) {
				int randomCenter = randfromInteger(-20, 20);
				addAimedShot(this, p, "enemy_mid", getEnemyShotCollisionList(), randomCenter-20, 1);
				addAimedShot(this, p, "enemy_mid", getEnemyShotCollisionList(), randomCenter-10, 1);
				addAimedShot(this, p, "enemy_mid", getEnemyShotCollisionList(), randomCenter, 1);
				addAimedShot(this, p, "enemy_mid", getEnemyShotCollisionList(), randomCenter+10, 1);
				addAimedShot(this, p, "enemy_mid", getEnemyShotCollisionList(), randomCenter+20, 1);
			}
		}
		
	}

	virtual void update() override {
		if (mStage == -1) {
			if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, 20), 3)) {
				increaseStage(600);
				reshowHealthBar();
			}
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {
			
				updateSpellcard0();

				if (mLife <= mLifeMax / 2) {
					increaseSpellcard(2);
				}
			}
			else {
				updateSpellcard1();

				if (mLife == 0) increaseStage();
			}
		}
		else {
			removeBoss();
		}
	}

	virtual void hitCB() override {
		if (mStage < 0) return;
		hitCBStandard();
	}
};

struct AckBoss : Boss {


	AckBoss() {
		createAtPosition(getScreenPositionFromGamePosition(makePosition(-0.1, -0.1, BOSS_Z)), 1002, 13);
		hideHealthBar();
		setBossName("ACK");
		mStageAmount = 2;

		setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		mStage = 1;
	}

	~AckBoss() {
		startPostDialog();
	}

	virtual void customCB() override {
		if (mStage == -1) {
			reshowHealthBar();
			increaseStage(600);
		}
	}

	void updateNonSpell1() {
		if (mTimeInCard % 10 == 0) {
			int amount = 20;
			Position p = getBlitzEntityPosition(mEntityID);
			for (int i = 0; i < amount; i++) {
				int angle = i * (360 / amount);
				addAngledShot(this, p + vecRotateZ(makePosition(1, 0, 0), mTimeInCard), "enemy_slim", getEnemyShotCollisionList(), angle + mTimeInCard, 2);
			}
		}
	}

	void updateSpell1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "ack1", getEnemyShotCollisionList());
		}
	}

	void updateNonSpell2() {
		if (mTimeInCard % 10 == 0) {
			Position p1 = getBlitzEntityPosition(mEntityID) - vecRotateZ(makePosition(40, 0, 0), 0);
			Position p2 = getBlitzEntityPosition(mEntityID) + vecRotateZ(makePosition(40, 0, 0), 0);
				addAimedShot(this, p1 + vecRotateZ(makePosition(20, 0, 0), mTimeInCard), "enemy_slim", getEnemyShotCollisionList(), 0, 2);
				addAimedShot(this, p2 + vecRotateZ(makePosition(20, 0, 0), mTimeInCard), "enemy_slim", getEnemyShotCollisionList(), 0, 2);
		}
	}

	int mEntityID2;
	int mSpellcard2Stage = 0;
	void updateSpell2() {
		if (mSpellcard2Stage == 0) {
			if (mTimeInCard == 0) {
				mEntityID2 = addBlitzEntity(makePosition(96 + 16, 62 + 8, BOSS_Z));
				addBlitzMugenAnimationComponent(mEntityID2, getLevelSprites(), getLevelAnimations(), 1002);
				addBlitzCollisionComponent(mEntityID2);
				int collisionID = addBlitzCollisionCirc(mEntityID2, getEnemyCollisionList(), makeCollisionCirc(makePosition(0, 0, 0), 13));
				addBlitzCollisionCB(mEntityID2, collisionID, bossHitCB, NULL);
			}
			updateToPositionEntity(mEntityID, makePosition(96 + 16 + 50, 62 + 8, BOSS_Z), 2);
			if (updateToPositionEntity(mEntityID2, makePosition(96 + 16 - 50, 62 + 8, BOSS_Z), 2)) {
				mSpellcard2Stage++;
			}
		}
		else {
			if (mTimeInCard % 30 == 0) {
				int amount = 15;
				Position p = getBlitzEntityPosition(mEntityID);
				for (int i = 0; i < amount; i++) {
					int angle = i * (360 / amount);
					addAngledShot(this, p, "enemy_mid", getEnemyShotCollisionList(), angle + mTimeInCard, 0.5);
				}
			}

			if (mTimeInCard % 20 == 0) {
				Position p = getBlitzEntityPosition(mEntityID2);
				addAimedShot(this, p, "enemy_slim", getEnemyShotCollisionList(), randfromInteger(-10, 10), 1);
			}

		}
	}

	virtual void update() override {
		if (mStage == -1) {
			if (mTimeInCard == 0) startPreDialog();
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {
				updateNonSpell1();

				if (mLife <= mLifeMax / 2) {
					increaseSpellcard(2);
				}
			}
			else {
				updateSpell1();

				if (!mLife) {
					increaseStage(800, 25, 10);
				}
			}
		}
		else if (mStage == 1) {
			if (mSpellCard == 0) {
				updateNonSpell2();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				updateSpell2();

				if (!mLife) {
					removeBlitzEntity(mEntityID2);
					increaseStage();
				}
			}
		}
		else {
			removeBoss();
		}
	}

	virtual void hitCB() override {
		if (mStage < 0) return;
		hitCBStandard();
	}
};

struct AcceleratorMidBoss : Boss {


	AcceleratorMidBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1001, 13); // TODO: proper animation
		hideHealthBar();
	}

	void updateSpellcard0() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "acc_mid", getEnemyShotCollisionList());
		}
	}

	virtual void update() override {
		if (mStage == -1) {
			if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, 20), 3)) {
				increaseStage(1000);
				reshowHealthBar();
			}
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {

				updateSpellcard0();

				if (mLife == 0) increaseStage();
			}
		}
		else {
			if (updateToPosition(getScreenPositionFromGamePosition(-0.5, -0.2, 20), 3)) {
				removeBoss();
			}
		}
	}

	virtual void hitCB() override {
		if (mStage < 0) return;
		hitCBStandard();
	}
};

struct AcceleratorBoss : Boss {



	AcceleratorBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1001, 13); // TODO: proper animation
		//hideHealthBar();
		setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		mStage = 1;
		mSpellCard = 0;
	}

	virtual void customCB() override {
		if (mStage == -1) {
			reshowHealthBar();
			mStage = 0;
		}
		mTimeInCard = -1;
	}


	void updateNonSpellCard1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "acc_ns1", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "acc_s1", getEnemyShotCollisionList());
		}


	}

	struct Star {
		int mEntityID;
		Position mTarget;
		double mSpeed;

		int mNow;
		int mHasReachedTarget = 0;
		int mFrequency;
		int mNext = 0;
		Star(int frequency) {
			Position p = getScreenPositionFromGamePosition(randfrom(0.1, 0.9), randfrom(0.1, 0.3), 30);
			mTarget = getScreenPositionFromGamePosition(randfrom(0.1, 0.9), randfrom(0.1, 0.3), 30);
			mSpeed = randfrom(1, 3);
			mNow = 0;
			mFrequency = frequency;

			mEntityID = addBlitzEntity(p);
			addBlitzMugenAnimationComponent(mEntityID, getEnemySprites(), getEnemyAnimations(), 13);
		}

		~Star() {
			removeBlitzEntity(mEntityID);
		}

		void moveToTarget() {
			if (mHasReachedTarget) return;

			auto* p = getBlitzEntityPositionReference(mEntityID);
			Position delta = mTarget - *p;
			delta.z = 0;

			if (vecLength(delta) < mSpeed) {
				*p = mTarget;
				mHasReachedTarget = 1;
				return;
			}

			delta = vecNormalize(delta);
			*p += delta * mSpeed;
		}

		void updateShoot() {
			if (!mHasReachedTarget) return;

			if (mNow >= mNext) {
				addShot(this, getBlitzEntityPosition(mEntityID), "enemy_slim_lower_half", getEnemyShotCollisionList());
				mNext = randfromInteger(1, mFrequency);
				mNow = 0;
			}
		}

		int update() {
			moveToTarget();
			updateShoot();
			mNow++;

			return mNow > 1000;
		}

	};

	map<int, unique_ptr<Star> > mStars;

	void updateStars() {
		stl_int_map_remove_predicate(mStars, &Star::update);
	}

	void clearStars() {
		mStars.clear();
	}

	void addStar(int frequency) {
		stl_int_map_push_back(mStars, std::move(make_unique<Star>(frequency)));
	}

	void updateNonSpellCard2() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "acc_ns2", getEnemyShotCollisionList());
		}

		if (mTimeInCard % 500 == 0) {
			addStar(15);
		}

		updateStars();
	}

	void updateSpellCard2() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "acc_s2", getEnemyShotCollisionList());
		}
	}
	void updateSpellCard3() {
		int time = 180;
		if (mTimeInCard % time == time-1) {
			clearStars();
		}
		if (mTimeInCard%time == 0) {
			for (int i = 0; i < 50; i++) addStar(100);
		}

		updateStars();
	}


	virtual void update() override {
		if (mStage == -1) {
			if (mTimeInCard == 0) startPreDialog();
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {

				updateNonSpellCard1();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				updateSpellCard1();
				if (!mLife) {
					increaseStage(1000);
				}
			}
		}
		else if (mStage == 1) {
			if (mSpellCard == 0) {

				updateNonSpellCard2();

				if (mLife <= mLifeMax / 3) {
					clearStars();
					increaseSpellcard(3);
				}
			}
			else {
				updateSpellCard2();
				if (!mLife) {
					increaseStage(1000);
				}
			}
		}
		else if (mStage == 2) {
			updateSpellCard3();

			if (!mLife) {
				increaseStage(1);
			}
		}
		else {
			if (updateToPosition(getScreenPositionFromGamePosition(-0.5, -0.2, 20), 3)) {
				clearStars();
				removeBoss();
			}
		}
	}

	virtual void hitCB() override {
		if (mStage < 0) return;
		hitCBStandard();
	}
};

struct WoaznBoss : Boss {


	WoaznBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1000, 13); // TODO: proper animation
		hideHealthBar();
		setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		mStage = 0;
		mSpellCard = 1;
	}

	void updateNonSpell1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "woazn_ns", getEnemyShotCollisionList());
		}
	}

	void updateSpell1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "woazn_s", getEnemyShotCollisionList());
		}
	}

	virtual void update() override {
		if (mStage == -1) {
			if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, 20), 3)) {
				increaseStage(1000);
				reshowHealthBar();
			}
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {

				updateNonSpell1();

				if (mLife < mLifeMax / 3) increaseSpellcard(6);
			}
			else {
				updateSpell1();
				if (!mLife) increaseStage();
			}
		}
		else {
			if (updateToPosition(getScreenPositionFromGamePosition(-0.5, -0.2, 20), 3)) {
				removeBoss();
			}
		}
	}

	virtual void hitCB() override {
		if (mStage < 0) return;
		hitCBStandard();
	}
};

struct AusMootBoss : Boss {



	AusMootBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1001, 13); // TODO: proper animation
		//hideHealthBar();
		setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		mStage = 3;
		mSpellCard = 0;
	}

	virtual void customCB() override {
		if (mStage == -1) {
			reshowHealthBar();
			mStage = 0;
		}
		mTimeInCard = -1;
	}


	void updateNonSpellCard1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "aus_ns1", getEnemyShotCollisionList());
		}
	}

	int angle = 0;
	int targetAngle = 180;
	void updateSpellCard1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "aus_s1", getEnemyShotCollisionList());
			setBlitzCameraHandlerEffectPositionOffset(getScreenPositionFromGamePosition(0.5, 0.5, 0));
		}

		if (mTimeInCard % 720 == 680 ) {
			targetAngle += 180;
		}

		if (angle != targetAngle) {
			angle+=5;
			setBlitzCameraHandlerRotationZ((angle / 360.0) * 2 * M_PI);
		}
	}

	void updateNonSpellCard2() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "aus_ns2", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard2() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "aus_s2", getEnemyShotCollisionList());
		}
	}

	void updateNonSpellCard3() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "aus_ns3", getEnemyShotCollisionList());
		}
	}

	int mS3Stage = 0;

	void increaseS3Stage() {
		mS3Stage = (mS3Stage + 1) % 4;
		mCurrentFrame = -1;
	}

	int p1Iteration = 0;
	double startX;
	double targetX;
	void updateS3P1() {
		if (mCurrentFrame == 0) {
			if (p1Iteration == 3) {
				increaseS3Stage();
				p1Iteration = 0;
			}
			else {
				startX = getBlitzEntityPositionX(mEntityID);
				targetX = randfrom(gGameVars.gameScreenOffset.x, gGameVars.gameScreenOffset.x + gGameVars.gameScreen.x);
				p1Iteration++;
			}
		}
		else if (mCurrentFrame <= 30) {
			if (mCurrentFrame <= 15) {
				addBlitzEntityPositionY(mEntityID, -1);
			}
			else {
				addBlitzEntityPositionY(mEntityID, 1);
			}

			double t = mCurrentFrame / double(30);
			setBlitzEntityPositionX(mEntityID, startX + t * (targetX - startX));
		}
		else if (mCurrentFrame == 31) {
			Position p = getBlitzEntityPosition(mEntityID);
			for (int i = 0; i < 30; i++) {
				addShot(nullptr, p, "aus_s3_1", getEnemyShotCollisionList());
			}
		}
		else if (mCurrentFrame == 32) {
			mCurrentFrame = -1;
		}
	}


	void updateS3P2() {
		if (mCurrentFrame == 0) {
			startX = getBlitzEntityPositionX(mEntityID);
			if (startX < gGameVars.gameScreenOffset.x + gGameVars.gameScreen.x / 2) {
				targetX = startX + gGameVars.gameScreen.x / 2;
			}
			else {
				targetX = startX - gGameVars.gameScreen.x / 2;
			}
		}
		else if (mCurrentFrame <= 50) {
			double t = mCurrentFrame / double(50);
			setBlitzEntityPositionX(mEntityID, startX + t * (targetX - startX));
			Position p = getBlitzEntityPosition(mEntityID);
			for (int i = 0; i < 1; i++) {
				addShot(nullptr, p, "aus_s3_1", getEnemyShotCollisionList());
			}
		}
		else {
			increaseS3Stage();
		}
	}
	void updateS3P3() {
		if (mCurrentFrame == 0) {
			if (p1Iteration == 3) {
				increaseS3Stage();
				p1Iteration = 0;
			}
			else {
				startX = getBlitzEntityPositionX(mEntityID);
				targetX = randfrom(gGameVars.gameScreenOffset.x, gGameVars.gameScreenOffset.x + gGameVars.gameScreen.x);
				p1Iteration++;
			}
		}
		else if (mCurrentFrame <= 30) {
			if (mCurrentFrame <= 15) {
				addBlitzEntityPositionY(mEntityID, -1);
			}
			else {
				addBlitzEntityPositionY(mEntityID, 1);
			}
		}
		else if (mCurrentFrame == 31) {
			Position p = getBlitzEntityPosition(mEntityID);
			addShot(nullptr, p, "aus_s3_3", getEnemyShotCollisionList());
		}
		else if (mCurrentFrame == 50) {
			mCurrentFrame = -1;
		}
	}
	void updateS3P4() {
		if (mCurrentFrame == 0) {
			Position p = getBlitzEntityPosition(mEntityID);
			addShot(nullptr, p, "aus_s3_4", getEnemyShotCollisionList());
		}
		else if (mCurrentFrame == 60) {
			increaseS3Stage();
		}
	}
	void updateSpellCard3() {
		if (mS3Stage == 0) {
			updateS3P1();


		} else if (mS3Stage == 1) {
			updateS3P2();
		}
		else if (mS3Stage == 2) {
			updateS3P3();
		}
		else {
			updateS3P4();
		}
	}

	void updateSpellCard4() {
		if (mTimeInCard%360 == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "aus_s4", getEnemyShotCollisionList());
		}
	}

	virtual void update() override {
		if (mStage == -1) {
			if (mTimeInCard == 0) startPreDialog();
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {

				updateNonSpellCard1();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				updateSpellCard1();
				if (!mLife) {
					increaseStage(1000);
				}
			}
		}
		else if (mStage == 1) {
			if (mSpellCard == 0) {

				updateNonSpellCard2();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				updateSpellCard2();
				if (!mLife) {
					increaseStage(1000);
				}
			}
		}
		else if (mStage == 2) {
			if (mSpellCard == 0) {

				updateNonSpellCard3();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				updateSpellCard3();
				if (!mLife) {
					increaseStage(1000);
				}
			}
		}
		else if (mStage == 3) {
			updateSpellCard4();

			if (!mLife) {
				increaseStage(1000);
			}
		}
		else {
			if (updateToPosition(getScreenPositionFromGamePosition(-0.5, -0.2, 20), 3)) {
				removeBoss();
			}
		}
	}

	virtual void hitCB() override {
		if (mStage < 0) return;
		hitCBStandard();
	}
};

struct ShiiBoss : Boss {



	ShiiBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1001, 13); // TODO: proper animation
		//hideHealthBar();
		setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		mStage = 3;
		mSpellCard = 0;
	}

	virtual void customCB() override {
		if (mStage == -1) {
			reshowHealthBar();
			mStage = 0;
		}
		mTimeInCard = -1;
	}


	void updateNonSpellCard1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_ns1", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard1() {
	}

	void updateNonSpellCard2() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_ns2", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard2() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_s2", getEnemyShotCollisionList());
		}
	}

	void updateNonSpellCard3() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_ns3", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard3() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_s3", getEnemyShotCollisionList());
		}
	}

	void updateNonSpellCard4() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_ns4", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard4() {
	}

	void updateSpellCard5() {
	}

	virtual void update() override {
		if (mStage == -1) {
			if (mTimeInCard == 0) startPreDialog();
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {

				updateNonSpellCard1();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				updateSpellCard1();
				if (!mLife) {
					increaseStage(1000);
				}
			}
		}
		else if (mStage == 1) {
			if (mSpellCard == 0) {

				updateNonSpellCard2();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				updateSpellCard2();
				if (!mLife) {
					increaseStage(1000);
				}
			}
		}
		else if (mStage == 2) {
			if (mSpellCard == 0) {

				updateNonSpellCard3();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				updateSpellCard3();
				if (!mLife) {
					increaseStage(1000);
				}
			}
		}
		else if (mStage == 3) {
			if (mSpellCard == 0) {

				updateNonSpellCard4();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				updateSpellCard4();
				if (!mLife) {
					increaseStage(1000);
				}
			}
		}
		else if (mStage == 4) {
			updateSpellCard5();


			if (!mLife) {
				increaseStage(1000);
			}

		}
		else {
			if (updateToPosition(getScreenPositionFromGamePosition(-0.5, -0.2, 20), 3)) {
				removeBoss();
			}
		}
	}

	virtual void hitCB() override {
		if (mStage < 0) return;
		hitCBStandard();
	}
};


void addBoss(std::string tName)
{
	if (gBossData.mIsActive) return;

	if (tName == "Barney") {
		gBossData.mBoss = make_unique<BarneyBoss>();
	} else if (tName == "Ack") {
		gBossData.mBoss = make_unique<AckBoss>();
	}
	else if (tName == "AcceleratorMid") {
		gBossData.mBoss = make_unique<AcceleratorMidBoss>();
	}
	else if (tName == "Accelerator") {
		gBossData.mBoss = make_unique<AcceleratorBoss>();
	}
	else if (tName == "Woazn") {
		gBossData.mBoss = make_unique<WoaznBoss>();
	}
	else if (tName == "AusMoot") {
		gBossData.mBoss = make_unique<AusMootBoss>();
	}
	else if (tName == "Shii") {
		gBossData.mBoss = make_unique<ShiiBoss>();
	}
	else {
		return;
	}

	gBossData.mIsActive = 1;
}

int hasActiveBoss()
{
	return gBossData.mIsActive;
}

Position getBossPosition()
{
	if (!gBossData.mIsActive) return makePosition(0, 0, 0);

	return gBossData.mBoss->getPosition();
}

void bossFinishedDialogCB()
{
	if (!gBossData.mIsActive) return;
	gBossData.mBoss->customCB();
}

void addBossDamage(int tDamage)
{
	if (!gBossData.mIsActive) return;

	gBossData.mBoss->addDamage(tDamage);
}

void introBoss(void(*tFunc)(void*), int tNow)
{
	gBossData.mBoss->introToPosition(tFunc, tNow);
}

