#pragma once

#include <prism/actorhandler.h>

ActorBlueprint getDialogHandler();


void startPreDialog();
void startPostDialog();
void startCustomDialog();
int isDialogActive();