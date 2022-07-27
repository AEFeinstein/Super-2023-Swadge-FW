#ifndef _ENTITY_H_
#define _ENTITY_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "common_typedef.h"
#include "tilemap.h"
#include "gameData.h"

//==============================================================================
// Enums
//==============================================================================

typedef enum {
    ENTITY_PLAYER,
    ENTITY_TEST
} entityIndex_t;

//==============================================================================
// Structs
//==============================================================================

typedef void(*updateFunction_t)(struct entity_t *self);
typedef void(*collisionHandler_t)(struct entity_t *self, struct entity_t *other);

struct entity_t
{
    bool active;
    //bool important;

    uint8_t type;
    updateFunction_t updateFunction;

    uint16_t x;
    uint16_t y;
    
    int16_t xspeed;
    int16_t yspeed;

    int16_t xMaxSpeed;
    int16_t yMaxSpeed;

    int16_t xDamping;
    int16_t yDamping;

    bool gravityEnabled;
    int16_t gravity;
    bool falling;

    uint8_t spriteIndex;
    bool spriteFlipHorizontal;
    uint8_t animationTimer;

    tilemap_t * tilemap;
    gameData_t * gameData;

    uint8_t homeTileX;
    uint8_t homeTileY;

    int16_t jumpPower;
    
    entity_t *entities;

    collisionHandler_t collisionHandler;
};

//==============================================================================
// Prototypes
//==============================================================================
void initializeEntity(entity_t * self, entity_t * entities, tilemap_t * tilemap, gameData_t * gameData);

void updatePlayer(entity_t * self) ;

void updateTestObject(entity_t * self);

void moveEntityWithTileCollisions(entity_t * self);

void despawnWhenOffscreen(entity_t *self);

void destroyEntity(entity_t *self, bool respawn);

void applyDamping(entity_t *self);

void applyGravity(entity_t *self);

void animatePlayer(entity_t * self);

void detectEntityCollisions(entity_t *self);

void playerCollisionHandler(entity_t *self, entity_t* other);
void enemyCollisionHandler(entity_t *self, entity_t *other);

#endif
