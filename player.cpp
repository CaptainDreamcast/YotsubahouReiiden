#include "player.h"

#include <prism/blitz.h>

#include "shothandler.h"
#include "collision.h"
#include "boss.h"
#include "enemyhandler.h"
#include "debug.h"
#include "itemhandler.h"
#include "dialoghandler.h"
#include "uihandler.h"
#include "inmenu.h"

#define PLAYER_Z 8
#define BOMB_Z 7

struct PlayerHandler {

	static string mName;
	static PlayerFuncs::PlayerEnum mEnum;
	static PlayerHandler* mSelf;

	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	int mEntityID;
	int mHitboxIndicator;
	int mIsFocused;
	int mShotFrequencyNow;

	static int mPower;
	static uint64_t mScore;

	static int mItemAmount;
	int mNextItemAmount = 0;
	std::vector<int> mItemStages{ 50, 125, 200, 300, 450, 800, 1000 };

	static int mLife;
	int mInvincibleNow;
	int mInvincibleDuration;
	int mIsInvincible;

	static int mBombAmount;
	int mIsBombActive;

	int mBombNow;
	int mBombDuration;

	static int mContinues;
	int mDeathBombNow = -1;

	PlayerHandler() {
		mSelf = this;
		mSprites = loadMugenSpriteFileWithoutPalette("player/" + mName + ".sff");
		mAnimations = loadMugenAnimationFile("player/" + mName + ".air");

		Position startPos = gGameVars.gameScreenOffset + (gGameVars.gameScreen / 2.0);
		mEntityID = addBlitzEntity(makePosition(startPos.x, 220, PLAYER_Z));
		addBlitzMugenAnimationComponent(mEntityID, &mSprites, &mAnimations, 1);
		setBlitzMugenAnimationBaseDrawScale(mEntityID, 0.5);
		addBlitzPhysicsComponent(mEntityID);
		addBlitzCollisionComponent(mEntityID);
		int collisionID = addBlitzCollisionCirc(mEntityID, getPlayerCollisionList(), makeCollisionCirc(makePosition(0, 0, 0), 0.5));
		addBlitzCollisionCB(mEntityID, collisionID, playerHitCB, NULL);
		collisionID = addBlitzCollisionCirc(mEntityID, getPlayerCollectionList(), makeCollisionCirc(makePosition(0, 0, 0), 5));

		mHitboxIndicator = addMugenAnimation(getMugenAnimation(&mAnimations, 1000), &mSprites, getBlitzEntityPosition(mEntityID));
		setMugenAnimationVisibility(mHitboxIndicator, 0);

		mShotFrequencyNow = 0;

		mIsBombActive = 0;
		mIsInvincible = 0;

		recalculateNextScore();
	}

	void addPlayerShotInternal() {
		stringstream ss;
		if (mEnum == PlayerFuncs::PLAYER_YOURNAMEHERE) {
			if (mIsFocused) {
				ss << "yournamehere_straight_";
			}
			else {
				ss << "yournamehere_homing_";
			}
		}
		else if (mEnum == PlayerFuncs::PLAYER_KINOMOD) {
			if (mIsFocused) {
				ss << "kinomod_focused_";
			}
			else {
				ss << "kinomod_unfocused_";
			}
		}
		else {
			if (mIsFocused) {
				ss << "aerolite_unfocused_";
			}
			else {
				ss << "aerolite_unfocused_";
			}
		}

		ss << mPower / 100;
		addShot(NULL, getBlitzEntityPosition(mEntityID), ss.str(), getPlayerShotCollisionList());
	}

	void addDeath()
	{
		int id = addMugenAnimation(getMugenAnimation(getUIAnimations(), 16), getUISprites(), getBlitzEntityPosition(mSelf->mEntityID) + makePosition(0, 0, 1));
		setMugenAnimationNoLoop(id);

		if (mLife) {
			mSelf->mLife--;
		}
		else
		{
			if (mContinues) {
				showContinueScreen();
			}
			else
			{
				setFinalPauseMenuActive();
			}
		}
		addDeathPowerItems(getBlitzEntityPosition(mSelf->mEntityID));
		mSelf->mBombAmount = std::max(mSelf->mBombAmount, 4);
		mSelf->mPower = std::max(mSelf->mPower - 100, 100);
		mSelf->mInvincibleNow = 0;
		mSelf->mInvincibleDuration = 180;
		mSelf->mIsInvincible = 1;
	}

	void updateDeath()
	{
		if (mDeathBombNow < 0) return;
		mDeathBombNow--;
		if(!mDeathBombNow)
		{
			addDeath();
			mDeathBombNow = -1;
		}
	}

	static void playerHitCB(void* tCaller, void* tCollisionData) {
		(void)tCaller;
		if (mSelf->mIsInvincible) return;

		if (mSelf->mDeathBombNow < 0) {
			mSelf->mDeathBombNow = 5;
		}
	}

