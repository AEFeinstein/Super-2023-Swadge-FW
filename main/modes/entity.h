#ifndef _ENTITY_H_
#define _ENTITY_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "tilemap.h"
#include "gameData.h"

//==============================================================================
// Structs
//==============================================================================

struct entity_t;
typedef void(*updateFunction_t)(struct entity_t *self);

typedef struct
{
    bool active;
    //bool important;

    uint8_t type;
    updateFunction_t updateFunction;

    int16_t x;
    int16_t y;
    
    int16_t xspeed;
    int16_t yspeed;
    int16_t gravity;

    uint8_t spriteIndex;
    uint8_t animationTimer;

    tilemap_t * tilemap;
    gameData_t * gameData;
} entity_t;

//==============================================================================
// Prototypes
//==============================================================================
void initializeEntity(entity_t * entity, tilemap_t * tilemap, gameData_t * gameData);

void updateTestObject(entity_t * self);
void moveEntityWithTileCollisions(entity_t * self);

#endif
