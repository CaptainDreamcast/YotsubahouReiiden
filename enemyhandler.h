#pragma once
#include <vector>
#include <prism/actorhandler.h>
#include <prism/mugenanimationhandler.h>

#include "level.h"

struct ActiveEnemy {

	LevelEnemy* mData;

	int mEntityID;
	int mCurrentPathPosition;
	double mPathSectionPosition;
	int mIsInvincible;

	int m_CurrentShotPosition;
	int m_DeltaTime;
	int m_shotFrequencyNow;
	int m_shotFrequencyNext;
	int m_id;
	int mChangeAnimationNow;
	int mLifeNow;
	ActiveEnemy(LevelEnemy* tEnemy, int tID);
	~ActiveEnemy();
};

ActorBlueprint getEnemyHandler();

MugenSpriteFile* getEnemySprites();
MugenAnimations* getEnemyAnimations();

void addActiveEnemy(LevelEnemy* tEnemy);
int isActiveEnemy(ActiveEnemy* tEnemy);
std::vector<Position> getFullControlPointsForEnemyPath(const std::vector<EnemyPathPoint>& controls);
void removeAllEnemies();

int getActiveEnemyAmount();
void addDamageToAllEnemies(int tDamage);
ActiveEnemy* getClosestEnemy();