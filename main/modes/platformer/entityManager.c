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

    
    entityManager->viewEntity = createPlayer(entityManager, entityManager->tilemap->warps[0].x * 16, entityManager->tilemap->warps[0].y * 16);
    entityManager->playerEntity = entityManager->viewEntity;
};

void loadSprites(entityManager_t * entityManager)
{
    loadWsg("sprite000.wsg", &entityManager->sprites[SP_PLAYER_IDLE]);
    loadWsg("sprite001.wsg", &entityManager->sprites[SP_PLAYER_WALK1]);
    loadWsg("sprite002.wsg", &entityManager->sprites[SP_PLAYER_WALK2]);
    loadWsg("sprite003.wsg", &entityManager->sprites[SP_PLAYER_WALK3]);
    loadWsg("sprite004.wsg", &entityManager->sprites[SP_PLAYER_JUMP]);
    loadWsg("sprite005.wsg", &entityManager->sprites[SP_PLAYER_SLIDE]);
    loadWsg("sprite006.wsg", &entityManager->sprites[SP_PLAYER_HURT]);
    loadWsg("sprite007.wsg", &entityManager->sprites[SP_PLAYER_CLIMB]);
    loadWsg("sprite008.wsg", &entityManager->sprites[SP_PLAYER_WIN]);
    loadWsg("sprite009.wsg", &entityManager->sprites[SP_ENEMY_BASIC]);
    loadWsg("tile066.wsg", &entityManager->sprites[SP_HITBLOCK_CONTAINER]);
    loadWsg("tile034.wsg", &entityManager->sprites[SP_HITBLOCK_BRICKS]);
    loadWsg("sprite012.wsg", &entityManager->sprites[SP_DUSTBUNNY_IDLE]);
    loadWsg("sprite013.wsg", &entityManager->sprites[SP_DUSTBUNNY_CHARGE]);
    loadWsg("sprite014.wsg", &entityManager->sprites[SP_DUSTBUNNY_JUMP]);
    loadWsg("sprite015.wsg", &entityManager->sprites[SP_GAMING_1]);
    loadWsg("sprite016.wsg", &entityManager->sprites[SP_GAMING_2]);
    loadWsg("sprite017.wsg", &entityManager->sprites[SP_GAMING_3]);
    loadWsg("sprite018.wsg", &entityManager->sprites[SP_MUSIC_1]);
    loadWsg("sprite019.wsg", &entityManager->sprites[SP_MUSIC_2]);
    loadWsg("sprite020.wsg", &entityManager->sprites[SP_MUSIC_3]);
    loadWsg("sprite021.wsg", &entityManager->sprites[SP_WARP_1]);
    loadWsg("sprite022.wsg", &entityManager->sprites[SP_WARP_2]);
    loadWsg("sprite023.wsg", &entityManager->sprites[SP_WARP_3]);
};

void updateEntities(entityManager_t * entityManager)
{
    for(uint8_t i=0; i < MAX_ENTITIES; i++)
    {
        if(entityManager->entities[i].active)
        {
            entityManager->entities[i].updateFunction(&(entityManager->entities[i]));

            if(&(entityManager->entities[i]) == entityManager->viewEntity){
                viewFollowEntity(entityManager->tilemap, &(entityManager->entities[i]));
            }
        }
    }
};

void deactivateAllEntities(entityManager_t * entityManager, bool excludePlayer){
    for(uint8_t i=0; i < MAX_ENTITIES; i++)
    {
        entity_t* currentEntity = &(entityManager->entities[i]);
        
        currentEntity->active = false;

    //TODO: respawn warp container blocks
    /*
        if(currentEntity->type == ENTITY_WARP){
            //In destroyEntity, this will overflow to the correct value.
            currentEntity->type = 128 + TILE_CONTAINER_1;
        }
    */
   
        if(excludePlayer && currentEntity == entityManager->playerEntity){
            currentEntity->active = true;
        }
    }
}