	void updatePlayerShot() {
		int shotFrequency = mIsFocused ? 5 : 5;

		mShotFrequencyNow = std::max(0, mShotFrequencyNow - 1);
		if (hasPressedA() && !mShotFrequencyNow) {
			addPlayerShotInternal();
			mShotFrequencyNow = shotFrequency;
		}
	}

	void updatePlayerMovement() {
		double speed = mIsFocused ? 1 : 2;
		if (hasPressedLeft()) {
			addBlitzEntityPositionX(mEntityID, -speed);
		}
		else if (hasPressedRight()) {
			addBlitzEntityPositionX(mEntityID, speed);
		}
		if (hasPressedUp()) {
			addBlitzEntityPositionY(mEntityID, -speed);
		}
		else if (hasPressedDown()) {
			addBlitzEntityPositionY(mEntityID, speed);
		}

		auto p = getBlitzEntityPositionReference(mEntityID);
		*p = clampPositionToGeoRectangle(*p, makeGeoRectangle(gGameVars.gameScreenOffset.x, gGameVars.gameScreenOffset.y, gGameVars.gameScreen.x, gGameVars.gameScreen.y));
	}

	void updateFocus() {
		mIsFocused = hasPressedR();
	}

	void updateHitboxIndicator() {
		setMugenAnimationVisibility(mHitboxIndicator, mIsFocused);
		setMugenAnimationDrawAngle(mHitboxIndicator, getMugenAnimationDrawAngle(mHitboxIndicator) + 0.01);
		Position p = getBlitzEntityPosition(mEntityID);
		p.z = 11;
		setMugenAnimationPosition(mHitboxIndicator, p);
	}

	void updatePlayerInvincible() {
		if (!mIsInvincible) {
			setBlitzMugenAnimationTransparency(mEntityID, 1);
			return;
		}

		setBlitzMugenAnimationTransparency(mEntityID, randfrom(0.3, 0.7));

		mInvincibleNow++;
		if (mInvincibleNow >= mInvincibleDuration) {
			mIsInvincible = 0;
		}
	}

	void addBombDamage(int tDamage)
	{
		addDamageToAllEnemies(tDamage);
		addBossDamage(tDamage);
	}

	int mBroomEntityID[3];
	void setYournamehereBombActive()
	{
		mBroomEntityID[0] = addBlitzEntity(getScreenPositionFromGamePosition(0.25, 1.4, BOMB_Z));
		mBroomEntityID[1] = addBlitzEntity(getScreenPositionFromGamePosition(0.5, 1.3, BOMB_Z));
		mBroomEntityID[2] = addBlitzEntity(getScreenPositionFromGamePosition(0.75, 1.4, BOMB_Z));

		for (int i = 0; i < 3; i++)
		{
			addBlitzMugenAnimationComponent(mBroomEntityID[i], &mSprites, &mAnimations, 10);
			addBlitzPhysicsComponent(mBroomEntityID[i]);
			addBlitzPhysicsVelocityY(mBroomEntityID[i], -1);
			addBlitzEntityRotationZ(mBroomEntityID[i], M_PI);
		}

		mBombDuration = INF;
	}
	void updateYournamehereBomb()
	{

		for (int i = 0; i < 3; i++)
		{
			addBlitzPhysicsVelocityY(mBroomEntityID[i], -0.01);
			addBlitzEntityRotationZ(mBroomEntityID[i], 0.1);
		}

		if (mBombNow > 120)
		{
			addBombDamage(2);
		}

		if (getBlitzEntityPositionY(mBroomEntityID[0]) < getScreenPositionFromGamePositionY(-0.4))
		{
			mBombDuration = 0;
			mInvincibleDuration = 0;
		}
	}

	void removeYournamehereBomb()
	{

		for (int i = 0; i < 3; i++)
		{
			removeBlitzEntity(mBroomEntityID[i]);
		}
	}

	int mLockEntity;
	int mLockedEntity;
	void setKinomodBombActive()
	{
		mLockEntity = addBlitzEntity(getScreenPositionFromGamePosition(0.5, 0.4, BOMB_Z));
		addBlitzMugenAnimationComponent(mLockEntity, &mSprites, &mAnimations, 10);

		mBombDuration = 200;
	}

	void updateKinomodBomb()
	{
		if (mBombNow == 24)
		{
			mLockedEntity = addBlitzEntity(getScreenPositionFromGamePosition(0.5, 0.4, BOMB_Z));
			addBlitzMugenAnimationComponent(mLockedEntity, &mSprites, &mAnimations, 11);

			addBombDamage(600);
		}
	}

