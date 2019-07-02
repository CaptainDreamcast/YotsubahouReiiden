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
#include "player.h"

#define BOSS_Z 20

static void bossHitCB(void* tCaller, void* tCollisionData);
static void customBossCBCaller(void* tCaller);
static void removeBoss();
static void anonHitCB(void* tCaller, void* tCollisionData);

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
	int mTimeLeftInCard = 60;
	int mIsTimerActive = 0;
	int mSurvivalTimerTextID;
	int mSurvivalTimerID;
	int mIsInvincible = 0;

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
		mSurvivalTimerTextID = addMugenTextMugenStyle("Time left: ", makePosition(193, 40, BOSS_Z + 2), makeVector3DI(4, 0, -1));
		setMugenTextScale(mSurvivalTimerTextID, 0.5);
		mSurvivalTimerID = addMugenTextMugenStyle("23", makePosition(193, 40, BOSS_Z + 2), makeVector3DI(2, 0, 1));
		setMugenTextScale(mSurvivalTimerID, 0.5);
		disableSurvivalTimer();
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
		removeMugenText(mSurvivalTimerTextID);
		removeMugenText(mSurvivalTimerID);
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

	void updateSurvivalTimer()
	{
		if (!mIsTimerActive) return;

		if (mTimeLeftInCard < 0)
		{
			mIsInvincible = 0;
			addBossDamage(50);
		}

		mTimeLeftInCard--;
		int seconds = mTimeLeftInCard / 60;
		seconds = std::max(0, seconds);
		stringstream ss;
		if (seconds < 10) ss << 0;
		ss << seconds;
		changeMugenText(mSurvivalTimerID, ss.str().data());
	}

	void enableSurvivalTimer(int tSeconds)
	{
		mTimeLeftInCard = tSeconds * 60 + 60;
		setMugenTextVisibility(mSurvivalTimerTextID, 1);
		setMugenTextVisibility(mSurvivalTimerID, 1);
		updateSurvivalTimer();

		mIsTimerActive = 1;
	}

	void disableSurvivalTimer()
	{
		setMugenTextVisibility(mSurvivalTimerTextID, 0);
		setMugenTextVisibility(mSurvivalTimerID, 0);
		updateSurvivalTimer();

		mIsTimerActive = 0;
	}

	void increaseStage(int tLife, int tPowerItems = 0, int tScoreItems = 0) {
		
		increaseStageAfter(tPowerItems, tScoreItems);
		setLife(tLife);
		updateStageDisplay();
	}	

	void increaseStageAfter(int tPowerItems = 0, int tScoreItems = 0) {
		addPowerItems(getBlitzEntityPosition(mEntityID), tPowerItems);
		addScoreItems(getBlitzEntityPosition(mEntityID), tScoreItems);
		mStage++;
		mSpellCard = 0;
		mTimeInCard = -1;
		mIsInvincible = 0;
		int stagesLeft = mStageAmount - mStage - 1;
		if (mStage >= 0 && stagesLeft >= 0) {
			enableSurvivalTimer(60);
		} else
		{
			disableSurvivalTimer();
		}
		removeEnemyBullets();
	}

	void increaseSpellcard(double tHealthFactor = 1, int tTimeInCard = 60) {
		mSpellCard++;
		mTimeInCard = -1;
		mIsInvincible = 0;
		mLife = int(mLife*tHealthFactor);
		mLifeMax = int(mLifeMax*tHealthFactor);
		enableSurvivalTimer(tTimeInCard);
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
	virtual void hitCB(int tDamage) = 0;
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

	void startSurvival(int tSeconds) {
		mIsInvincible = 1;
		mTimeLeftInCard = tSeconds * 60 + 59;
	}

	void addDamage(int tDamage) {
		if (mIsInvincible) return;
		mLife = std::max(0, mLife - tDamage);
	}

	void hitCBStandard(int tDamage) {
		addDamage(tDamage);
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
	gBossData.mBoss->updateSurvivalTimer();
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
	if (tCollisionData == nullptr) return;
	gBossData.mBoss->hitCB(getShotDamage(tCollisionData));
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

	virtual void hitCB(int tDamage) override {
		hitCBStandard(tDamage);
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
			if (updateToPosition(getScreenPositionFromGamePosition(0.3, 0.2, BOSS_Z), 3)) {
				mSpellcard1Stage = 1;
				mSpellTime = 0;
			}
		} else if (mSpellcard1Stage == 1) {
			if (mSpellTime == 200) {
				mSpellTime = 0;
				mSpellcard1Stage = 2;
			}
		} else if (mSpellcard1Stage == 2) {
			if (updateToPosition(getScreenPositionFromGamePosition(0.7, 0.2, BOSS_Z), 3)) {
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
			if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3)) {
				increaseStage(1200);
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

				if (mLife == 0) increaseStageAfter(38, 17);
			}
		}
		else {
			removeBoss();
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};

struct AckBoss : Boss {


	AckBoss() {
		createAtPosition(getScreenPositionFromGamePosition(makePosition(-0.1, -0.1, BOSS_Z)), 1002, 13);
		hideHealthBar();
		setBossName("ACK");
		mStageAmount = 2;

		//setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		//mStage = -1;
	}

	~AckBoss() {
		
	}

	virtual void customCB() override {
		if (mStage == -1) {
			reshowHealthBar();
			increaseStage(1200);
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
					increaseStage(1600, 25, 10);
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
					increaseStageAfter();
				}
			}
		}
		else {
			removeBoss();
			startPostDialog();
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};

struct AcceleratorMidBoss : Boss {


	AcceleratorMidBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1000, 13); // TODO: proper animation
		hideHealthBar();
		setBossName("Accelerator");
		mStageAmount = 1;
	}

	~AcceleratorMidBoss()
	{
		addLifeItem(getBlitzEntityPosition(mEntityID));
	}

	void updateSpellcard0() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "acc_mid", getEnemyShotCollisionList());
		}
	}

	virtual void update() override {
		if (mStage == -1) {
			if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3)) {
				increaseStage(1200);
				reshowHealthBar();
			}
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {

				updateSpellcard0();

				if (mLife == 0) increaseStageAfter(40, 20);
			}
		}
		else {
			if (updateToPosition(getScreenPositionFromGamePosition(-0.5, -0.2, BOSS_Z), 3)) {
				removeBoss();
			}
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};

