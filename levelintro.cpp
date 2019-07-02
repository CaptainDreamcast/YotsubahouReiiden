#include "levelintro.h"

#include <prism/blitz.h>
#include "debug.h"
#include "level.h"

#define LEVEL_INTRO_Z 85

struct LevelIntro {
	int mIsActive = 0;

	int mNow = 0;
	int mNameID;
	int mTextID;

	void remove() {
		removeMugenText(mNameID);
		removeMugenText(mTextID);
		mIsActive = 0;
	}

	void update() {
		if (!mIsActive) return;

		addMugenTextPosition(mNameID, makePosition(0.1, 0, 0));
		addMugenTextPosition(mTextID, makePosition(-0.1, 0, 0));

		if (mNow == 360) {
			remove();
		}

		mNow++;
	}

	void start(const std::string & tLevelName, const std::string & tLevelText) {
		mNameID = addMugenTextMugenStyle(tLevelName.data(), getScreenPositionFromGamePosition(0.1, 0.1, LEVEL_INTRO_Z), makeVector3DI(4, 0, 1));
		setMugenTextBuildup(mNameID, 1);
		setMugenTextScale(mNameID, 0.8);

		mTextID = addMugenTextMugenStyle(tLevelText.data(), getScreenPositionFromGamePosition(0.2, 0.15, LEVEL_INTRO_Z), makeVector3DI(4, 0, 1));
		setMugenTextBuildup(mTextID, 1);
		setMugenTextTextBoxWidth(mTextID, 140);
		setMugenTextScale(mTextID, 0.8);

		mNow = 0;
		mIsActive = 1;
	}
};

EXPORT_ACTOR_CLASS(LevelIntro);

void addLevelIntro(const std::string & tLevelName, const std::string & tLevelText)
{
	gLevelIntro->start(tLevelName, tLevelText);
}

int isLevelIntroActive()
{
	return gLevelIntro->mIsActive;
}
