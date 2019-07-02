#include "menuscreen.h"

#include <prism/blitz.h>
#include "debug.h"
#include "enemyhandler.h"
#include "player.h"

class MenuScreen {

public:
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	BlitzTimelineAnimations mTimelineAnimations;

	int mBGEntity;
	int mLogoEntity;
	int mImageEntity;


	int mMenuItems[3];
	int mSelectedTimelineAnimation;
	static int mSelected;

	int mIsSelectingCharacter = 0;
	int mSelectors[2];
	int mCharacterID;
	int mOtherCharacterID;
	static int mSelectedCharacter;

	int mNameText;
	int mFocusedTitleText;
	int mFocusedText;
	int mUnfocusedTitleText;
	int mUnfocusedText;
	int mBombTitleText;
	int mBombText;
	MenuScreen()
	{
		mSprites = loadMugenSpriteFileWithoutPalette("title/TITLE.sff");
		mAnimations = loadMugenAnimationFile("title/TITLE.air");
		mTimelineAnimations = loadBlitzTimelineAnimations("title/TITLE.taf");

		mBGEntity = addBlitzEntity(makePosition(0, 0, 1));
		addBlitzMugenAnimationComponent(mBGEntity, &mSprites, &mAnimations, 1);

		mLogoEntity = addBlitzEntity(makePosition(160, -1, 3));
		addBlitzMugenAnimationComponent(mLogoEntity, &mSprites, &mAnimations, 2);
		addBlitzTimelineComponent(mLogoEntity, &mTimelineAnimations);

		mImageEntity = addBlitzEntity(makePosition(-1, 240, 2));
		addBlitzMugenAnimationComponent(mImageEntity, &mSprites, &mAnimations, 3);
		addBlitzTimelineComponent(mImageEntity, &mTimelineAnimations);

		for (int i = 0; i < 2; i++) {
			mSelectors[i] = addBlitzEntity(makePosition(350, 240, 3));
			addBlitzMugenAnimationComponent(mSelectors[i], &mSprites, &mAnimations, 100 + i);
			addBlitzTimelineComponent(mSelectors[i], &mTimelineAnimations);
			playBlitzTimelineAnimation(mSelectors[i], 500 + i);
		}

		mCharacterID = addBlitzEntity(makePosition(500, 186, 2));
		addBlitzMugenAnimationComponent(mCharacterID, &mSprites, &mAnimations, 200 + mSelectedCharacter);
		addBlitzTimelineComponent(mCharacterID, &mTimelineAnimations);

		mOtherCharacterID = addBlitzEntity(makePosition(500, -10, 2));
		addBlitzMugenAnimationComponent(mOtherCharacterID, &mSprites, &mAnimations, 200 + mSelectedCharacter);
		addBlitzTimelineComponent(mOtherCharacterID, &mTimelineAnimations);

		int base = 30;
		mNameText = addMugenTextMugenStyle("", makePosition(140, base + 40, 4), makeVector3DI(4, 0, 1));
		mFocusedTitleText = addMugenTextMugenStyle("", makePosition(140, base + 60, 4), makeVector3DI(4, 0, 1));
		mFocusedText = addMugenTextMugenStyle("", makePosition(160, base + 70, 4), makeVector3DI(4, 0, 1));
		mUnfocusedTitleText = addMugenTextMugenStyle("", makePosition(140, base + 90, 4), makeVector3DI(4, 0, 1));
		mUnfocusedText = addMugenTextMugenStyle("", makePosition(160, base + 100, 4), makeVector3DI(4, 0, 1));
		mBombTitleText = addMugenTextMugenStyle("", makePosition(140, base + 120, 4), makeVector3DI(4, 0, 1));
		mBombText = addMugenTextMugenStyle("", makePosition(160, base + 130, 4), makeVector3DI(4, 0, 1));

		addOption(mMenuItems[0], 10, makePosition(320, 120, 4));
		addOption(mMenuItems[1], 11, makePosition(320, 150, 4));
		addOption(mMenuItems[2], 12, makePosition(320, 180, 4));
		setSelectedOptionActive();

		fadeInMainMenu();
		setWrapperTitleScreen(getMenuScreen());
	}

