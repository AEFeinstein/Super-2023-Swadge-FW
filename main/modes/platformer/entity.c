//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include "entity.h"
#include "entityManager.h"
#include "tilemap.h"
#include "gameData.h"
#include "musical_buzzer.h"
#include "btn.h"
#include "esp_random.h"

//==============================================================================
// Constants
//==============================================================================
#define SUBPIXEL_RESOLUTION 4
#define TILE_SIZE_IN_POWERS_OF_2 4
#define TILE_SIZE 16
#define HALF_TILE_SIZE 8
#define DESPAWN_THRESHOLD 64

#define SIGNOF(x) ((x > 0) - (x < 0))
#define TO_TILE_COORDS(x) ((x) >> TILE_SIZE_IN_POWERS_OF_2)
// #define TO_PIXEL_COORDS(x) ((x) >> SUBPIXEL_RESOLUTION)
// #define TO_SUBPIXEL_COORDS(x) ((x) << SUBPIXEL_RESOLUTION)

static const song_t sndHit =
    {
        .notes =
            {
                {740, 10}, {840, 10}, {940, 10}},
        .numNotes = 3,
        .shouldLoop = false
    };

static const song_t sndCoin =
    {
        .notes =
            {
                {1000, 50}, {1200, 100}},
        .numNotes = 2,
        .shouldLoop = false
    };

static const song_t sndPowerUp =
    {
        .notes =
            { 
                {C_4, 100}, {G_4, 100}, {E_4, 100}, {C_5, 100}, {G_4, 100}, {D_5, 100}, {C_5, 100}},
        .numNotes = 7,
        .shouldLoop = false
    };

static const song_t sndJump1 =
    {
        .notes =
            { 
                {C_5, 50}, {E_5, 50}, {C_5, 50}},
        .numNotes = 3,
        .shouldLoop = false
    };         

static const song_t sndJump2 =
    {
        .notes =
            { 
                {E_5, 50}, {G_5, 50}, {E_5, 50}},
        .numNotes = 3,
        .shouldLoop = false
    };         

static const song_t sndJump3 =
    {
        .notes =
            { 
                {G_5, 50}, {C_6, 50}, {G_5, 50}},
        .numNotes = 3,
        .shouldLoop = false
    };                 

//==============================================================================
// Functions
//==============================================================================
void initializeEntity(entity_t *self, entityManager_t *entityManager, tilemap_t *tilemap, gameData_t *gameData)
{
    self->active = false;
    self->tilemap = tilemap;
    self->gameData = gameData;
    self->homeTileX = 0;
    self->homeTileY = 0;
    self->gravity = false;
    self->falling = false;
    self->entityManager = entityManager;
    self->spriteFlipVertical = false;
};

void updatePlayer(entity_t *self)
{
    if (self->gameData->btnState & BTN_B)
    {
        self->xMaxSpeed = 132;
    }
    else
    {
        self->xMaxSpeed = 72;
    }

    if (self->gameData->btnState & LEFT)
    {
        self->xspeed -= (self->falling) ? 12 : 16;

        if (self->xspeed < -self->xMaxSpeed)
        {
            self->xspeed = -self->xMaxSpeed;
        }
    }
    else if (self->gameData->btnState & RIGHT)
    {
        self->xspeed += (self->falling) ? 12 : 16;

        if (self->xspeed > self->xMaxSpeed)
        {
            self->xspeed = self->xMaxSpeed;
        }
    }

    if (self->gameData->btnState & UP)
    {
        self->yspeed -= 16;

        if (self->yspeed < -self->yMaxSpeed)
        {
            self->yspeed = -self->yMaxSpeed;
        }
    }
    else if (self->gameData->btnState & DOWN)
    {
        self->yspeed += 16;

        if (self->yspeed > self->yMaxSpeed)
        {
            self->yspeed = self->yMaxSpeed;
        }
    }

    if (self->gameData->btnState & BTN_A)
    {
        if (!self->falling && !(self->gameData->prevBtnState & BTN_A))
        {
            // initiate jump
            self->jumpPower = 180 + (abs(self->xspeed) >> 2);
            self->yspeed = -self->jumpPower;
            self->falling = true;
            buzzer_play_sfx(&sndJump1);
        }
        else if (self->jumpPower > 0 && self->yspeed < 0)
        {
            // jump dampening
            self->jumpPower -= 16; // 32
            self->yspeed = -self->jumpPower;
            
            if(self->jumpPower > 112 && self->jumpPower < 128){
                buzzer_play_sfx(&sndJump2);
            }

            if(self->yspeed > -24 && self->yspeed < -16){
                buzzer_play_sfx(&sndJump3);
            }

            if (self->jumpPower < 0)
            {
                self->jumpPower = 0;
            }
        }
    }
    else if (self->falling && self->jumpPower > 0 && self->yspeed < 0)
    {
        // Cut jump short if player lets go of jump button
        self->jumpPower = 0;
        self->yspeed = self->yspeed >> 2; // technically shouldn't do this with a signed int
    }

    if(self->invincibilityFrames > 0){
        self->invincibilityFrames--;
        self->visible = (self->invincibilityFrames % 2);
        if(self->invincibilityFrames <= 0){
            self->visible = true;
        }
    }

    moveEntityWithTileCollisions(self);
    dieWhenFallingOffScreen(self);
    applyGravity(self);
    applyDamping(self);
    detectEntityCollisions(self);
    animatePlayer(self);
    
};

