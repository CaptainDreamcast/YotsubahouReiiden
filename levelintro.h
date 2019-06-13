#pragma once

#include <string>
#include <prism/actorhandler.h>

ActorBlueprint getLevelIntro();
void addLevelIntro(const std::string& tLevelName, const std::string& tLevelText);
int isLevelIntroActive();