	~MenuScreen() {

	}

	void addOption(int& tEntityID, int animation, Position tPos)
	{
		tEntityID = addBlitzEntity(tPos);
		addBlitzMugenAnimationComponent(tEntityID, &mSprites, &mAnimations, animation);
		addBlitzTimelineComponent(tEntityID, &mTimelineAnimations);

	}

	void setSelectedOptionActive()
	{
		int entity = mMenuItems[mSelected];
		mSelectedTimelineAnimation = playBlitzTimelineAnimation(entity, 50);
		setBlitzMugenAnimationColor(entity, 0, 1, 1);
	}

	void setSelectedOptionInactive()
	{
		int entity = mMenuItems[mSelected];
		stopBlitzTimelineAnimation(entity, mSelectedTimelineAnimation);
		setBlitzMugenAnimationColor(entity, 1, 1, 1);
		setBlitzEntityScale2D(entity, 1);
	}

	void fadeInMainMenu()
	{
		playBlitzTimelineAnimation(mLogoEntity, 2);
		playBlitzTimelineAnimation(mImageEntity, 3);

		playBlitzTimelineAnimation(mMenuItems[0], 100);
		playBlitzTimelineAnimation(mMenuItems[1], 101);
		playBlitzTimelineAnimation(mMenuItems[2], 102);
	}

	void fadeToCharacterSelect()
	{
		stopAllBlitzTimelineAnimations(mLogoEntity);
		setBlitzEntityPositionY(mLogoEntity, 71);
		playBlitzTimelineAnimation(mLogoEntity, 12);
		stopAllBlitzTimelineAnimations(mImageEntity);
		playBlitzTimelineAnimation(mImageEntity, 13);

		playBlitzTimelineAnimation(mMenuItems[0], 200);
		playBlitzTimelineAnimation(mMenuItems[1], 201);
		playBlitzTimelineAnimation(mMenuItems[2], 202);

		playBlitzTimelineAnimation(mCharacterID, 600);
		playBlitzTimelineAnimation(mOtherCharacterID, 600);
		playBlitzTimelineAnimation(mSelectors[0], 600);
		playBlitzTimelineAnimation(mSelectors[1], 600);

		setTextActive();

		mIsSelectingCharacter = 1;
	}

	void fadeFromCharacterSelect()
	{
		playBlitzTimelineAnimation(mLogoEntity, 22);
		playBlitzTimelineAnimation(mImageEntity, 23);

		playBlitzTimelineAnimation(mMenuItems[0], 300);
		playBlitzTimelineAnimation(mMenuItems[1], 301);
		playBlitzTimelineAnimation(mMenuItems[2], 302);

		playBlitzTimelineAnimation(mCharacterID, 610);
		playBlitzTimelineAnimation(mOtherCharacterID, 610);
		playBlitzTimelineAnimation(mSelectors[0], 610);
		playBlitzTimelineAnimation(mSelectors[1], 610);

		setTextInActive();

		mIsSelectingCharacter = 0;
	}

	vector<string> mNames{ "YOURNAMEHERE", "KINOMOD", "AEROLITE" };
	vector<string> mFocusedTexts{ "Straight shot", "Close spread", "LASER!" };
	vector<string> mUnfocusedTexts{ "Homing shot", "Large spread", "LASER!!" };
	vector<string> mBombTexts{ "Clean Sweep", "Lockdown", "High Impact Gorilla" };

	void setTextInActive()
	{
		changeMugenText(mNameText, " ");
		setMugenTextBuildup(mNameText, 1);

		changeMugenText(mFocusedTitleText, " ");
		changeMugenText(mFocusedText, " ");

		changeMugenText(mUnfocusedTitleText, " ");
		changeMugenText(mUnfocusedText, " ");

		changeMugenText(mBombTitleText, " ");
		changeMugenText(mBombText, " ");
	}

