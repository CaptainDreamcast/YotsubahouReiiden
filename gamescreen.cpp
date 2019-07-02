#include "gamescreen.h"

#include <prism/blitz.h>

#include "player.h"
#include "collision.h"
#include "shothandler.h"
#include "level.h"
#include "enemyhandler.h"
#include "uihandler.h"
#include "itemhandler.h"
#include "boss.h"
#include "dialoghandler.h"
#include "debug.h"
#include "levelintro.h"
#include "bghandler.h"
#include "inmenu.h"

struct GameScreen {

	GameScreen() {
		instantiateActor(getBlitzCameraHandler());

		loadGameCollisions();
		instantiateActor(getLevelIntro());
		instantiateActor(getUIHandler());
		instantiateActor(getDialogHandler());
		instantiateActor(getBossHandler());
		instantiateActor(getItemHandler());
		instantiateActor(getEnemyHandler());
		instantiateActor(getLevelHandler());
		instantiateActor(getPlayerHandler());
		instantiateActor(getShotHandler());
		instantiateActor(getBGHandler());
		int id = instantiateActor(getInMenu());
		setActorUnpausable(id);
	}

	void update()
	{
		if(!isInMenuActive() && hasPressedAbortFlank())
		{
			setPauseMenuActive();
		}
	}
};

EXPORT_SCREEN_CLASS(GameScreen);