struct AcceleratorBoss : Boss {



	AcceleratorBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1000, 13); // TODO: proper animation
		hideHealthBar();
		setBossName("Accelerator");
		//setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		//mStage = 1;
		//mSpellCard = 0;
		mStageAmount = 3;
	}

	virtual void customCB() override {
		if (mStage == -1) {
			reshowHealthBar();
			increaseStage(1200);
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
					increaseStage(1400, 20, 10);
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
					increaseStage(1400, 30, 10);
				}
			}
		}
		else if (mStage == 2) {
			updateSpellCard3();

			if (!mLife) {
				increaseStageAfter();
			}
		}
		else {
			clearStars();
			removeBoss();
			startPostDialog();
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};

struct WoaznBoss : Boss {


	WoaznBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1001, 13); // TODO: proper animation
		hideHealthBar();
		setBossName("Woazn");
		//setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		//mStage = 0;
		//mSpellCard = 1;
		mStageAmount = 1;
	}

	~WoaznBoss()
	{
		addBombItem(getBlitzEntityPosition(mEntityID));
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
			if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3)) {
				increaseStage(2000);
				reshowHealthBar();
			}
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {

				updateNonSpell1();

				if (mLife < mLifeMax / 3) increaseSpellcard(2);
			}
			else {
				updateSpell1();
				if (!mLife) increaseStageAfter(50, 20);
			}
		}
		else {
			removeBoss();
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};

struct AusMootBoss : Boss {



	AusMootBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1002, 13); // TODO: proper animation
		hideHealthBar();
		setBossName("Australian moot");
		//setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		//mStage = 3;
		//mSpellCard = 0;

		mStageAmount = 4;
	}

	virtual void customCB() override {
		if (mStage == -1) {
			reshowHealthBar();
			increaseStage(1400);
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
					setBlitzCameraHandlerRotationZ(0);
					increaseStage(1400, 30, 10);
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
					increaseStage(1400, 40, 10);
				}
			}
		}
		else if (mStage == 2) {
			if (mSpellCard == 0) {

				updateNonSpellCard3();

				if (mLife <= mLifeMax / 3) {
					mCurrentFrame = -1;
					increaseSpellcard(3);
				}
			}
			else {
				if (mTimeInCard == 0) {
					startSurvival(15);
				}
				if (!mLife) {
					if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3)) {
						increaseStage(1600, 50, 10);
					}
				}
				else
				{	
					updateSpellCard3();
				}
			}
		}
		else if (mStage == 3) {
			updateSpellCard4();

			if (!mLife) {
				increaseStageAfter();
			}
		}
		else {
			startPostDialog();
			removeBoss();

		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};

struct ShiiBoss : Boss {



	ShiiBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1002, 13); // TODO: proper animation
		hideHealthBar();
		setBossName("Shii");
		//setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		//mStage = 4;
		//mSpellCard = 0;

		mStageAmount = 5;
	}

	virtual void customCB() override {
		if (mStage == -1) {
			reshowHealthBar();
			increaseStage(2000);
		}
		mTimeInCard = -1;
	}


	void updateNonSpellCard1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_ns1", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_s1", getEnemyShotCollisionList());
		}
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
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_s4", getEnemyShotCollisionList());
		}
	}

	int mWorldEntity;
	Position mTarget;
	void loadWorld() {
		mWorldEntity = addBlitzEntity(getScreenPositionFromGamePosition(0.5, 0.1, 0));
		addBlitzMugenAnimationComponent(mWorldEntity, getLevelSprites(), getLevelAnimations(), 1002);
		addBlitzCollisionComponent(mWorldEntity);
		addBlitzCollisionCirc(mWorldEntity, getEnemyShotCollisionList(), makeCollisionCirc(makePosition(0, 0, 0), 30));
	}

	void unloadWorld() {
		removeBlitzEntity(mWorldEntity);
	}

	void findWorldTarget() {
		auto p = getBlitzEntityPosition(mWorldEntity);
		auto playerPos = getBlitzEntityPosition(getPlayerEntity());
		auto delta = playerPos - p;
		double t[4] = { -1, -1, -1, -1 };
		if(delta.x) t[0] = 0 - p.x / delta.x;
		if(delta.x) t[1] = 320 - p.x / delta.x;
		if(delta.y) t[2] = 0 - p.x / delta.x;
		if(delta.y) t[3] = 0 - p.x / delta.x;
	}

	int hasWorldReachedTarget() {
		return 0;
	}

	void updateWorldMovement() {
	
	}

	void updateOppositeBulletSpam() {
	
	}

	void updateSpellCard5() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_s5", getEnemyShotCollisionList());
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
					increaseStage(2000, 40, 10);
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
					increaseStage(2000, 40, 10);
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
					increaseStage(2000, 50, 10);
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
					increaseStage(2000, 60, 10);
				}
			}
		}
		else if (mStage == 4) {
			updateSpellCard5();


			if (!mLife) {
				increaseStageAfter();
			}

		}
		else {
			removeBoss();
			startPostDialog();
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};

struct ShiiMidBoss : Boss {


	ShiiMidBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1001, 13); // TODO: proper animation
		hideHealthBar();
		setBossName("Shii");
		//setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		//mStage = 0;
		//mSpellCard = 1;

		mStageAmount = 1;
	}

	~ShiiMidBoss()
	{
		addLifeItem(getBlitzEntityPosition(mEntityID));
	}

	void updateNonSpell1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_mid_ns1", getEnemyShotCollisionList());
		}
	}

	void updateSpell1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "shii_mid_s1", getEnemyShotCollisionList());
		}
	}

	virtual void update() override {
		if (mStage == -1) {
			if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3)) {
				increaseStage(2000);
				reshowHealthBar();
			}
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {

				updateNonSpell1();

				if (mLife < mLifeMax / 3) increaseSpellcard(2);
			}
			else {
				updateSpell1();
				if (!mLife) increaseStageAfter();
			}
		}
		else {
			removeBoss();
		
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};

struct HiroBoss : Boss {

	HiroBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1002, 13); // TODO: proper animation
		hideHealthBar();
		setBossName("Hiro");
		//setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		//mStage = 5;
		//mSpellCard = 0;

		mStageAmount = 6;
	}

	virtual void customCB() override {
		if (mStage == -1) {
			reshowHealthBar();
			increaseStage(2000);
		}
		mTimeInCard = -1;
	}


	void updateNonSpellCard1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_ns1", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard1() {
		if(mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_s1", getEnemyShotCollisionList());
		}
	}

	void updateNonSpellCard2() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_ns2", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard2() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_s2", getEnemyShotCollisionList());
		}
	}

	void updateNonSpellCard3() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_ns3", getEnemyShotCollisionList());
		}
	}

	int spellcard3Stage = 1;
	Position target = makePosition(0, 0, 0);
	void updateSpellCard3() {
		if (spellcard3Stage == 0) {
			if (updateToPosition(getScreenPositionFromGamePosition(target.x, 0.2, BOSS_Z), 3)) {
				spellcard3Stage = 1;
			}
		}
		else if (spellcard3Stage == 1) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_s3", getEnemyShotCollisionList());
			target.x = randfrom(0.0, 1.0);
			spellcard3Stage = 2;
			mTimeInCard = -1;
		}
		else {
			if (mTimeInCard == 1) {
				spellcard3Stage = 0;
			}
		}
	}

	void updateNonSpellCard4() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_ns4", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard4() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_s4", getEnemyShotCollisionList());
		}
	}

	void updateNonSpellCard5() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_ns5", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard5() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_s5", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard6() {
		if (mTimeInCard%120 == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_s6_1", getEnemyShotCollisionList());
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_s6_2", getEnemyShotCollisionList());
		}

		if (mTimeInCard % 140 == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "hiro_s6_3", getEnemyShotCollisionList());
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
					increaseStage(2000, 50, 10);
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
					increaseStage(2000, 50, 10);
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
				
				if (!mLife) {
					if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3)) {
						increaseStage(2000, 60, 10);
					}
				} else
				{
					updateSpellCard3();
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
					increaseStage(2000, 60, 10);
				}
			}
		}
		else if (mStage == 4) {
			if (mSpellCard == 0) {

				updateNonSpellCard5();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				updateSpellCard5();
				if (!mLife) {
					increaseStage(2000, 60, 10);
				}
			}
		}
		else if (mStage == 5) {
			updateSpellCard6();


			if (!mLife) {
				hideHealthBar();
				increaseStageAfter();
			}

		}
		else {
				removeBoss();
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};

struct CrisisBoss : Boss {

	CrisisBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1002, 13); // TODO: proper animation
		hideHealthBar();
		setBossName("Crisis");
	
		mStageAmount = 6;
	}

	virtual void customCB() override {
		if (mStage == -1) {
			increaseStage(2000);
		}
		mTimeInCard = -1;
	}

	int mErrorEntity;
	void updateSpellCard7() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "crisis", getEnemyShotCollisionList());
			mErrorEntity = addMugenAnimation(getMugenAnimation(getLevelAnimations(), 2), getLevelSprites(), makePosition(0, 0, 2));
			setMugenAnimationTransparency(mErrorEntity, 0);
		}

		setMugenAnimationTransparency(mErrorEntity, std::min(1.0, mTimeInCard / double(60 * 60)));
	}

	virtual void update() override {
		if (mStage == -1) {
			increaseStage(1000);
			startSurvival(60);
		}
		else if (mStage == 0) {
			
			updateSpellCard7();


			if (!mLife) {
				increaseStageAfter();
			}

		}
		else {
				removeBoss();
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};


struct AlternativeBoss : Boss {


	AlternativeBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1001, 13); // TODO: proper animation
		hideHealthBar();
		setBossName("ALTERNATIVE");
		//setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		//mStage = 1;
		//mSpellCard = 0;

		mStageAmount = 2;
	}

	~AlternativeBoss()
	{
		addLifeItem(getBlitzEntityPosition(mEntityID));
	}

	void updateNonSpell1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "alternative_ns1", getEnemyShotCollisionList());
		}
	}

	int mStageS1 = 0;
	void updateSpell1() {
		if (mStageS1 == 0) {
			if (updateToPosition(getScreenPositionFromGamePosition(0, 0.2, BOSS_Z), 3)) {
				mStageS1 = 1;
				mTimeInCard = -1;
			}
		} else if (mStageS1 == 1) {
			if (mTimeInCard == 0) {
				addShot(this, getBlitzEntityPosition(mEntityID), "alternative_s11", getEnemyShotCollisionList());
			}
			if (mTimeInCard == 30) {
				mStageS1 = 2;
				mTimeInCard = -1;
			}
		} else if (mStageS1 == 2) {
			if (updateToPosition(getScreenPositionFromGamePosition(1, 0.2, BOSS_Z), 3)) {
				mStageS1 = 3;
				mTimeInCard = -1;
			}
		}
		else if (mStageS1 == 3) {
			if (mTimeInCard == 0) {
				addShot(this, getBlitzEntityPosition(mEntityID), "alternative_s12", getEnemyShotCollisionList());
			}
			if (mTimeInCard == 30) {
				mStageS1 = 0;
				mTimeInCard = -1;
			}
		}
	}

	void updateSpell2() { 
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "alternative_s2", getEnemyShotCollisionList());
		}
	}

	virtual void update() override {
		if (mStage == -1) {
			if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3)) {
				increaseStage(2000);
				reshowHealthBar();
			}
		}
		else if (mStage == 0) {
			if (mSpellCard == 0) {

				updateNonSpell1();

				if (mLife < mLifeMax / 3) increaseSpellcard(2);
			}
			else {
				
				if (!mLife) {
					if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3)) {
						increaseStage(1400, 50, 20);
					}
				} else {
				updateSpell1();
				}
			}
		}
		else if (mStage == 1) {
			updateSpell2();
			if (!mLife) increaseStageAfter(50, 20);
		}
		else {
				removeBoss();
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		hitCBStandard(tDamage);
	}
};

struct MootBoss : Boss {

	MootBoss() {
		createAtPosition(getScreenPositionFromGamePosition(-0.1, 0.1, BOSS_Z), 1002, 13); // TODO: proper animation
		hideHealthBar();
		setBossName("Chris");
		//setBlitzEntityPosition(mEntityID, makePosition(96 + 16, 62 + 8, BOSS_Z));
		//mStage = 8;
		//mSpellCard = 0;
		mStageAmount = 9;
	}

	virtual void customCB() override {
		if (mStage == -1) {
			reshowHealthBar();
			increaseStage(2000);
		}
		mTimeInCard = -1;
	}


