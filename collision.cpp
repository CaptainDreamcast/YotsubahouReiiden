#include "collision.h"

#include <prism/blitz.h>

static struct {
	int mPlayerList;
	int mEnemyList;
	int mPlayerShotList;
	int mEnemyShotList;

	int mPlayerCollectionList;
	int mItemList;

} gGameCollisionData;

void loadGameCollisions()
{
	gGameCollisionData.mPlayerList = addCollisionListToHandler();
	gGameCollisionData.mEnemyList = addCollisionListToHandler();
	gGameCollisionData.mPlayerShotList = addCollisionListToHandler();
	gGameCollisionData.mEnemyShotList = addCollisionListToHandler();
	gGameCollisionData.mPlayerCollectionList = addCollisionListToHandler();
	gGameCollisionData.mItemList = addCollisionListToHandler();
	addCollisionHandlerCheck(gGameCollisionData.mPlayerList, gGameCollisionData.mEnemyShotList);
	addCollisionHandlerCheck(gGameCollisionData.mPlayerList, gGameCollisionData.mEnemyList);
	addCollisionHandlerCheck(gGameCollisionData.mEnemyList, gGameCollisionData.mPlayerShotList);
	addCollisionHandlerCheck(gGameCollisionData.mPlayerCollectionList, gGameCollisionData.mItemList);
}

int getPlayerCollisionList()
{
	return gGameCollisionData.mPlayerList;
}

int getEnemyCollisionList()
{
	return gGameCollisionData.mEnemyList;
}

int getPlayerShotCollisionList()
{
	return gGameCollisionData.mPlayerShotList;
}

int getEnemyShotCollisionList()
{
	return gGameCollisionData.mEnemyShotList;
}

int getPlayerCollectionList()
{
	return gGameCollisionData.mPlayerCollectionList;
}

int getItemCollisionList()
{
	return gGameCollisionData.mItemList;
}
