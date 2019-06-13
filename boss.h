#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>
#include <prism/stlutil.h>

ActorBlueprint getBossHandler();

void addBoss(std::string tName);

int hasActiveBoss();
Position getBossPosition();
void bossFinishedDialogCB();
void addBossDamage(int tDamage);
void introBoss(void(*tFunc)(void*), int tNow);