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

#define PLAYER_Z 8

struct PlayerHandler {

		static string mName;
		static PlayerHandler* mSelf;

		MugenSpriteFile mSprites;
		MugenAnimations mAnimations;

		int mEntityID;
		int mHitboxIndicator;
		int mIsFocused;
		int mShotFrequencyNow;

		int mPower;
		uint64_t mScore;

		int mLife;
		int mInvincibleNow;
		int mInvincibleDuration;
		int mIsInvincible;

		int mBombAmount;
		int mIsBombActive;
		int mBombNow;
		Vector3DI mBombDamageDuration;
		int mBombDamage;
		int mBombDuration;

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

		mPower = 100;
		mScore = 0;
		mLife = 2;
		mBombAmount = 4;
		mIsBombActive = 0;
		mIsInvincible = 0;
	}

	void addPlayerShotInternal() {
		stringstream ss;
		if (mIsFocused) {
			ss << "yournamehere_straight_";
		}
		else {
			ss << "yournamehere_homing_";
		}

		ss << mPower / 100;
		addShot(NULL, getBlitzEntityPosition(mEntityID), ss.str(), getPlayerShotCollisionList());
	}

	static void playerHitCB(void* tCaller, void* tCollisionData) {
		(void)tCaller;
		if (mSelf->mIsInvincible) return;

		int id = addMugenAnimation(getMugenAnimation(getUIAnimations(), 16), getUISprites(), getBlitzEntityPosition(mSelf->mEntityID) + makePosition(0, 0, 1));
		setMugenAnimationNoLoop(id);

		mSelf->mLife--;
		mSelf->mPower = std::max(mSelf->mPower - 100, 100);
		mSelf->mInvincibleNow = 0;
		mSelf->mInvincibleDuration = 180;
		mSelf->mIsInvincible = 1;

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

	void updatePlayerBombSetActive() {
		if (mIsBombActive) return;

		if (mBombAmount && hasPressedBFlank()) {
			int id = addMugenAnimation(getMugenAnimation(&mAnimations, 1), &mSprites, getBlitzEntityPosition(mEntityID)); // TODO: proper animation
			setMugenAnimationNoLoop(id);
			mBombDamage = 100;
			mBombDamageDuration = makeVector3DI(60, 80, 0);

			mBombNow = 0;
			mBombDuration = 120;
			mIsBombActive = 1;
			mBombAmount--;

			mInvincibleDuration = mBombDuration;
			mIsInvincible = 1;
		}

	}

	void updatePlayerBomb() {
		if (!mIsBombActive) return;

		if (mBombNow >= mBombDamageDuration.x && mBombNow <= mBombDamageDuration.y) {
			addDamageToAllEnemies(mBombDamage);
			addBossDamage(mBombDamage);
		}
		removeEnemyBulletsExceptComplex();

		if (mBombNow >= mBombDuration) {
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

	void update() {
		if (isDialogActive()) return;
		updateFocus();
		updatePlayerMovement();
		updatePlayerShot();
		updatePlayerBombSetActive();
		updatePlayerBomb();
		updatePlayerInvincible();
		updatePlayerAutocollect();
		updateHitboxIndicator();

		if (hasPressedX()) {
			addPlayerPower(1);
		}
	}
};
PlayerHandler* PlayerHandler::mSelf = nullptr;
string PlayerHandler::mName;

EXPORT_ACTOR_CLASS(PlayerHandler);

void setPlayerName(const std::string & tName)
{
	PlayerHandler::mName = tName;
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