void updateTestObject(entity_t *self)
{
    self->spriteFlipHorizontal = !self->spriteFlipHorizontal;

    despawnWhenOffscreen(self);
    moveEntityWithTileCollisions(self);
    applyGravity(self);
    detectEntityCollisions(self);
};

void updateHitBlock(entity_t *self)
{
    self->x += self->xspeed;
    self->y += self->yspeed;

    self->animationTimer++;
    if (self->animationTimer == 2)
    {
        self->xspeed = -self->xspeed;
        self->yspeed = -self->yspeed;
    }
    if (self->animationTimer > 4)
    {
        uint8_t aboveTile = self->tilemap->map[(self->homeTileY - 1) * self->tilemap->mapWidth + self->homeTileX];
        entity_t *createdEntity = NULL;

        switch (aboveTile)
        {
            case TILE_CTNR_COIN:
            {
                self->gameData->coins++;
                self->gameData->score += 50;
                buzzer_play_sfx(&sndCoin);
                self->jumpPower = TILE_CONTAINER_2;
                break;
            }
            case TILE_CTNR_POW1:
            {
                createdEntity = createEntity(self->entityManager, ENTITY_POWERUP, (self->homeTileX * TILE_SIZE) + HALF_TILE_SIZE, ((self->homeTileY - 1) * TILE_SIZE) + HALF_TILE_SIZE);
                createdEntity->homeTileX = 0;
                createdEntity->homeTileY = 0;

                self->jumpPower = TILE_CONTAINER_2;
                break;
            }
            case TILE_WARP_0 ... TILE_WARP_F:
            {
                createdEntity = createEntity(self->entityManager, ENTITY_WARP, (self->homeTileX * TILE_SIZE) + HALF_TILE_SIZE, ((self->homeTileY - 1) * TILE_SIZE) + HALF_TILE_SIZE);

                createdEntity->homeTileX = self->homeTileX;
                createdEntity->homeTileY = self->homeTileY;

                createdEntity->jumpPower = aboveTile - TILE_WARP_0;
                self->jumpPower = TILE_CONTAINER_2;
                break;
            }
            default:
            {
                break;
            }

        }

        if(self->jumpPower == TILE_BRICK_BLOCK && (self->yspeed > 0 || self->yDamping == 1) && createdEntity == NULL ) {
            self->jumpPower = TILE_EMPTY;
            self->gameData->score += 10;
        }

        self->tilemap->map[self->homeTileY * self->tilemap->mapWidth + self->homeTileX] = self->jumpPower;

        destroyEntity(self, false);
    }
};

