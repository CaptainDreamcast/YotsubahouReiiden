#include "itemhandler.h"

#include <prism/blitz.h>

#include "uihandler.h"
#include "collision.h"
#include "player.h"
#include "debug.h"
#include "level.h"

#define ITEM_Z 10

static void itemHitCB(void* tCaller, void* tCollisionData);

struct Item {
	int m_entityID;
	int m_tType;
	int m_isHuge;
	int m_id;
	int mIsAutocollected = 0;

	enum ItemType {
		POWER,
		SCORE,
		BOMB,
		LIFE
	};

	Item(Position tPos, ItemType tType, int isHuge, int tID, Position tDirection = makePosition(0, 0.5, 0)) {
		m_tType = tType;
		m_isHuge = isHuge;
		m_id = tID;
		tPos.z = ITEM_Z;
		m_entityID = addBlitzEntity(tPos);
		int animation;
		switch (tType) {
		case POWER:
			animation = (isHuge ? 11 : 10);
			break;
		case SCORE:
			animation = 12;
			break;
		case BOMB:
			animation = 17;
			break;
		case LIFE:
			animation = 18;
			break;
		default:
			animation = 11;
			break;
		}
		addBlitzMugenAnimationComponent(m_entityID, getUISprites(), getUIAnimations(), animation);
		addBlitzCollisionComponent(m_entityID);
		const int collisionID = addBlitzCollisionCirc(m_entityID, getItemCollisionList(), makeCollisionCirc(makePosition(0, 0, 0), isHuge ? 10 : 5));
		addBlitzCollisionCB(m_entityID, collisionID, itemHitCB, this);
		addBlitzPhysicsComponent(m_entityID);
		addBlitzPhysicsVelocity(m_entityID, tDirection);
	}

	void setAutocollect() {
		mIsAutocollected = 1;
	}

	void updateAutocollect() {
		if (!mIsAutocollected) return;

		Position playerPos = getBlitzEntityPosition(getPlayerEntity());
		Position* p = getBlitzEntityPositionReference(m_entityID);
		
		Position delta = playerPos - *p;
		delta.z = 0;

		*p += delta * 0.2;
	}

	void updateSpeed()
	{
		if (mIsAutocollected) return;
		Position* v = getBlitzPhysicsVelocityReference(m_entityID);
		v->y = std::min(v->y + 0.02, 0.5);
		v->x *= 0.1;
	}

	int update() {
		updateAutocollect();
		updateSpeed();
		const auto y = getBlitzEntityPositionY(m_entityID);
		return y > getScreenPositionFromGamePositionY(1.2);
	}

	~Item() {
		removeBlitzEntity(m_entityID);
	}
};

typedef unique_ptr<Item> ItemPtr;

static struct {
	map<int, ItemPtr> mItems;
} gItemHandler;

static void loadItemHandler(void* tCaller) {
	(void)tCaller;
	gItemHandler.mItems.clear();
}

static void unloadItemHandler(void* tCaller) {
	(void)tCaller;
	gItemHandler.mItems.clear();
}

static void updateItemHandler(void* tCaller) {
	stl_int_map_remove_predicate(gItemHandler.mItems, &Item::update);
}

ActorBlueprint getItemHandler()
{
	return makeActorBlueprint(loadItemHandler, unloadItemHandler, updateItemHandler);
}

static void itemHitCB(void* tCaller, void* tCollisionData) {
	(void)tCollisionData;
	Item* item = (Item*)tCaller;


	double t = 0;
	double y = 0;
	switch (item->m_tType) {
	case Item::ItemType::POWER:
		addPlayerPower(item->m_isHuge ? 10 : 1);
		break;
	case Item::ItemType::SCORE:
		y = getBlitzEntityPositionY(item->m_entityID);
		t = 1 - (y / 240);
		if (item->mIsAutocollected) t = 1;
		addPlayerScoreItem(int((1 + t*2) * (item->m_isHuge ? 10 : 1)));
		break;
	case Item::ItemType::BOMB:
		addPlayerBomb();
		break;
	case Item::ItemType::LIFE:
		addPlayerLife();
		break;
	default:
		break;
	}

	gItemHandler.mItems.erase(item->m_id);
}

void addPowerItems(Position tPos, int tPower)
{
	while (tPower >= 10) {
		Position finalPos = tPos + makePosition(randfrom(-10, 10), randfrom(-10, 10), 0);
		finalPos.z = randfrom(11.5, 12.5);
		int id = stl_int_map_get_id();
		gItemHandler.mItems.insert(make_pair(id, make_unique<Item>(finalPos, Item::ItemType::POWER, 1, id)));
		tPower -= 10;
	}

	while(tPower--) {
		Position finalPos = tPos + makePosition(randfrom(-5, 5), randfrom(-5, 5), 0);
		finalPos.z = randfrom(11.5, 12.5);
		int id = stl_int_map_get_id();
		gItemHandler.mItems.insert(make_pair(id, make_unique<Item>(finalPos, Item::ItemType::POWER, 0, id)));
	}

}

void addDeathPowerItems(Position tPos)
{
	int id = stl_int_map_get_id();
	gItemHandler.mItems.insert(make_pair(id, make_unique<Item>(tPos + makePosition(-20, -20, 0), Item::ItemType::POWER, 1, id, makePosition(-5, -0.5, 0))));
	id = stl_int_map_get_id();
	gItemHandler.mItems.insert(make_pair(id, make_unique<Item>(tPos + makePosition(20, -20, 0), Item::ItemType::POWER, 1, id, makePosition(5, -0.5, 0))));
}

void addScoreItems(Position tPos, int tScore)
{
	while (tScore--) {
		Position finalPos = tPos + makePosition(randfrom(-5, 5), randfrom(-5, 5), 0);
		finalPos.z = randfrom(11.5, 12.5);
		int id = stl_int_map_get_id();
		gItemHandler.mItems.insert(make_pair(id, make_unique<Item>(finalPos, Item::ItemType::SCORE, 0, id)));
	}
}

void addBombItem(Position tPos)
{
	Position finalPos = tPos + makePosition(randfrom(-10, 10), randfrom(-10, 10), 0);
	finalPos.z = randfrom(11.5, 12.5);
	int id = stl_int_map_get_id();
	gItemHandler.mItems.insert(make_pair(id, make_unique<Item>(finalPos, Item::ItemType::BOMB, 1, id)));
}

void addLifeItem(Position tPos)
{
	Position finalPos = tPos + makePosition(randfrom(-10, 10), randfrom(-10, 10), 0);
	finalPos.z = randfrom(11.5, 12.5);
	int id = stl_int_map_get_id();
	gItemHandler.mItems.insert(make_pair(id, make_unique<Item>(finalPos, Item::ItemType::LIFE, 1, id)));
}

void setItemsAutocollect()
{
	for (auto& item : gItemHandler.mItems) {
		item.second->setAutocollect();
	}
}
