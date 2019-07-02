#include "inmenu.h"

#include <prism/blitz.h>

#include "player.h"
#include "menuscreen.h"
#include "enemyhandler.h"

#define IN_MENU_BASE_Z 80


struct InMenu
{
	TextureData mWhiteTexture;
	int mBGAnimation;

	int mIsPausedText;
	int mContinueText;
	int mToTitleText;
	int mRetryText;

	int mContinueQuestionText;
	int mContinueYes;
	int mContinueNo;

	int mContinuesLeftText;

	int mIsContinueActive = 1;

	int mIsActive = 0;
	int mIsContinue = 0;
	int mSelected = 0;
	int mSelectedContinue = 0;
	InMenu()
	{
		mWhiteTexture = createWhiteTexture();
		mBGAnimation = playOneFrameAnimationLoop(makePosition(0, 0, IN_MENU_BASE_Z), &mWhiteTexture);
		setAnimationSize(mBGAnimation, makePosition(320, 240, 1), makePosition(0, 0, 0));
		setAnimationTransparency(mBGAnimation, 0);
		setAnimationColor(mBGAnimation, 0, 0, 0);

		mIsPausedText = addMugenTextMugenStyle("PAUSED", makePosition(30, 40, IN_MENU_BASE_Z + 1), makeVector3DI(4, 0, 1));
		setMugenTextVisibility(mIsPausedText, 0);
		setMugenTextScale(mIsPausedText, 2);

		Position basePosition = makePosition(50, 100, IN_MENU_BASE_Z + 1);
		mContinueText = addMugenTextMugenStyle("CONTINUE", basePosition, makeVector3DI(4, 0, 1));
		setMugenTextVisibility(mContinueText, 0);

		mToTitleText = addMugenTextMugenStyle("TO TITLE", basePosition + makePosition(10, 10, 0), makeVector3DI(4, 0, 1));
		setMugenTextVisibility(mToTitleText, 0);

		mRetryText = addMugenTextMugenStyle("RETRY", basePosition + makePosition(20, 20, 0), makeVector3DI(4, 0, 1));
		setMugenTextVisibility(mRetryText, 0);

		mContinueQuestionText = addMugenTextMugenStyle("CONTINUE?", makePosition(30, 40, IN_MENU_BASE_Z + 1), makeVector3DI(4, 0, 1));
		setMugenTextVisibility(mContinueQuestionText, 0);
		setMugenTextScale(mContinueQuestionText, 2);

		basePosition = makePosition(50, 100, IN_MENU_BASE_Z + 1);
		mContinueYes = addMugenTextMugenStyle("YES", basePosition, makeVector3DI(4, 0, 1));
		setMugenTextVisibility(mContinueYes, 0);

		mContinueNo = addMugenTextMugenStyle("NO", basePosition + makePosition(10, 10, 0), makeVector3DI(4, 0, 1));
		setMugenTextVisibility(mContinueNo, 0);

		mContinuesLeftText = addMugenTextMugenStyle("CONTINUES LEFT: ", makePosition(30, 200, IN_MENU_BASE_Z + 1), makeVector3DI(4, 0, 1));
		setMugenTextVisibility(mContinuesLeftText, 0);
		setMugenTextScale(mContinuesLeftText, 0.5);
	}

	int& getSelectedTextID()
	{
		if(mSelected == 0)
		{
			return mContinueText;
		} else if (mSelected == 1)
		{
			return mToTitleText;
		} else
		{
			return mRetryText;
		}
	}


	int& getSelectedContinueTextID()
	{
		if (mSelectedContinue == 0)
		{
			return mContinueYes;
		}
		else
		{
			return mContinueNo;
		}
	}

	void setSelectedTextActive()
	{
		auto& id = getSelectedTextID();
		setMugenTextColor(id, COLOR_BLUE);
	}

	void setSelectedTextInActive()
	{
		auto& id = getSelectedTextID();
		setMugenTextColor(id, COLOR_WHITE);
	}

	void increaseSelectedText(int tDelta)
	{
		setSelectedTextInActive();
		mSelected = (mSelected + 3 + tDelta) % 3;
		if(mSelected == 0 && !mIsContinueActive) mSelected = (mSelected + 3 + tDelta) % 3;
		setSelectedTextActive();
	}

	void setSelectedContinueTextActive()
	{
		auto& id = getSelectedContinueTextID();
		setMugenTextColor(id, COLOR_BLUE);
	}