	void setTextActive()
	{
		if (mNames[mSelectedCharacter] == "YOURNAMEHERE") {
			changeMugenText(mNameText, (mNames[mSelectedCharacter] + " (/ck/)").data());
		} else if (mNames[mSelectedCharacter] == "KINOMOD") {
			changeMugenText(mNameText, (mNames[mSelectedCharacter] + " (/a/)").data());
		} else
		{
			changeMugenText(mNameText, mNames[mSelectedCharacter].data());
		}
		setMugenTextBuildup(mNameText, 1);

		changeMugenText(mFocusedTitleText, "Focused");
		changeMugenText(mFocusedText, mFocusedTexts[mSelectedCharacter].data());
		setMugenTextBuildup(mFocusedText, 1);

		changeMugenText(mUnfocusedTitleText, "Unfocused");
		changeMugenText(mUnfocusedText, mUnfocusedTexts[mSelectedCharacter].data());
		setMugenTextBuildup(mUnfocusedText, 1);

		changeMugenText(mBombTitleText, "Rangeban");
		changeMugenText(mBombText, mBombTexts[mSelectedCharacter].data());
		setMugenTextBuildup(mBombText, 1);
	}

	void updateSelection(int delta)
	{
		setSelectedOptionInactive();
		mSelected = (mSelected + 3 + delta) % 3;
		setSelectedOptionActive();
	}

	void updateCharacterUp()
	{
		const int previous = mSelectedCharacter;
		mSelectedCharacter = (mSelectedCharacter + 3 - 1) % 3;
		changeBlitzMugenAnimation(mOtherCharacterID, 200 + previous);
		setBlitzEntityPositionY(mOtherCharacterID, 186);
		changeBlitzMugenAnimation(mCharacterID, 200 + mSelectedCharacter);
		setBlitzEntityPositionY(mCharacterID, 500);
		playBlitzTimelineAnimation(mOtherCharacterID, 702);
		playBlitzTimelineAnimation(mCharacterID, 701);
		setTextActive();
	}

	void updateCharacterDown()
	{
		const int previous = mSelectedCharacter;
		mSelectedCharacter = (mSelectedCharacter + 3 + 1) % 3;
		changeBlitzMugenAnimation(mOtherCharacterID, 200 + previous);
		setBlitzEntityPositionY(mOtherCharacterID, 186);
		changeBlitzMugenAnimation(mCharacterID, 200 + mSelectedCharacter);
		setBlitzEntityPositionY(mCharacterID, 500);
		playBlitzTimelineAnimation(mOtherCharacterID, 700);
		playBlitzTimelineAnimation(mCharacterID, 703);
		setTextActive();
	}

	void update() {
		if (!mIsSelectingCharacter) {
			if (hasPressedUpFlank())
			{
				updateSelection(-1);
			}

			if (hasPressedDownFlank())
			{
				updateSelection(1);
			}

			if (hasPressedAFlank())
			{
				if (mSelected == 2)
				{
					abortScreenHandling();
				}
				else
				{
					fadeToCharacterSelect();
				}

			}
		}
		else
		{
			if (hasPressedUpFlank())
			{
				updateCharacterUp();
			}

			if (hasPressedDownFlank())
			{
				updateCharacterDown();
			}

			if (hasPressedAFlank())
			{
				setPlayerName(mNames[mSelectedCharacter]);
				if (mSelected == 0)
				{
					startGame();
				}
				else
				{
					startExtra();
				}
			}

			if(hasPressedBFlank())
			{
				fadeFromCharacterSelect();
			}

		}
	}

};

int MenuScreen::mSelected = 0;
int MenuScreen::mSelectedCharacter = 0;

EXPORT_SCREEN_CLASS(MenuScreen)