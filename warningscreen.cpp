#include "warningscreen.h"

#include <prism/blitz.h>
#include "menuscreen.h"
static 	void gotoTitleScreen(void*)
{
	setNewScreen(getMenuScreen());
}

struct WarningScreen
{
	MugenSpriteFile mSprites;
	BlitzTimelineAnimations mTimelineAnimations;
	int mEntityID1;
	int mEntityID2;
	int mEntityID3;

	WarningScreen()
	{
		mSprites = loadMugenSpriteFileWithoutPalette("title/WARNING.sff");
		mTimelineAnimations = loadBlitzTimelineAnimations("title/WARNING.taf");

		mEntityID1 = addBlitzEntity(makePosition(0, 0, 1));
		addBlitzMugenAnimationComponentStatic(mEntityID1, &mSprites, 1, 0);
		addBlitzTimelineComponent(mEntityID1, &mTimelineAnimations);
		playBlitzTimelineAnimation(mEntityID1, 1);
		mEntityID2 = addBlitzEntity(makePosition(0, 0, 2));
		addBlitzMugenAnimationComponentStatic(mEntityID2, &mSprites, 1, 1);
		addBlitzTimelineComponent(mEntityID2, &mTimelineAnimations);
		playBlitzTimelineAnimation(mEntityID2, 2);
		mEntityID3 = addBlitzEntity(makePosition(0, 0, 3));
		addBlitzMugenAnimationComponentStatic(mEntityID3, &mSprites, 1, 2);
		addBlitzTimelineComponent(mEntityID3, &mTimelineAnimations);
		playBlitzTimelineAnimation(mEntityID3, 3);
	}

	~WarningScreen()
	{
	}

	int mNow = 0;
	void update()
	{
		if(mNow++ == 770)
		{
			gotoTitleScreen(nullptr);
		}

		if(hasPressedStartFlank())
		{
			addFadeOut(20, gotoTitleScreen);

		}
	}


};

EXPORT_SCREEN_CLASS(WarningScreen);
