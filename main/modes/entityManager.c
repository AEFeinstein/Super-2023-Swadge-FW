//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <esp_log.h>
#include <string.h>

#include "entityManager.h"
#include "esp_random.h"

#include "../../components/hdw-spiffs/spiffs_manager.h"

//==============================================================================
// Constants
//==============================================================================
#define MAX_ENTITIES 16

//==============================================================================
// Functions
//==============================================================================
void initializeEntityManager(entityManager_t * entityManager, tilemap_t * tilemap)
{
    loadSprites(entityManager);
    for(uint8_t i=0; i < MAX_ENTITIES; i++)
    {
        initializeEntity(&(entityManager->entities[i]), tilemap);
    }

    entityManager->activeEntities = 0;
    entityManager->tilemap = tilemap;

    createTestObject(entityManager, 33, 27);
};

void loadSprites(entityManager_t * entityManager)
{
    loadWsg("tile001.wsg", &entityManager->sprites[0]);
    loadWsg("tile001.wsg", &entityManager->sprites[1]);
    loadWsg("tile001.wsg", &entityManager->sprites[2]);
    loadWsg("tile001.wsg", &entityManager->sprites[3]);
    loadWsg("tile001.wsg", &entityManager->sprites[4]);
    loadWsg("tile001.wsg", &entityManager->sprites[5]);
    loadWsg("tile001.wsg", &entityManager->sprites[6]);
    loadWsg("tile001.wsg", &entityManager->sprites[7]);
};

void updateEntities(entityManager_t * entityManager)
{
    for(u_int8_t i=0; i < MAX_ENTITIES; i++)
    {
        entity_t currentEntity = entityManager->entities[i];
        if(currentEntity.active)
        {
            currentEntity.updateFunction(&currentEntity);
        }
    }
};

void drawEntities(display_t * disp, entityManager_t * entityManager)
{
    for(uint8_t i=0; i < MAX_ENTITIES; i++)
    {
        entity_t currentEntity = entityManager->entities[i];
        drawWsg(disp, &entityManager->sprites[currentEntity.spriteIndex], currentEntity.x, currentEntity.y);
    }
};

entity_t * findInactiveEntity(entityManager_t * entityManager)
{
    if(entityManager->activeEntities == MAX_ENTITIES)
    {
        return NULL;
    };
    
    uint8_t entityIndex = 0;
    entity_t currentEntity = entityManager->entities[entityIndex];

    while(currentEntity.active){
        entityIndex++;

        //Extra safeguard to make sure we don't get stuck here
        if(entityIndex > MAX_ENTITIES)
        {
            return NULL;
        }

        entity_t currentEntity = entityManager->entities[entityIndex];
    }

    return &currentEntity;
}

void createTestObject(entityManager_t * entityManager, int16_t x, int16_t y)
{
    entity_t * entity = findInactiveEntity(entityManager);

    entity->x = x;
    entity->y = y;

    entity->type = 0;
    entity->spriteIndex = 1;
    entity->updateFunction = &updateTestObject;
}