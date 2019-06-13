#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

ActorBlueprint getItemHandler();

void addPowerItems(Position tPos, int tPower);
void addScoreItems(Position tPos, int tPower);
void addBombItem(Position tPos);
void setItemsAutocollect();