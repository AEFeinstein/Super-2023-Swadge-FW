#ifndef _ENTITYMANAGER_H_
#define _ENTITYMANAGER_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "common_typedef.h"
#include "entity.h"
#include "tilemap.h"
#include "gameData.h"
#include "display.h"

//==============================================================================
// Structs
//==============================================================================

struct entityManager_t
{
    wsg_t sprites[8];
    entity_t * entities;
    uint8_t activeEntities;

    int16_t viewEntity;

    tilemap_t * tilemap;
};

//==============================================================================
// Prototypes
//==============================================================================
void initializeEntityManager(entityManager_t * entityManager, tilemap_t * tilemap, gameData_t * gameData);
void loadSprites(entityManager_t * entityManager);
void updateEntities(entityManager_t * entityManager);
void drawEntities(display_t * disp, entityManager_t * entityManager);
entity_t * findInactiveEntity(entityManager_t * entityManager);

void viewFollowEntity(tilemap_t * tilemap, entity_t * entity);
bool createEntity(entityManager_t *entityManager, uint8_t objectIndex, uint16_t x, uint16_t y);
bool createPlayer(entityManager_t * entityManager, uint16_t x, uint16_t y);
bool createTestObject(entityManager_t * entityManager, uint16_t x, uint16_t y);

#endif
