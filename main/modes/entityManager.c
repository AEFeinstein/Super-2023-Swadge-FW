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
#define SUBPIXEL_RESOLUTION 4

//==============================================================================
// Functions
//==============================================================================
void initializeEntityManager(entityManager_t * entityManager, tilemap_t * tilemap, gameData_t * gameData)
{
    loadSprites(entityManager);
    entityManager->entities = malloc(sizeof(entity_t) * MAX_ENTITIES);
    
    for(uint8_t i=0; i < MAX_ENTITIES; i++)
    {
        initializeEntity(&(entityManager->entities[i]), tilemap, gameData);
    }

    entityManager->activeEntities = 0;
    entityManager->tilemap = tilemap;

    createTestObject(entityManager, 33, 27);
    entityManager->viewEntity = 0;
    //createTestObject(entityManager, 104, 202);
};

void loadSprites(entityManager_t * entityManager)
{
    loadWsg("tile003.wsg", &entityManager->sprites[0]);
    loadWsg("tile004.wsg", &entityManager->sprites[1]);
    loadWsg("tile005.wsg", &entityManager->sprites[2]);
    loadWsg("tile001.wsg", &entityManager->sprites[3]);
    loadWsg("tile001.wsg", &entityManager->sprites[4]);
    loadWsg("tile001.wsg", &entityManager->sprites[5]);
    loadWsg("tile001.wsg", &entityManager->sprites[6]);
    loadWsg("tile001.wsg", &entityManager->sprites[7]);
};

void updateEntities(entityManager_t * entityManager)
{
    for(uint8_t i=0; i < MAX_ENTITIES; i++)
    {
        if(entityManager->entities[i].active)
        {
            entityManager->entities[i].updateFunction(&(entityManager->entities[i]));

            if(i == entityManager->viewEntity){
                viewFollowEntity(entityManager->tilemap, &(entityManager->entities[i]));
            }
        }
    }
};

void drawEntities(display_t * disp, entityManager_t * entityManager)
{
    for(uint8_t i=0; i < MAX_ENTITIES; i++)
    {
        entity_t currentEntity = entityManager->entities[i];

        if(currentEntity.active)
        {
            drawWsg(disp, &entityManager->sprites[currentEntity.spriteIndex], (currentEntity.x >> SUBPIXEL_RESOLUTION) - 8 - entityManager->tilemap->mapOffsetX, (currentEntity.y >> SUBPIXEL_RESOLUTION)  - entityManager->tilemap->mapOffsetY - 8, false, false, 0);
        }
    }
};

entity_t * findInactiveEntity(entityManager_t * entityManager)
{
    if(entityManager->activeEntities == MAX_ENTITIES)
    {
        return NULL;
    };
    
    uint8_t entityIndex = 0;

    while(entityManager->entities[entityIndex].active){
        entityIndex++;

        //Extra safeguard to make sure we don't get stuck here
        if(entityIndex > MAX_ENTITIES)
        {
            return NULL;
        }
    }

    return &(entityManager->entities[entityIndex]);
}

void createTestObject(entityManager_t * entityManager, int16_t x, int16_t y)
{
    entity_t * entity = findInactiveEntity(entityManager);

    entity->active = true;
    entity->x = x << SUBPIXEL_RESOLUTION;
    entity->y = y << SUBPIXEL_RESOLUTION;

    entity->type = 0;
    entity->spriteIndex = 1;
    entity->updateFunction = &updateTestObject;
}

void viewFollowEntity(tilemap_t * tilemap, entity_t * entity){
    int16_t moveViewByX = (entity->x) >> SUBPIXEL_RESOLUTION;
    int16_t moveViewByY = (entity->y) >> SUBPIXEL_RESOLUTION;

    int16_t centerOfViewX = tilemap->mapOffsetX + 120;
    int16_t centerOfViewY = tilemap->mapOffsetY + 120;

    //if(centerOfViewX != moveViewByX) {
        moveViewByX -= centerOfViewX;
    //}

    //if(centerOfViewY != moveViewByY) {
        moveViewByY -= centerOfViewY;
    //}

    //if(moveViewByX && moveViewByY){
        scrollTileMap(tilemap, moveViewByX, moveViewByY);
    //}
}