	void removeKinomodBomb()
	{
		removeBlitzEntity(mLockEntity);
		removeBlitzEntity(mLockedEntity);
	}

	int mGorillaEntity;
	int mLaserEntity[2];
	void setAeroliteBombActive()
	{
		mGorillaEntity = addBlitzEntity(getScreenPositionFromGamePosition(0.5, 1.0, BOMB_Z) + makePosition(0, 146, 0));
		addBlitzMugenAnimationComponent(mGorillaEntity, &mSprites, &mAnimations, 10);
		addBlitzPhysicsComponent(mGorillaEntity);
		addBlitzPhysicsVelocityY(mGorillaEntity, -1);

		mBombDuration = 300;
	}

	void addAeroliteLaser(int& entityID, Position tPos)
	{
		entityID = addBlitzEntity(tPos);
		addBlitzMugenAnimationComponent(entityID, &mSprites, &mAnimations, 11);
		
	}

	int mIsLaserGoingRight[2];
	double mLaserSpeed[2];
	void updateAeroliteBomb()
	{
		if(mBombNow == 120)
		{
			addAeroliteLaser(mLaserEntity[0], getBlitzEntityPosition(mGorillaEntity) + makePosition(2, -65, 1));
			addAeroliteLaser(mLaserEntity[1], getBlitzEntityPosition(mGorillaEntity) + makePosition(35, -66, 1));
			setBlitzPhysicsVelocityY(mGorillaEntity, 0);
			mIsLaserGoingRight[0] = 0;
			mIsLaserGoingRight[1] = 1;
			mLaserSpeed[0] = 0.01;
			mLaserSpeed[1] = 0.02;
		}

		if(mBombNow >= 120 && mBombNow < 240)
		{
			addBombDamage(2);

			for(int i = 0; i < 2; i++)
			{
				double angle = getBlitzEntityRotationZ(mLaserEntity[i]);
				if(mIsLaserGoingRight[i])
				{
					angle += mLaserSpeed[i];
					if (angle > M_PI) mIsLaserGoingRight[i] ^= 1;
				} else
				{
					angle -= mLaserSpeed[i];
					if (angle < -M_PI) mIsLaserGoingRight[i] ^= 1;
				}
				addBlitzEntityRotationZ(mLaserEntity[i], angle);
			}
		}

		if(mBombNow == 240)
		{
			setBlitzPhysicsVelocityY(mGorillaEntity, 2);
			for (int i = 0; i < 2; i++) removeBlitzEntity(mLaserEntity[i]);
		}
	}

	void removeAeroliteBomb()
	{
		removeBlitzEntity(mGorillaEntity);
	}

	void setPlayerBombActive()
	{
		switch (mEnum) {
		case PlayerFuncs::PLAYER_YOURNAMEHERE:
			setYournamehereBombActive();
			break;
		case PlayerFuncs::PLAYER_KINOMOD:
			setKinomodBombActive();
			break;
		case PlayerFuncs::PLAYER_AEROLITE:
			setAeroliteBombActive();
			break;
		default:;
		}
	}

	void updateSelectedPlayerBomb()
	{
		switch (mEnum) {
		case PlayerFuncs::PLAYER_YOURNAMEHERE:
			updateYournamehereBomb();
			break;
		case PlayerFuncs::PLAYER_KINOMOD:
			updateKinomodBomb();
			break;
		case PlayerFuncs::PLAYER_AEROLITE:
			updateAeroliteBomb();
			break;
		default:;
		}
	}

	void removePlayerBomb()
	{
		switch (mEnum) {
		case PlayerFuncs::PLAYER_YOURNAMEHERE:
			removeYournamehereBomb();
			break;
		case PlayerFuncs::PLAYER_KINOMOD:
			removeKinomodBomb();
			break;
		case PlayerFuncs::PLAYER_AEROLITE:
			removeAeroliteBomb();
			break;
		default:;
		}
	}

	void updatePlayerBombSetActive() {
		if (mIsBombActive) return;

		if (mBombAmount && hasPressedBFlank()) {
			setPlayerBombActive();

			mDeathBombNow = -1;
			mBombNow = 0;
			mIsBombActive = 1;
			mBombAmount--;

			mInvincibleNow = 0;
			mInvincibleDuration = mBombDuration;
			mIsInvincible = 1;
		}

	}

	void updatePlayerBomb() {
		if (!mIsBombActive) return;

		removeEnemyBulletsExceptComplex();
		updateSelectedPlayerBomb();


		if (mBombNow >= mBombDuration) {
			removePlayerBomb();
			mIsBombActive = 0;
		}
		mBombNow++;
	}

	void updatePlayerAutocollect() {
		Position collectPosition = getScreenPositionFromGamePosition(0, 0.2, 0);
		Position p = getBlitzEntityPosition(mEntityID);

		if (p.y < collectPosition.y) {
			setItemsAutocollect();
		}
	}