void drawEntities(display_t * disp, entityManager_t * entityManager)
{
    for(uint8_t i=0; i < MAX_ENTITIES; i++)
    {
        entity_t currentEntity = entityManager->entities[i];

        if(currentEntity.active)
        {
            drawWsg(disp, &entityManager->sprites[currentEntity.spriteIndex], (currentEntity.x >> SUBPIXEL_RESOLUTION) - 8 - entityManager->tilemap->mapOffsetX, (currentEntity.y >> SUBPIXEL_RESOLUTION)  - entityManager->tilemap->mapOffsetY - 8, currentEntity.spriteFlipHorizontal, currentEntity.spriteFlipVertical, 0);
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

    int16_t centerOfViewX = tilemap->mapOffsetX + 140;
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
        case ENTITY_SCROLL_LOCK_LEFT:
            createdEntity = createScrollLockLeft(entityManager, x, y);
            break;
        case ENTITY_SCROLL_LOCK_RIGHT:
            createdEntity = createScrollLockRight(entityManager, x, y);
            break;
        case ENTITY_SCROLL_LOCK_UP:
            createdEntity = createScrollLockUp(entityManager, x, y);
            break;
        case ENTITY_SCROLL_LOCK_DOWN:
            createdEntity = createScrollLockDown(entityManager, x, y);
            break;       
        case ENTITY_SCROLL_UNLOCK:
            createdEntity = createScrollUnlock(entityManager, x, y);
            break;
        case ENTITY_HIT_BLOCK:
            createdEntity = createHitBlock(entityManager, x, y);
            break;
        case ENTITY_POWERUP:
            createdEntity = createPowerUp(entityManager, x, y);
            break;
        case ENTITY_WARP:
            createdEntity = createWarp(entityManager, x, y);
            break;
        case ENTITY_DUST_BUNNY:
            createdEntity = createDustBunny(entityManager, x, y);
            break;
        default:
            createdEntity = NULL;
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
    entity->spriteFlipVertical = false;

    entity->type = ENTITY_PLAYER;
    entity->spriteIndex = SP_PLAYER_IDLE;
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
    entity->spriteFlipVertical = false;

    entity->type = ENTITY_TEST;
    entity->spriteIndex = SP_ENEMY_BASIC;
    entity->updateFunction = &updateTestObject;
    entity->collisionHandler = &enemyCollisionHandler;
    entity->tileCollisionHandler = &enemyTileCollisionHandler;

    return entity;
}

entity_t* createScrollLockLeft(entityManager_t * entityManager, uint16_t x, uint16_t y)
{
    entity_t * entity = findInactiveEntity(entityManager);

    if(entity == NULL) {
        return NULL;
    }

    entity->active = true;
    entity->x = x << SUBPIXEL_RESOLUTION;
    entity->y = y << SUBPIXEL_RESOLUTION;
    
    entity->type = ENTITY_SCROLL_LOCK_LEFT;
    entity->updateFunction = &updateScrollLockLeft;
    entity->collisionHandler = &dummyCollisionHandler;
    entity->tileCollisionHandler = &dummyTileCollisionHandler;

    return entity;
}

entity_t* createScrollLockRight(entityManager_t * entityManager, uint16_t x, uint16_t y)
{
    entity_t * entity = findInactiveEntity(entityManager);

    if(entity == NULL) {
        return NULL;
    }

    entity->active = true;
    entity->x = x << SUBPIXEL_RESOLUTION;
    entity->y = y << SUBPIXEL_RESOLUTION;
    
    entity->type = ENTITY_SCROLL_LOCK_RIGHT;
    entity->updateFunction = &updateScrollLockRight;
    entity->collisionHandler = &dummyCollisionHandler;
    entity->tileCollisionHandler = &dummyTileCollisionHandler;

    return entity;
}

entity_t* createScrollLockUp(entityManager_t * entityManager, uint16_t x, uint16_t y)
{
    entity_t * entity = findInactiveEntity(entityManager);

    if(entity == NULL) {
        return NULL;
    }

    entity->active = true;
    entity->x = x << SUBPIXEL_RESOLUTION;
    entity->y = y << SUBPIXEL_RESOLUTION;
    
    entity->type = ENTITY_SCROLL_LOCK_UP;
    entity->updateFunction = &updateScrollLockUp;
    entity->collisionHandler = &dummyCollisionHandler;
    entity->tileCollisionHandler = &dummyTileCollisionHandler;

    return entity;
}

entity_t* createScrollLockDown(entityManager_t * entityManager, uint16_t x, uint16_t y)
{
    entity_t * entity = findInactiveEntity(entityManager);

    if(entity == NULL) {
        return NULL;
    }

    entity->active = true;
    entity->x = x << SUBPIXEL_RESOLUTION;
    entity->y = y << SUBPIXEL_RESOLUTION;
    
    entity->type = ENTITY_SCROLL_LOCK_DOWN;
    entity->updateFunction = &updateScrollLockDown;
    entity->collisionHandler = &dummyCollisionHandler;
    entity->tileCollisionHandler = &dummyTileCollisionHandler;

    return entity;
}

entity_t* createScrollUnlock(entityManager_t * entityManager, uint16_t x, uint16_t y)
{
    entity_t * entity = findInactiveEntity(entityManager);

    if(entity == NULL) {
        return NULL;
    }

    entity->active = true;
    entity->x = x << SUBPIXEL_RESOLUTION;
    entity->y = y << SUBPIXEL_RESOLUTION;
    
    entity->type = ENTITY_SCROLL_UNLOCK;
    entity->updateFunction = &updateScrollUnlock;
    entity->collisionHandler = &dummyCollisionHandler;
    entity->tileCollisionHandler = &dummyTileCollisionHandler;

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

    entity->spriteFlipVertical = false;

    entity->type = ENTITY_HIT_BLOCK;
    entity->spriteIndex = SP_HITBLOCK_CONTAINER;
    entity->animationTimer = 0;
    entity->updateFunction = &updateHitBlock;
    entity->collisionHandler = &dummyCollisionHandler;
    entity->tileCollisionHandler = &dummyTileCollisionHandler;

    return entity;
}

entity_t* createPowerUp(entityManager_t * entityManager, uint16_t x, uint16_t y){
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

    entity->spriteFlipVertical = false;

    entity->type = ENTITY_POWERUP;
    entity->spriteIndex = SP_GAMING_1;
    entity->animationTimer = 0;
    entity->updateFunction = &updatePowerUp;
    entity->collisionHandler = &dummyCollisionHandler;
    entity->tileCollisionHandler = &dummyTileCollisionHandler;

    return entity;
};

entity_t* createWarp(entityManager_t * entityManager, uint16_t x, uint16_t y){
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

    entity->spriteFlipVertical = false;

    entity->type = ENTITY_WARP;
    entity->spriteIndex = SP_WARP_1;
    entity->animationTimer = 0;
    entity->updateFunction = &updateWarp;
    entity->collisionHandler = &dummyCollisionHandler;
    entity->tileCollisionHandler = &dummyTileCollisionHandler;

    return entity;
};

entity_t* createDustBunny(entityManager_t * entityManager, uint16_t x, uint16_t y)
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
    entity->xDamping = 0; //This will be repurposed to track state
    entity->yDamping = 0; //This will be repurposed as a state timer
    entity->gravityEnabled = true;
    entity->gravity = 32;
    entity->spriteFlipHorizontal = (x < (entityManager->tilemap->mapOffsetX + 120)) ? true : false;
    entity->spriteFlipVertical = false;

    entity->type = ENTITY_DUST_BUNNY;
    entity->spriteIndex = SP_DUSTBUNNY_IDLE;
    entity->updateFunction = &updateDustBunny;
    entity->collisionHandler = &enemyCollisionHandler;
    entity->tileCollisionHandler = &dustBunnyTileCollisionHandler;

    return entity;
}