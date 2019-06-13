#include "bghandler.h"

#include <prism/blitz.h>

#include "level.h"
#include "debug.h"

#define BG_Z 1

struct BGHandler {

	int mBlitzEntity1;
	int mBlitzEntity2;
	Vector3DI mSize;

	TextureData mWhiteTexture;
	int blackAnimationID;

	BGHandler(){
		auto animations = getLevelAnimations();
		auto sprites = getLevelSprites();
		auto animation = getMugenAnimation(animations, 1);
		mSize = getAnimationFirstElementSpriteSize(animation, sprites);

		mBlitzEntity1 = addBlitzEntity(makePosition(gGameVars.gameScreenOffset.x, -mSize.y, BG_Z));
		addBlitzMugenAnimationComponent(mBlitzEntity1, sprites, animations, 1);
		addBlitzPhysicsComponent(mBlitzEntity1);
		addBlitzPhysicsVelocityY(mBlitzEntity1, 6);

		mBlitzEntity2 = addBlitzEntity(makePosition(gGameVars.gameScreenOffset.x, 0, BG_Z));
		addBlitzMugenAnimationComponent(mBlitzEntity2, sprites, animations, 1);
		addBlitzPhysicsComponent(mBlitzEntity2);
		addBlitzPhysicsVelocityY(mBlitzEntity2, 6);

		mWhiteTexture = createWhiteTexture();
		blackAnimationID = playOneFrameAnimationLoop(makePosition(0, 0, BG_Z + 1), &mWhiteTexture);
		setAnimationSize(blackAnimationID, makePosition(320, 240, 1), makePosition(0, 0, 0));
		setAnimationColor(blackAnimationID, 0, 0, 0);
		setAnimationTransparency(blackAnimationID, 0.3);
	}

	void updateSingle(int entityID) {
		auto pos = getBlitzEntityPositionReference(entityID);
		if (pos->y > gGameVars.gameScreenOffset.y + gGameVars.gameScreen.y) {
			pos->y -= mSize.y * 2;
		}
	}

	void update() {
		updateSingle(mBlitzEntity1);
		updateSingle(mBlitzEntity2);
	}

};

EXPORT_ACTOR_CLASS(BGHandler);
