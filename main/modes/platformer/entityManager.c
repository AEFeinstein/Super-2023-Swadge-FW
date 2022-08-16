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
        initializeEntity(&(entityManager->entities[i]), entityManager, tilemap, gameData);
    }

    entityManager->activeEntities = 0;
    entityManager->tilemap = tilemap;

    createPlayer(entityManager, 33, 27);
    entityManager->viewEntity = 0;
    createTestObject(entityManager, 104, 202);
};

void loadSprites(entityManager_t * entityManager)
{
    loadWsg("sprite000.wsg", &entityManager->sprites[0]);
    loadWsg("sprite001.wsg", &entityManager->sprites[1]);
    loadWsg("sprite002.wsg", &entityManager->sprites[2]);
    loadWsg("sprite003.wsg", &entityManager->sprites[3]);
    loadWsg("sprite004.wsg", &entityManager->sprites[4]);
    loadWsg("sprite008.wsg", &entityManager->sprites[5]);
    loadWsg("tile064.wsg", &entityManager->sprites[6]);
    loadWsg("tile032.wsg", &entityManager->sprites[7]);
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
            drawWsg(disp, &entityManager->sprites[currentEntity.spriteIndex], (currentEntity.x >> SUBPIXEL_RESOLUTION) - 8 - entityManager->tilemap->mapOffsetX, (currentEntity.y >> SUBPIXEL_RESOLUTION)  - entityManager->tilemap->mapOffsetY - 8, currentEntity.spriteFlipHorizontal, false, 0);
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

entity_t* createEntity(entityManager_t *entityManager, uint8_t objectIndex, uint16_t x, uint16_t y){
    //if(entityManager->activeEntities == MAX_ENTITIES){
    //    return NULL;
    //}

    entity_t *createdEntity;

    switch(objectIndex){
        case ENTITY_PLAYER:
            createdEntity = createPlayer(entityManager, x, y);
            break;
        case ENTITY_TEST:
            createdEntity = createTestObject(entityManager, x, y);
            break;
        case ENTITY_HIT_BLOCK:
            createdEntity = createHitBlock(entityManager, x, y);
            break;
        default:
            ;
    }

    //if(createdEntity != NULL) {
    //    entityManager->activeEntities++;
    //}

    return createdEntity;
}

entity_t* createPlayer(entityManager_t * entityManager, uint16_t x, uint16_t y)
{
    entity_t * entity = findInactiveEntity(entityManager);

    if(entity == NULL) {
        return NULL;
    }

    entity->active = true;
    entity->x = x << SUBPIXEL_RESOLUTION;
    entity->y = y << SUBPIXEL_RESOLUTION;

    entity->xspeed = 0;
    entity->yspeed = 0;
    entity->xMaxSpeed = 120; //72; Walking
    entity->yMaxSpeed = 132; //72;
    entity->xDamping = 8;
    entity->yDamping = 8;
    entity->gravityEnabled = true;
    entity->gravity = 32;
    entity->falling = true;
    entity->jumpPower = 0;

    entity->type = ENTITY_PLAYER;
    entity->spriteIndex = 1;
    entity->updateFunction = &updatePlayer;
    entity->collisionHandler = &playerCollisionHandler;
    entity->tileCollisionHandler = &playerTileCollisionHandler;

    return entity;
}

entity_t* createTestObject(entityManager_t * entityManager, uint16_t x, uint16_t y)
{
    entity_t * entity = findInactiveEntity(entityManager);

    if(entity == NULL) {
        return NULL;
    }

    entity->active = true;
    entity->x = x << SUBPIXEL_RESOLUTION;
    entity->y = y << SUBPIXEL_RESOLUTION;
    
    entity->xspeed = (x < (entityManager->tilemap->mapOffsetX + 120)) ? 16 : -16;
    entity->yspeed = 0;
    entity->xMaxSpeed = 132;
    entity->yMaxSpeed = 132;
    entity->gravityEnabled = true;
    entity->gravity = 32;

    entity->type = ENTITY_TEST;
    entity->spriteIndex = 5;
    entity->updateFunction = &updateTestObject;
    entity->collisionHandler = &enemyCollisionHandler;
    entity->tileCollisionHandler = &enemyTileCollisionHandler;

    return entity;
}

entity_t* createHitBlock(entityManager_t * entityManager, uint16_t x, uint16_t y)
{
    entity_t * entity = findInactiveEntity(entityManager);

    if(entity == NULL) {
        return NULL;
    }

    entity->active = true;
    entity->x = x << SUBPIXEL_RESOLUTION;
    entity->y = y << SUBPIXEL_RESOLUTION;
    
    entity->xspeed = 0;
    entity->yspeed = 0;
    entity->xMaxSpeed = 132;
    entity->yMaxSpeed = 132;
    entity->gravityEnabled = true;
    entity->gravity = 32;

    entity->type = ENTITY_TEST;
    entity->spriteIndex = 6;
    entity->animationTimer = 0;
    entity->updateFunction = &updateHitBlock;
    entity->collisionHandler = &dummyCollisionHandler;
    entity->tileCollisionHandler = &dummyTileCollisionHandler;

    return entity;
}