	void setSelectedContinueTextInActive()
	{
		auto& id = getSelectedContinueTextID();
		setMugenTextColor(id, COLOR_WHITE);
	}

	void increaseSelectedContinueText(int tDelta)
	{
		setSelectedContinueTextInActive();
		mSelectedContinue = (mSelectedContinue + 2 + tDelta) % 2;
		setSelectedContinueTextActive();
	}

	void setInactive()
	{
		setAnimationTransparency(mBGAnimation, 0);

		setMugenTextVisibility(mIsPausedText, 0);
		setMugenTextVisibility(mContinueText, 0);
		setMugenTextVisibility(mToTitleText, 0);
		setMugenTextVisibility(mRetryText, 0);
		setMugenTextVisibility(mContinueQuestionText, 0);
		setMugenTextVisibility(mContinueYes, 0);
		setMugenTextVisibility(mContinueNo, 0);
		setMugenTextVisibility(mContinuesLeftText, 0);

		setSelectedTextInActive();

		resumeWrapper();
		mIsActive = 0;
	}

	void updateContinueText()
	{
		int continues = getContinueAmount();
		stringstream ss;
		ss << "CONTINUES LEFT: " << continues;
		changeMugenText(mContinuesLeftText, ss.str().data());
	}

	void setActive(int tIsFinal)
	{
		setAnimationTransparency(mBGAnimation, 0.7);

		setMugenTextVisibility(mIsPausedText, 1);
		if (!tIsFinal) setMugenTextVisibility(mContinueText, 1);
		setMugenTextVisibility(mToTitleText, 1);
		setMugenTextVisibility(mRetryText, 1);
		setMugenTextVisibility(mContinuesLeftText, 1);
		updateContinueText();

		if (mSelected == 0 && tIsFinal) mSelected = (mSelected + 3 + 1) % 3;
		setSelectedTextActive();

		pauseWrapper();
		mIsContinue = 0;
		mIsContinueActive = !tIsFinal;
		mIsActive = 1;
	}

	void setContinueActive()
	{
		setAnimationTransparency(mBGAnimation, 0.7);

		setMugenTextVisibility(mContinueQuestionText, 1);
		setMugenTextVisibility(mContinueYes, 1);
		setMugenTextVisibility(mContinueNo, 1);
		setMugenTextVisibility(mContinuesLeftText, 1);
		updateContinueText();

		setSelectedContinueTextActive();

		pauseWrapper();
		mIsContinue = 1;
		mIsActive = 1;
	}

	void gotoTitle()
	{
		setInactive();
		setNewScreen(getMenuScreen());
	}

	void switchToFinalSelectionText()
	{
		setInactive();
		setFinalPauseMenuActive();
	}

	void handleContinueInput()
	{
		if (mSelectedContinue == 0)
		{
			usePlayerContinue();
			setInactive();
		}
		else
		{
			switchToFinalSelectionText();
		}
	}

	void updateContinue()
	{
		if (hasPressedUpFlank())
		{
			increaseSelectedContinueText(-1);
		}

		if (hasPressedDownFlank())
		{
			increaseSelectedContinueText(1);
		}

		if (hasPressedAFlank())
		{
			handleContinueInput();
		}

		if (hasPressedAbortFlank())
		{
			switchToFinalSelectionText();
		}
	}

	void handlePauseInput()
	{
		if(mSelected == 0)
		{
			setInactive();
		} else if (mSelected == 1)
		{
			gotoTitle();
		} else
		{
			if (isInExtra()) {
				startExtra();
			} else
			{
				startGame();
			}
		}
	}

	void updatePause()
	{
		if(hasPressedUpFlank())
		{
			increaseSelectedText(-1);
		}

		if (hasPressedDownFlank())
		{
			increaseSelectedText(1);
		}

		if(hasPressedAFlank())
		{
			handlePauseInput();
		}

		if (hasPressedAbortFlank())
		{
			if (mIsContinueActive) {
				setInactive();
			}
			else
			{
				gotoTitle();
			}
		}
	}

	void update()
	{
		if (!mIsActive) return;
		if(mIsContinue)
		{
			updateContinue();
		} else
		{
			updatePause();
		}
	}
};

EXPORT_ACTOR_CLASS(InMenu);

void setPauseMenuActive()
{
	gInMenu->setActive(0);
}

void setFinalPauseMenuActive()
{
	gInMenu->setActive(1);
}

void showContinueScreen()
{
	gInMenu->setContinueActive();
}

int isInMenuActive()
{
	return gInMenu->mIsActive;
}
