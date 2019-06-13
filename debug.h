#pragma once

#include <prism/debug.h>
#include <prism/stlutil.h>
#include <prism/geometry.h>

using namespace std;

class GameVars {
public:
	int drawPaths = 0;

	Vector3DI gameScreen = makeVector3DI(192, 224, 1);
	Vector3DI gameScreenOffset = makeVector3DI(16, 8, 0);
};

extern GameVars gGameVars;