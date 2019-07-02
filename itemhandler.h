#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

ActorBlueprint getItemHandler();

void addPowerItems(Position tPos, int tPower);
void addDeathPowerItems(Position tPos);
void addScoreItems(Position tPos, int tPower);
void addBombItem(Position tPos);
void addLifeItem(Position tPos);
void setItemsAutocollect();