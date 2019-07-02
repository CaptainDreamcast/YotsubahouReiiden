#pragma once

#include <string>
#include <prism/actorhandler.h>
#include "prism/mugenspritefilereader.h"
#include "prism/mugenanimationreader.h"

namespace PlayerFuncs {
	enum PlayerEnum {
		PLAYER_YOURNAMEHERE,
		PLAYER_KINOMOD,
		PLAYER_AEROLITE,
	};
}

ActorBlueprint getPlayerHandler();
const std::string& getPlayerName();
void setPlayerName(const std::string& tName);
PlayerFuncs::PlayerEnum getPlayerEnumValue();
MugenSpriteFile* getPlayerSprites();
MugenAnimations* getPlayerAnimations();

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
int getContinueAmount();
void resetPlayer();
void usePlayerContinue();

int getCollectedItemAmount();
int getNextItemLifeUpAmount();