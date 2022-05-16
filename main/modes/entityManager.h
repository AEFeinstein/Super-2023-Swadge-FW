#ifndef _ENTITYMANAGER_H_
#define _ENTITYMANAGER_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "entity.h"
#include "tilemap.h"
#include "display.h"

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    wsg_t sprites[8];
    entity_t entities[16];
    uint8_t activeEntities;

    tilemap_t * tilemap;
} entityManager_t;


//==============================================================================
// Prototypes
//==============================================================================
void initializeEntityManager(entityManager_t * entityManager, tilemap_t * tilemap);
void loadSprites(entityManager_t * entityManager);
void updateEntities(entityManager_t * entityManager);
void drawEntities(display_t * disp, entityManager_t * entityManager);
entity_t * findInactiveEntity(entityManager_t * entityManager);

void createTestObject(entityManager_t * entityManager, int16_t x, int16_t y);

#endif
