#pragma once

#include <functional>
#include <string>
#include <prism/actorhandler.h>
#include <prism/geometry.h>

ActorBlueprint getShotHandler();

void addShot(void* tOwner, Position tPos, std::string tName, int tList);
void addAngledShot(void* tCaller, Position tPosition, const std::string& tName, int tCollisionList, int tAngle = 0, double tSpeed = 2);
void addAimedShot(void* tCaller, Position tPosition, const std::string& tName, int tCollisionList, int tAngleOffset = 0, double tSpeed = 2);
void removeEnemyBullets();
void removeEnemyBulletsExceptComplex();
int getShotDamage(void* tCollisionData);