	void recalculateNextScore()
	{
		int next = 1000;
		size_t i = 0;
		while (true)
		{
			if (i < mItemStages.size()) next = mItemStages[i];
			else next += 200;
			if (next > mItemAmount) break;
			i++;
		}
		mNextItemAmount = next;
	}

	void update() {
		updateFocus();
		updatePlayerMovement();
		if (!isDialogActive()) updatePlayerShot();
		if (!isDialogActive()) updatePlayerBombSetActive();
		updatePlayerBomb();
		updatePlayerInvincible();
		updatePlayerAutocollect();
		updateHitboxIndicator();
		updateDeath();
	}
};
PlayerHandler* PlayerHandler::mSelf = nullptr;
string PlayerHandler::mName;
PlayerFuncs::PlayerEnum PlayerHandler::mEnum = PlayerFuncs::PLAYER_YOURNAMEHERE;
int PlayerHandler::mItemAmount = 0;
int PlayerHandler::mPower = 100;
int PlayerHandler::mLife = 0;
int PlayerHandler::mBombAmount = 4;
uint64_t PlayerHandler::mScore = 0;
int PlayerHandler::mContinues = 4;

EXPORT_ACTOR_CLASS(PlayerHandler);

const std::string& getPlayerName()
{
	return gPlayerHandler->mName;
}

void setPlayerName(const std::string & tName)
{
	if (tName == "YOURNAMEHERE") {
		PlayerHandler::mEnum = PlayerFuncs::PLAYER_YOURNAMEHERE;
	}
	else if (tName == "KINOMOD") {
		PlayerHandler::mEnum = PlayerFuncs::PLAYER_KINOMOD;
	}
	else {
		PlayerHandler::mEnum = PlayerFuncs::PLAYER_AEROLITE;
	}

	PlayerHandler::mName = tName;
}

PlayerFuncs::PlayerEnum getPlayerEnumValue()
{
	return PlayerHandler::mEnum;
}

MugenSpriteFile* getPlayerSprites()
{
	return &gPlayerHandler->mSprites;
}

MugenAnimations * getPlayerAnimations()
{
	return &gPlayerHandler->mAnimations;
}

int getPlayerPower()
{
	return gPlayerHandler->mPower;
}

void addPlayerPower(int tPower)
{
	gPlayerHandler->mPower += tPower;
	gPlayerHandler->mPower = std::min(400, gPlayerHandler->mPower);
}

uint64_t getPlayerScore()
{
	return gPlayerHandler->mScore;
}

void addPlayerScoreItem(int tScoreMultiplier)
{
	gPlayerHandler->mScore += 12345 * tScoreMultiplier;
	gPlayerHandler->mItemAmount++;
	if(gPlayerHandler->mItemAmount >= gPlayerHandler->mNextItemAmount)
	{
		addPlayerLife();
		gPlayerHandler->recalculateNextScore();
	}
}

void addPlayerScore(int tScore)
{
	gPlayerHandler->mScore += tScore;
}

int getPlayerLife()
{
	return gPlayerHandler->mLife;
}

void addPlayerLife()
{
	gPlayerHandler->mLife++;
}

int getPlayerBombs()
{
	return gPlayerHandler->mBombAmount;
}

void addPlayerBomb()
{
	gPlayerHandler->mBombAmount++;
}

int getPlayerEntity()
{
	return gPlayerHandler->mEntityID;
}

int isPlayerBombActive()
{
	return gPlayerHandler->mIsBombActive;
}

int getContinueAmount()
{
	return gPlayerHandler->mContinues;
}

void resetPlayer()
{
	if (!isInExtra()) {
		gPlayerHandler->mItemAmount = 0;
		gPlayerHandler->mPower = 100;
		gPlayerHandler->mLife = 2;
		gPlayerHandler->mBombAmount = 4;
		gPlayerHandler->mScore = 0;
		gPlayerHandler->mContinues = 4;
	} else
	{
		gPlayerHandler->mItemAmount = 0;
		gPlayerHandler->mPower = 400;
		gPlayerHandler->mLife = 2;
		gPlayerHandler->mBombAmount = 4;
		gPlayerHandler->mScore = 0;
		gPlayerHandler->mContinues = 0;
	}
}

void usePlayerContinue()
{
	gPlayerHandler->mContinues--;
	gPlayerHandler->mItemAmount = 0;
	gPlayerHandler->mPower = 400;
	gPlayerHandler->mLife = 2;
	gPlayerHandler->mBombAmount = 4;
	gPlayerHandler->mScore = 0;
}

int getCollectedItemAmount()
{
	return gPlayerHandler->mItemAmount;
}

int getNextItemLifeUpAmount()
{
	return gPlayerHandler->mNextItemAmount;
}
