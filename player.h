#pragma once

#include <string>
#include <prism/actorhandler.h>

ActorBlueprint getPlayerHandler();
void setPlayerName(const std::string& tName);

int getPlayerPower();
void addPlayerPower(int tPower);
uint64_t getPlayerScore();
void addPlayerScoreItem(int tScoreMultiplier);
void addPlayerScore(int tScore);
int getPlayerLife();
void addPlayerLife();
int getPlayerBombs();
void addPlayerBomb();
int getPlayerEntity();
int isPlayerBombActive();