	void updateNonSpellCard1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_ns1", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard1() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_s1", getEnemyShotCollisionList());
		}
	}

	void updateNonSpellCard2() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_ns2", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard2() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_s2", getEnemyShotCollisionList());
		}
	}

	void updateNonSpellCard3() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_ns3", getEnemyShotCollisionList());
		}
	}

	int spellcard3Stage = 0;
	vector<Position> targets =
	{ 
		getScreenPositionFromGamePosition(0, 0.5, BOSS_Z), 
		getScreenPositionFromGamePosition(1, 0.5, BOSS_Z),
		getScreenPositionFromGamePosition(0.5, 0, BOSS_Z),
		getScreenPositionFromGamePosition(0.5, 1, BOSS_Z),

	};
	int mCurrentTarget = 0;
	int mSnacksEntity;
	
	void createSnacks() {
		mSnacksEntity = addBlitzEntity(makePosition(96 + 16, 62 + 38, BOSS_Z));
		addBlitzMugenAnimationComponent(mSnacksEntity, getLevelSprites(), getLevelAnimations(), 1003);
		addBlitzCollisionComponent(mSnacksEntity);
		int collisionID = addBlitzCollisionCirc(mSnacksEntity, getEnemyCollisionList(), makeCollisionCirc(makePosition(0, 0, 0), 12));
		addBlitzCollisionCB(mSnacksEntity, collisionID, bossHitCB, NULL);
		addShot(this, getBlitzEntityPosition(mEntityID), "snacks_s3", getEnemyShotCollisionList());
	}

	void updateSpellCard3() {
		if (spellcard3Stage == 0) {
			createSnacks();
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_s3", getEnemyShotCollisionList());
			spellcard3Stage = 1;
		}
		else if (spellcard3Stage == 1) {
			if (updateToPositionEntity(mSnacksEntity, targets[mCurrentTarget], 5)) {
				spellcard3Stage = 2;
				mCurrentFrame = -1;
			}
			addBlitzEntityRotationZ(mSnacksEntity, 0.1);
		}
		else if (spellcard3Stage == 2) {
			if (mCurrentFrame == 60) {
				mCurrentTarget = (mCurrentTarget + 1) % targets.size();
				spellcard3Stage = 1;
			}
		}
	}

	struct Anon {
		int mEntityID;
		int mWasShotAt = 0;
		int mNow = 0;
		int mDuration;
		int mIsDead = 0;
		Anon() {
			Position p = getScreenPositionFromGamePosition(randfrom(0.1, 0.9), 1.0, 30);

			mEntityID = addBlitzEntity(p);
			addBlitzMugenAnimationComponent(mEntityID, getEnemySprites(), getEnemyAnimations(), 13);
			addBlitzCollisionComponent(mEntityID);
			int id = addBlitzCollisionCirc(mEntityID, getPlayerCollisionList(), makeCollisionCirc(makePosition(0, 0, 0), 3));
			addBlitzCollisionCB(mEntityID, id, anonHitCB, this);
			addBlitzPhysicsComponent(mEntityID);
			addBlitzPhysicsVelocityY(mEntityID, -0.6);

			mDuration = randfromInteger(60, 150);
		}

		~Anon() {
			removeBlitzEntity(mEntityID);
		}

		int update() {
			if (mNow > 0 && mNow % mDuration == 0) {
				addShot(this, getBlitzEntityPosition(mEntityID), "moot_s5", getEnemyShotCollisionList());
			}
			mNow++;

			const auto y = getBlitzEntityPositionY(mEntityID);

			return y < 0 || mIsDead;
		}

	};

	map<int, unique_ptr<Anon> > mAnons;

	void updateAnons() {
		stl_int_map_remove_predicate(mAnons, &Anon::update);
	}

	void clearAnons() {
		mAnons.clear();
	}

	void addAnon(int frequency) {
		stl_int_map_push_back(mAnons, std::move(make_unique<Anon>()));
	}

	int timeSinceShot = 0;
	void updateSpellCard4() {
		timeSinceShot++;
		if (timeSinceShot > 180) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_s4", getEnemyShotCollisionList());
		}
	}

	void updateSpellCard5() {
		if (mTimeInCard %30 == 0) {
			addAnon(20);
		}

		updateAnons();
	}

	int mEntity2;
	int mEntity3;
	int mEntity4;
	void createNewCharacter(int& entityID, Position tPos, int tAnimation) {
		entityID = addBlitzEntity(tPos);
		addBlitzMugenAnimationComponent(entityID, getLevelSprites(), getLevelAnimations(), tAnimation);
		addBlitzCollisionComponent(entityID);
		int collisionID = addBlitzCollisionCirc(entityID, getEnemyCollisionList(), makeCollisionCirc(makePosition(0, 0, 0), 13));
		addBlitzCollisionCB(entityID, collisionID, bossHitCB, NULL);
	}

	void createCharacters() {
		createNewCharacter(mEntity2, getScreenPositionFromGamePosition(-0.1, -0.1, BOSS_Z), 1001); // ALTERNATIVE
		if (getPlayerEnumValue() == PlayerFuncs::PLAYER_YOURNAMEHERE) {
			createNewCharacter(mEntity3, getScreenPositionFromGamePosition(-0.1, -0.1, BOSS_Z), 1005); // KINOMOD
			createNewCharacter(mEntity4, getScreenPositionFromGamePosition(-0.1, -0.1, BOSS_Z), 1006); // AEROLITE
		} else if (getPlayerEnumValue() == PlayerFuncs::PLAYER_KINOMOD) {
			createNewCharacter(mEntity3, getScreenPositionFromGamePosition(-0.1, -0.1, BOSS_Z), 1004); // YOURNAMEHERE
			createNewCharacter(mEntity4, getScreenPositionFromGamePosition(-0.1, -0.1, BOSS_Z), 1006); // AEROLITE
		}
		else {
			createNewCharacter(mEntity3, getScreenPositionFromGamePosition(-0.1, -0.1, BOSS_Z), 1004); // YOURNAMEHERE
			createNewCharacter(mEntity4, getScreenPositionFromGamePosition(-0.1, -0.1, BOSS_Z), 1005); // KINOMOD
		}
	}

	void removeCharacter(int tEntity)
	{
		int id = addMugenAnimation(getMugenAnimation(getUIAnimations(), 16), getUISprites(), getBlitzEntityPosition(tEntity) + makePosition(0, 0, 1));
		setMugenAnimationNoLoop(id);
		removeBlitzEntity(tEntity);
	}

	void removeCharacters()
	{
		removeCharacter(mEntity2);
		removeCharacter(mEntity3);
		removeCharacter(mEntity4);
	}

	void addCharacterShot(int entity) {
		int anim = getBlitzMugenAnimationAnimationNumber(entity);
		string name = "";
		if (anim == 1001) {
			name = "alternative_s6";
		} else if (anim == 1005) {
			name = "kinomod_s6";
		}
		else if(anim == 1006) {
			name = "aerolite_s6";
		}
		else {
			name = "yournamehere_s6";
		}

		addShot(this, getBlitzEntityPosition(entity), name, getEnemyShotCollisionList());
	}

	void updateNonSpellCard6() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_ns6", getEnemyShotCollisionList());
		}
	}

	int mSpell6Stage = 0;
	void updateSpellCard6() {
		if (mSpell6Stage == 0) {
			createCharacters();
			mSpell6Stage = 1;
		} else if (mSpell6Stage == 1) {
			int areAllThere = 1;
			areAllThere &= updateToPosition(getScreenPositionFromGamePosition(0.5, 0.1, BOSS_Z), 3);
			areAllThere &= updateToPositionEntity(mEntity2, getScreenPositionFromGamePosition(0.25, 0.2, BOSS_Z+1), 3);
			areAllThere &= updateToPositionEntity(mEntity3, getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z+1), 3);
			areAllThere &= updateToPositionEntity(mEntity4, getScreenPositionFromGamePosition(0.75, 0.2, BOSS_Z+1), 3);
			if (areAllThere) {
				mSpell6Stage = 2;
			}
		}
		else if (mSpell6Stage == 2) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_s6", getEnemyShotCollisionList());
			addCharacterShot(mEntity2);
			addCharacterShot(mEntity3);
			addCharacterShot(mEntity4);
			mSpell6Stage = 3;
		}
	}

	void updateNonSpellCard7() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_ns7", getEnemyShotCollisionList());
		}
	}

	int mSpellCard7Stage = 0;
	void updateSpellCard7() {
		if (mSpellCard7Stage == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_s7", getEnemyShotCollisionList());
			mSpellCard7Stage++;
		}
		else if (mSpellCard7Stage == 1)
		{
			if (updateToPosition(getScreenPositionFromGamePosition(0, 0.2, BOSS_Z), 3)) {
				mSpellCard7Stage++;
				mTimeInCard = -1;
			}
		}
		else if (mSpellCard7Stage == 2)
		{
			if (mTimeInCard == 180) {
				mSpellCard7Stage++;
			}
		}
		else if (mSpellCard7Stage == 3)
		{
			if (updateToPosition(getScreenPositionFromGamePosition(1.0, 0.2, BOSS_Z), 3)) {
				mSpellCard7Stage++;
				mTimeInCard = -1;
			}
		}
		else if (mSpellCard7Stage == 4)
		{
			if (mTimeInCard == 180) {
				mSpellCard7Stage = 1;
			}
		}
	}

	void updateSpellCard8() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_s8", getEnemyShotCollisionList());
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
					increaseStage(2000, 40, 10);
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
					increaseStage(2000, 50, 10);
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
					increaseStage(2000, 50, 10);
				}
			}
		}
		else if (mStage == 3) {
			if (mSpellCard == 0)
			{
				setBlitzEntityRotationZ(mSnacksEntity, 0);
				int done = updateToPosition(getScreenPositionFromGamePosition(0.5, -0.2, BOSS_Z), 3);
				done &= updateToPositionEntity(mSnacksEntity, getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3);
				if (done) mSpellCard++;
			}
			else {
				updateSpellCard4();
				if (!mLife) {
					increaseStage(2000, 50, 10);
				}
			}

		}
		else if (mStage == 4) {
			if (mSpellCard == 0)
			{
				if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3))
				{
					mSpellCard++;
				}
			}
			else if (mSpellCard == 1) {
				if (updateToPositionEntity(mSnacksEntity, getScreenPositionFromGamePosition(0.5, 1.2, BOSS_Z), 5))
				{
					mSpellCard++;
				}
				addBlitzEntityRotationZ(mSnacksEntity, 0.3);
			}
			else {
				updateSpellCard5();

				if (!mLife) {
					clearAnons();
					increaseStage(2000, 50, 10);
				}
			}
		}
		else if (mStage == 5) {
			if (mSpellCard == 0)
			{
				updateNonSpellCard6();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else if (mSpellCard == 1) {
				updateSpellCard6();

				if (!mLife) {
					removeCharacters();
					mSpellCard++;
					increaseStageAfter();
				}
			}
			else
			{
				if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.2, BOSS_Z), 3))
				{
					increaseStage(2000, 50, 10);
				}

			}
		}
		else if (mStage == 6) {
			if (mSpellCard == 0)
			{
				updateNonSpellCard7();

				if (mLife <= mLifeMax / 3) {
					increaseSpellcard(3);
				}
			}
			else {
				if (!mLife) {
					if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.5, BOSS_Z), 3))
					{
						increaseStage(2000, 50, 10);
						startSurvival(60);
					}

				}
				else
				{
					updateSpellCard7();
				}
			}

		}
		else if (mStage == 7) {
			if (!mLife) {
				if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.5, BOSS_Z), 3))
				{
					increaseStage(3000, 50, 10);
				}

			}
			else
			{
				updateSpellCard8();
			}
		}
		else if (mStage == 8) {
			updateSpellCard9();

			if (!mLife) {
				if (updateToPosition(getScreenPositionFromGamePosition(0.5, 0.5, BOSS_Z), 3))
				{
					increaseStageAfter();
				}

			}
		}
		else {
		startPostDialog();
			removeBoss();
		}
	}

	void updateSpellCard9() {
		if (mTimeInCard == 0) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_s9", getEnemyShotCollisionList());
		}
	}

	virtual void hitCB(int tDamage) override {
		if (mStage < 0) return;
		if (mStage == 3) {
			addShot(this, getBlitzEntityPosition(mEntityID), "moot_s4", getEnemyShotCollisionList());
			timeSinceShot = 0;
		}
		hitCBStandard(tDamage);
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
	else if (tName == "ShiiMid") {
		gBossData.mBoss = make_unique<ShiiMidBoss>();
	}
	else if (tName == "Hiro") {
		gBossData.mBoss = make_unique<HiroBoss>();
	}
	else if (tName == "Crisis") {
		gBossData.mBoss = make_unique<CrisisBoss>();
	}
	else if (tName == "Alternative") {
		gBossData.mBoss = make_unique<AlternativeBoss>();
	}
	else if (tName == "Moot") {
		gBossData.mBoss = make_unique<MootBoss>();
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

Position getSnacksPosition()
{
	if (!gBossData.mIsActive) return makePosition(0, 0, 0);
	MootBoss* mootBoss = (MootBoss*)gBossData.mBoss.get();
	return getBlitzEntityPosition(mootBoss->mSnacksEntity);
}

static void anonHitCB(void* tCaller, void* tCollisionData) {
	MootBoss::Anon* anon = (MootBoss::Anon*)tCaller;
	anon->mIsDead = 1;
}

Position getAnonPosition(void * tCaller)
{
	MootBoss::Anon* anon = (MootBoss::Anon*)tCaller;
	return getBlitzEntityPosition(anon->mEntityID);
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