void moveEntityWithTileCollisions(entity_t *self)
{

    uint16_t newX = self->x;
    uint16_t newY = self->y;
    uint8_t tx = TO_TILE_COORDS(self->x >> SUBPIXEL_RESOLUTION);
    uint8_t ty = TO_TILE_COORDS(self->y >> SUBPIXEL_RESOLUTION);
    // bool collision = false;

    // Are we inside a block? Push self out of block
    uint8_t t = getTile(self->tilemap, tx, ty);

    if (isSolid(t))
    {

        if (self->xspeed == 0 && self->yspeed == 0)
        {
            newX += (self->spriteFlipHorizontal) ? 16 : -16;
        }
        else
        {
            if (self->yspeed != 0)
            {
                self->yspeed = -self->yspeed;
            }
            else
            {
                self->xspeed = -self->xspeed;
            }
        }
    }
    else
    {

        if (self->yspeed != 0)
        {
            int16_t hcof = (((self->x >> SUBPIXEL_RESOLUTION) % TILE_SIZE) - HALF_TILE_SIZE);

            // Handle halfway though tile
            uint8_t at = getTile(self->tilemap, tx + SIGNOF(hcof), ty);

            if (isSolid(at))
            {
                // collision = true;
                newX = ((tx + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
            }

            uint8_t newTy = TO_TILE_COORDS(((self->y + self->yspeed) >> SUBPIXEL_RESOLUTION) + SIGNOF(self->yspeed) * HALF_TILE_SIZE);

            if (newTy != ty)
            {
                uint8_t newVerticalTile = getTile(self->tilemap, tx, newTy);

                if (newVerticalTile > TILE_UNUSED_29 && newVerticalTile < TILE_BG_GOAL_ZONE)
                {
                    if (self->tileCollisionHandler(self, newVerticalTile, tx, newTy, 2 << (self->yspeed > 0)))
                    {
                        newY = ((newTy + ((ty < newTy) ? -1 : 1)) * TILE_SIZE + HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
                    }
                }
            }
        }

        if (self->xspeed != 0)
        {
            int16_t vcof = (((self->y >> SUBPIXEL_RESOLUTION) % TILE_SIZE) - HALF_TILE_SIZE);

            // Handle halfway though tile
            uint8_t att = getTile(self->tilemap, tx, ty + SIGNOF(vcof));

            if (isSolid(att))
            {
                // collision = true;
                newY = ((ty + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
            }

            // Handle outside of tile
            uint8_t newTx = TO_TILE_COORDS(((self->x + self->xspeed) >> SUBPIXEL_RESOLUTION) + SIGNOF(self->xspeed) * HALF_TILE_SIZE);

            if (newTx != tx)
            {
                uint8_t newHorizontalTile = getTile(self->tilemap, newTx, ty);

                if (newHorizontalTile > TILE_UNUSED_29 && newHorizontalTile < TILE_BG_GOAL_ZONE)
                {
                    if (self->tileCollisionHandler(self, newHorizontalTile, newTx, ty, (self->xspeed > 0)))
                    {
                        newX = ((newTx + ((tx < newTx) ? -1 : 1)) * TILE_SIZE + HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
                    }
                }

                if (!self->falling)
                {
                    uint8_t newBelowTile = getTile(self->tilemap, tx, ty + 1);

                    if (!isSolid(newBelowTile))
                    {
                        self->falling = true;
                    }
                }
            }
        }
    }

    self->x = newX + self->xspeed;
    self->y = newY + self->yspeed;
}

void applyDamping(entity_t *self)
{
    if (!self->falling)
    {
        if (self->xspeed > 0)
        {
            self->xspeed -= self->xDamping;

            if (self->xspeed < 0)
            {
                self->xspeed = 0;
            }
        }
        else if (self->xspeed < 0)
        {
            self->xspeed += self->xDamping;

            if (self->xspeed > 0)
            {
                self->xspeed = 0;
            }
        }
    }

    if (self->gravityEnabled)
    {
        return;
    }

    if (self->yspeed > 0)
    {
        self->yspeed -= self->yDamping;

        if (self->yspeed < 0)
        {
            self->yspeed = 0;
        }
    }
    else if (self->yspeed < 0)
    {
        self->yspeed += self->yDamping;

        if (self->yspeed > 0)
        {
            self->yspeed = 0;
        }
    }
}

void applyGravity(entity_t *self)
{
    if (!self->gravityEnabled || !self->falling)
    {
        return;
    }

    self->yspeed += self->gravity;

    if (self->yspeed > self->yMaxSpeed)
    {
        self->yspeed = self->yMaxSpeed;
    }
}

void despawnWhenOffscreen(entity_t *self)
{
    if (
        (self->x >> SUBPIXEL_RESOLUTION) < (self->tilemap->mapOffsetX - DESPAWN_THRESHOLD) ||
        (self->x >> SUBPIXEL_RESOLUTION) > (self->tilemap->mapOffsetX + TILEMAP_DISPLAY_WIDTH_PIXELS + DESPAWN_THRESHOLD) ||
        (self->y >> SUBPIXEL_RESOLUTION) < (self->tilemap->mapOffsetY - DESPAWN_THRESHOLD) ||
        (self->y >> SUBPIXEL_RESOLUTION) > (self->tilemap->mapOffsetY + TILEMAP_DISPLAY_HEIGHT_PIXELS + DESPAWN_THRESHOLD))
    {
        destroyEntity(self, true);
    }
}

void destroyEntity(entity_t *self, bool respawn)
{
    if (respawn && !(self->homeTileX == 0 && self->homeTileY == 0))
    {
        self->tilemap->map[self->homeTileY * self->tilemap->mapWidth + self->homeTileX] = self->type + 128;
    }

    // self->entityManager->activeEntities--;
    self->active = false;
}

void animatePlayer(entity_t *self)
{
    if (self->spriteIndex == SP_PLAYER_WIN || self->spriteIndex == SP_PLAYER_HURT)
    {
        // Win pose has been set; don't change it!
        return;
    }

    if (self->falling)
    {
        if (self->yspeed < 0)
        {
            // Jumping
            self->spriteIndex = SP_PLAYER_JUMP;
        }
        else
        {
            // Falling
            self->spriteIndex = SP_PLAYER_WALK1;
        }
    }
    else if (self->xspeed != 0)
    {
        if (self->gameData->btnState & LEFT || self->gameData->btnState & RIGHT)
        {
            // Running
            self->spriteFlipHorizontal = (self->xspeed > 0) ? 0 : 1;
            self->spriteIndex = 1 + ((self->spriteIndex + 1) % 3);
        }
        else
        {
            self->spriteIndex = SP_PLAYER_SLIDE;
        }
    }
    else
    {
        // Standing
        self->spriteIndex = SP_PLAYER_IDLE;
    }
}

void detectEntityCollisions(entity_t *self)
{
    for (uint8_t i = 0; i < MAX_ENTITIES; i++)
    {
        entity_t *checkEntity = &(self->entityManager->entities[i]);
        if (checkEntity->active && checkEntity != self)
        {
            uint32_t dist = abs(self->x - checkEntity->x) + abs(self->y - checkEntity->y);

            if (dist < 200)
            {
                self->collisionHandler(self, checkEntity);
            }
        }
    }
}

void playerCollisionHandler(entity_t *self, entity_t *other)
{
    switch (other->type)
    {
        case ENTITY_TEST:
        case ENTITY_DUST_BUNNY:
        case ENTITY_WASP:
        {
            other->xspeed = -other->xspeed;

            if (/*self->y < other->y &&*/ self->yspeed > 0)
            {
                self->gameData->score += 100;
                other->homeTileX = 0;
                other->homeTileY = 0;
                other->falling = true;
                other->type = ENTITY_DEAD;
                other->spriteFlipVertical = true;
                other->updateFunction = &updateEntityDead;

                buzzer_play_sfx(&sndHit);

                self->yspeed = -180;

                if (self->gameData->btnState & BTN_B)
                {
                    self->jumpPower = 180 + (abs(self->xspeed) >> 2);
                }
            }
            else if(self->invincibilityFrames <= 0)
            {
                self->hp--;
                updateLedsHpMeter(self->entityManager, self->gameData);
                if(self->hp <= 0){
                    self->updateFunction = &updateEntityDead;
                    self->type = ENTITY_DEAD;
                    self->xspeed = 0;
                    self->yspeed = -180;
                    self->spriteIndex = SP_PLAYER_HURT;
                    self->gameData->changeState = ST_DEAD;
                } else {
                    self->xspeed = 0;
                    self->yspeed = 0;
                    self->invincibilityFrames = 40;
                }
            }

            self->falling = true;
            break;
        }
        case ENTITY_WARP:{
            //Execute warp
            self->x = (self->tilemap->warps[other->jumpPower].x * TILE_SIZE + HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
            self->y = (self->tilemap->warps[other->jumpPower].y * TILE_SIZE + HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
            self->falling = true;
            viewFollowEntity(self->tilemap, self->entityManager->playerEntity);

            unlockScrolling(self->tilemap);
            deactivateAllEntities(self->entityManager, true);
            self->tilemap->executeTileSpawnAll = true;

            break;
        }
        case ENTITY_POWERUP:{
            self->hp++;
            if(self->hp > 3){
                self->hp = 3;
            }
            self->gameData->score += 1000;
            buzzer_play_sfx(&sndPowerUp);
            updateLedsHpMeter(self->entityManager, self->gameData);
            destroyEntity(other, false);
            break;
        }
        default:
        {
            break;
        }
    }
}

void enemyCollisionHandler(entity_t *self, entity_t *other)
{
    switch (other->type)
    {
    case ENTITY_TEST:
        self->xspeed = -self->xspeed;
        break;
    case ENTITY_DUST_BUNNY:
        self->xspeed = -self->xspeed;
        break;
    default:
    {
        break;
    }
    }
}

void dummyCollisionHandler(entity_t *self, entity_t *other)
{
    return;
}

bool playerTileCollisionHandler(entity_t *self, uint8_t tileId, uint8_t tx, uint8_t ty, uint8_t direction)
{
    switch (tileId)
    {
    case TILE_CONTAINER_1:
    case TILE_BRICK_BLOCK:
    case TILE_INVISIBLE_CONTAINER:
    {
        entity_t *hitBlock = createEntity(self->entityManager, ENTITY_HIT_BLOCK, (tx * TILE_SIZE) + HALF_TILE_SIZE, (ty * TILE_SIZE) + HALF_TILE_SIZE);

        if (hitBlock != NULL)
        {

            setTile(self->tilemap, tx, ty, TILE_INVISIBLE_BLOCK);
            hitBlock->homeTileX = tx;
            hitBlock->homeTileY = ty;
            hitBlock->jumpPower = tileId;
            if (tileId == TILE_BRICK_BLOCK)
            {
                hitBlock->spriteIndex = SP_HITBLOCK_BRICKS;
                 if(abs(self->xspeed) > 131){ 
                    hitBlock->yDamping = 1;
                }
            }

            switch (direction)
            {
            case 0:
                hitBlock->xspeed = -64;
                break;
            case 1:
                hitBlock->xspeed = 64;
                break;
            case 2:
                hitBlock->yspeed = -64;
                break;
            case 4:
                hitBlock->yspeed = (tileId == TILE_BRICK_BLOCK) ? 32 : 64;
                break;
            default:
                break;
            }
        }
        break;
    }
    case TILE_GOAL_100PTS:
    {
        self->gameData->score += 100;
        self->spriteIndex = SP_PLAYER_WIN;
        self->updateFunction = &updateDummy;
        self->gameData->changeState = ST_LEVEL_CLEAR;
        break;
    }
    case TILE_GOAL_500PTS:
    {
        self->gameData->score += 200;
        self->spriteIndex = SP_PLAYER_WIN;
        self->updateFunction = &updateDummy;
        self->gameData->changeState = ST_LEVEL_CLEAR;
        break;
    }
    case TILE_GOAL_1000PTS:
    {
        self->gameData->score += 1000;
        self->spriteIndex = SP_PLAYER_WIN;
        self->updateFunction = &updateDummy;
        self->gameData->changeState = ST_LEVEL_CLEAR;
        break;
    }
    case TILE_GOAL_2000PTS:
    {
        self->gameData->score += 2000;
        self->spriteIndex = SP_PLAYER_WIN;
        self->updateFunction = &updateDummy;
        self->gameData->changeState = ST_LEVEL_CLEAR;
        break;
    }
    case TILE_GOAL_5000PTS:
    {
        self->gameData->score += 5000;
        self->spriteIndex = SP_PLAYER_WIN;
        self->updateFunction = &updateDummy;
        self->gameData->changeState = ST_LEVEL_CLEAR;
        break;
    }
    case TILE_COIN_1 ... TILE_COIN_3:
    {
        setTile(self->tilemap, tx, ty, TILE_EMPTY);
        self->gameData->coins++;
        self->gameData->score += 50;
        buzzer_play_sfx(&sndCoin);
        break;
    }
    default:
    {
        break;
    }
    }

    if (isSolid(tileId))
    {
        switch (direction)
        {
        case 0: // LEFT
            self->xspeed = 0;
            break;
        case 1: // RIGHT
            self->xspeed = 0;
            break;
        case 2: // UP
            self->yspeed = 0;
            break;
        case 4: // DOWN
            // Landed on platform
            self->falling = false;
            self->yspeed = 0;
            break;
        default: // Should never hit
            return false;
        }
        // trigger tile collision resolution
        return true;
    }

    return false;
}

bool enemyTileCollisionHandler(entity_t *self, uint8_t tileId, uint8_t tx, uint8_t ty, uint8_t direction)
{
    if (isSolid(tileId))
    {
        switch (direction)
        {
        case 0: // LEFT
            self->xspeed = -self->xspeed;
            break;
        case 1: // RIGHT
            self->xspeed = -self->xspeed;
            break;
        case 2: // UP
            self->yspeed = 0;
            break;
        case 4: // DOWN
            // Landed on platform
            self->falling = false;
            self->yspeed = 0;
            break;
        default: // Should never hit
            return false;
        }
        // trigger tile collision resolution
        return true;
    }

    return false;
}

bool dummyTileCollisionHandler(entity_t *self, uint8_t tileId, uint8_t tx, uint8_t ty, uint8_t direction)
{
    return false;
}

void dieWhenFallingOffScreen(entity_t *self)
{
    uint16_t deathBoundary = (self->tilemap->mapOffsetY + TILEMAP_DISPLAY_HEIGHT_PIXELS + DESPAWN_THRESHOLD);
    if (
        ((self->y >> SUBPIXEL_RESOLUTION) > deathBoundary) &&
        ((self->y >> SUBPIXEL_RESOLUTION) < deathBoundary + DESPAWN_THRESHOLD))
    {
        self->hp = 0;
        updateLedsHpMeter(self->entityManager, self->gameData);
        self->gameData->changeState = ST_DEAD;
        destroyEntity(self, true);
    }
}

void updateDummy(entity_t *self)
{
    // Do nothing, because that's what dummies do!
}

void updateScrollLockLeft(entity_t *self)
{
    self->tilemap->minMapOffsetX = (self->x >> SUBPIXEL_RESOLUTION) - 8;
    viewFollowEntity(self->entityManager->tilemap, self->entityManager->viewEntity);
    destroyEntity(self, true);
}

void updateScrollLockRight(entity_t *self)
{
    self->tilemap->maxMapOffsetX = (self->x >> SUBPIXEL_RESOLUTION) + 8 - TILEMAP_DISPLAY_WIDTH_PIXELS;
    viewFollowEntity(self->entityManager->tilemap, self->entityManager->viewEntity);
    destroyEntity(self, true);
}

void updateScrollLockUp(entity_t *self)
{
    self->tilemap->minMapOffsetY = (self->y >> SUBPIXEL_RESOLUTION) - 8;
    viewFollowEntity(self->entityManager->tilemap, self->entityManager->viewEntity);
    destroyEntity(self, true);
}

void updateScrollLockDown(entity_t *self)
{
    self->tilemap->maxMapOffsetY = (self->y >> SUBPIXEL_RESOLUTION) + 8 - TILEMAP_DISPLAY_HEIGHT_PIXELS;
    viewFollowEntity(self->entityManager->tilemap, self->entityManager->viewEntity);
    destroyEntity(self, true);
}

void updateScrollUnlock(entity_t *self)
{
    unlockScrolling(self->tilemap);
    viewFollowEntity(self->entityManager->tilemap, self->entityManager->viewEntity);
    destroyEntity(self, true);
}

void updateEntityDead(entity_t *self)
{
    applyGravity(self);
    self->x += self->xspeed;
    self->y += self->yspeed;

    despawnWhenOffscreen(self);
}

void updatePowerUp(entity_t *self)
{
    self->spriteIndex = SP_GAMING_1 + ((self->spriteIndex + 1) % 3);
    despawnWhenOffscreen(self);
}

void updateWarp(entity_t *self)
{
    self->spriteIndex = SP_WARP_1 + ((self->spriteIndex + 1) % 3);

    //Destroy self and respawn warp container block when offscreen
    if (
        (self->x >> SUBPIXEL_RESOLUTION) < (self->tilemap->mapOffsetX - DESPAWN_THRESHOLD) ||
        (self->x >> SUBPIXEL_RESOLUTION) > (self->tilemap->mapOffsetX + TILEMAP_DISPLAY_WIDTH_PIXELS + DESPAWN_THRESHOLD) ||
        (self->y >> SUBPIXEL_RESOLUTION) < (self->tilemap->mapOffsetY - DESPAWN_THRESHOLD) ||
        (self->y >> SUBPIXEL_RESOLUTION) > (self->tilemap->mapOffsetY + TILEMAP_DISPLAY_HEIGHT_PIXELS + DESPAWN_THRESHOLD))
    {
        //In destroyEntity, this will overflow to the correct value.
        self->type = 128 + TILE_CONTAINER_1;

        destroyEntity(self, true);
    }
}

void updateDustBunny(entity_t *self)
{
    if(!self->falling){
        self->yDamping--;
        if(self->yDamping <= 0){
            bool directionToPlayer = (self->entityManager->playerEntity->x < self->x);
            
            switch(self->xDamping){
                case 0: {
                    self->xspeed = (1 + esp_random() % 4) * 16 * ((directionToPlayer)?-1:1);
                    self->yspeed = (1 + esp_random() % 4) * -64;
                    self->xDamping = 1;
                    self->yDamping = (1 + esp_random() % 3) * 10;
                    self->spriteIndex = SP_DUSTBUNNY_JUMP;
                    self->spriteFlipHorizontal = !directionToPlayer;
                    break;
                }
                case 1: {
                    self->xDamping = 0;
                    self->yDamping = 10;
                    self->spriteIndex = SP_DUSTBUNNY_CHARGE;
                    self->spriteFlipHorizontal = !directionToPlayer;
                    break;
                }
                default:
                    self->xDamping = 0;
                    break;
            }
        }
    }
    
    despawnWhenOffscreen(self);
    moveEntityWithTileCollisions(self);
    applyGravity(self);
    detectEntityCollisions(self);
};

bool dustBunnyTileCollisionHandler(entity_t *self, uint8_t tileId, uint8_t tx, uint8_t ty, uint8_t direction){
    if (isSolid(tileId))
    {
        switch (direction)
        {
        case 0: // LEFT
            self->xspeed = -self->xspeed;
            break;
        case 1: // RIGHT
            self->xspeed = -self->xspeed;
            break;
        case 2: // UP
            self->yspeed = 0;
            break;
        case 4: // DOWN
            // Landed on platform
            self->falling = false;
            self->yspeed = 0;
            self->xspeed = 0;
            self->spriteIndex = SP_DUSTBUNNY_IDLE;
            break;
        default: // Should never hit
            return false;
        }
        // trigger tile collision resolution
        return true;
    }

    return false;
};

void updateWasp(entity_t *self)
{
    switch(self->xDamping){
        case 0:
            self->spriteIndex = SP_WASP_1 + ((self->spriteIndex + 1) % 2);
            self->yDamping--;

            if(self->yDamping < 0 && abs(self->x - self->entityManager->playerEntity->x) < 512) {
                self->xDamping = 1;
                self->gravityEnabled = true;
                self->falling = true;
                self->spriteIndex = SP_WASP_DIVE;
                self->xspeed = 0;
                self->yspeed = 128;
            }
            break;
        case 1:
            if(!self->falling) {
                self->yDamping--;
                if(self->yDamping < 0){
                    self->xDamping = 2;
                    self->gravityEnabled = false;
                    self->falling = false;
                    self->yspeed = -64;
                }
            }
            break;
        case 2:
            self->spriteIndex = SP_WASP_1 + ((self->spriteIndex + 1) % 2);
            if(self->y <= ((self->homeTileY * TILE_SIZE) << SUBPIXEL_RESOLUTION )) {
                self->xDamping = 0;
                self->xspeed = (self->spriteFlipHorizontal)? -32 : 32;
                self->yspeed = 0;
                self->yDamping = (1 + esp_random() % 2) * 20;
            }
        default:
            break;
    }
    
    despawnWhenOffscreen(self);
    moveEntityWithTileCollisions(self);
    applyGravity(self);
    detectEntityCollisions(self);
};

bool waspTileCollisionHandler(entity_t *self, uint8_t tileId, uint8_t tx, uint8_t ty, uint8_t direction){
    if (isSolid(tileId))
    {
        switch (direction)
        {
        case 0: // LEFT
        case 1: // RIGHT
            self->spriteFlipHorizontal = !self->spriteFlipHorizontal;
            self->xspeed = -self->xspeed;
            break;
        case 2: // UP
            self->yspeed = 0;
            break;
        case 4: // DOWN
            // Landed on platform
            self->falling = false;
            self->yspeed = 0;
            self->xspeed = 0;
            self->xDamping = 1;
            self->yDamping = 40;
            
            break;
        default: // Should never hit
            return false;
        }
        // trigger tile collision resolution
        return true;
    }

    return false;
};