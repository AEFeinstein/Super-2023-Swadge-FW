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

//==============================================================================
// Constants
//==============================================================================
#define SUBPIXEL_RESOLUTION 4
#define TILE_SIZE_IN_POWERS_OF_2 4
#define TILE_SIZE 16
#define HALF_TILE_SIZE 8
#define TILEMAP_DISPLAY_WIDTH_PIXELS 240   //The screen size
#define TILEMAP_DISPLAY_HEIGHT_PIXELS 240  //The screen size
#define DESPAWN_THRESHOLD 64

#define SIGNOF(x) ((x > 0) - (x < 0))
#define TO_TILE_COORDS(x) (x >> TILE_SIZE_IN_POWERS_OF_2)
#define TO_PIXEL_COORDS(x) (x >> SUBPIXEL_RESOLUTION)
#define TO_SUBPIXEL_COORDS(x) (x << SUBPIXEL_RESOLUTION)

static const song_t sndHit =
{
    .notes =
    {
        {740, 10},{840, 10},{940, 10}
    },
    .numNotes = 3,
    .shouldLoop = false
};

static const song_t sndCoin =
{
    .notes =
    {
        {1000, 50},{1200, 100}
    },
    .numNotes = 2,
    .shouldLoop = false
};

//==============================================================================
// Functions
//==============================================================================
void initializeEntity(entity_t * self, entityManager_t * entityManager, tilemap_t * tilemap, gameData_t * gameData){
    self->active = false;
    self->tilemap = tilemap;
    self->gameData = gameData;
    self->homeTileX = 0;
    self->homeTileY = 0;
    self->gravity = false;
    self->falling = false;
    self->entityManager = entityManager;
};

void updatePlayer(entity_t * self) {
    if(self->gameData->btnState & BTN_A){
        self->xMaxSpeed = 132;
    } else {
        self->xMaxSpeed = 72;
    }

    if(self->gameData->btnState & LEFT){
        self->xspeed -= (self->falling)? 8 : 12;

        if(self->xspeed < -self->xMaxSpeed){
            self->xspeed = -self->xMaxSpeed;
        }

    } else if(self->gameData->btnState & RIGHT){
        self->xspeed += (self->falling)? 8 : 12;

        if(self->xspeed > self->xMaxSpeed){
            self->xspeed = self->xMaxSpeed;
        }
    }


    if(self->gameData->btnState & UP){
        self->yspeed -= 16;

         if(self->yspeed < -self->yMaxSpeed){
            self->yspeed = -self->yMaxSpeed;
        }

    } else if(self->gameData->btnState & DOWN){
        self->yspeed += 16;

        if(self->yspeed > self->yMaxSpeed){
            self->yspeed = self->yMaxSpeed;
        }
    }

    if(self->gameData->btnState & BTN_B){
        if(!self->falling && !(self->gameData->prevBtnState & BTN_B) ) {
            //initiate jump
            self->jumpPower = 180 + (abs(self->xspeed) >> 2);
            self->yspeed = -self->jumpPower;
            self->falling = true;
        } else if (self->jumpPower > 0 && self->yspeed < 0) {
            //jump dampening
            self->jumpPower -= 16; //32
            self->yspeed = -self->jumpPower;

            if(self->jumpPower < 0) {
                self->jumpPower = 0;
            }
        }
    } else if (self->falling && self->jumpPower > 0 && self->yspeed < 0) {
        //Cut jump short if player lets go of jump button
        self->jumpPower = 0;
        self->yspeed=self->yspeed >> 2; //technically shouldn't do this with a signed int
    }

    moveEntityWithTileCollisions(self);
    applyGravity(self);
    applyDamping(self);
    detectEntityCollisions(self);
    animatePlayer(self);
};

void updateTestObject(entity_t * self) {
    self->spriteFlipHorizontal = !self->spriteFlipHorizontal;

    despawnWhenOffscreen(self);
    moveEntityWithTileCollisions(self);
    applyGravity(self);
    detectEntityCollisions(self);
};

void updateHitBlock(entity_t * self) {
    self->x += self->xspeed;
    self->y += self->yspeed;

    self->animationTimer++;
    if(self->animationTimer == 2){
        self->xspeed = -self->xspeed;
        self->yspeed = -self->yspeed;
    }
    if(self->animationTimer > 4){
        self->tilemap->map[self->homeTileY * self->tilemap->mapWidth + self->homeTileX] = self->jumpPower;
        destroyEntity(self, false);
    }

};

void moveEntityWithTileCollisions(entity_t * self){
    
    uint16_t newX = self->x;
    uint16_t newY = self->y;
    uint8_t tx = TO_TILE_COORDS(self->x >> SUBPIXEL_RESOLUTION);
    uint8_t ty = TO_TILE_COORDS(self->y >> SUBPIXEL_RESOLUTION);
    bool collision = false;

    //Are we inside a block? Push self out of block
    uint8_t t = getTile(self->tilemap, tx, ty);
 
    if(isSolid(t)){

        if(self->xspeed == 0 && self->yspeed ==0){
            newX += (self->spriteFlipHorizontal) ? 16: -16;
        } else {
            if(self->yspeed != 0) {
                self->yspeed = -self->yspeed;
            } else {
                self->xspeed = -self->xspeed;
            }
        }

    } else {

        if(self->yspeed != 0) {
            int16_t hcof = ( ((self->x >> SUBPIXEL_RESOLUTION) % TILE_SIZE) - HALF_TILE_SIZE);

            //Handle halfway though tile
            uint8_t at = getTile(self->tilemap, tx + SIGNOF(hcof), ty);

            if(isSolid(at)) {
                collision = true;
                newX=((tx + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
            }

            uint8_t newTy = TO_TILE_COORDS(((self->y + self->yspeed) >> SUBPIXEL_RESOLUTION) + SIGNOF(self->yspeed) * HALF_TILE_SIZE);

            if(newTy != ty) {
                uint8_t newVerticalTile = getTile(self->tilemap, tx, newTy);

                if(newVerticalTile > TILE_INVISIBLE_BLOCK && newVerticalTile < TILE_BG_GOAL_ZONE){
                    if(self->tileCollisionHandler(self, newVerticalTile, tx, newTy, 2 << (self->yspeed > 0))){
                        newY=((ty + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
                    }
                }
            }
        }
        
        if(self->xspeed != 0) 
        {
            int16_t vcof = ( ((self->y >> SUBPIXEL_RESOLUTION) % TILE_SIZE) - HALF_TILE_SIZE);

            //Handle halfway though tile
            uint8_t att = getTile(self->tilemap, tx, ty + SIGNOF(vcof));

            if(isSolid(att)) {
                collision = true;
                newY=((ty + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
            }

            //Handle outside of tile
            uint8_t newTx = TO_TILE_COORDS(((self->x + self->xspeed) >> SUBPIXEL_RESOLUTION) + SIGNOF(self->xspeed) * HALF_TILE_SIZE);

            if(newTx != tx) {
                uint8_t newHorizontalTile = getTile(self->tilemap, newTx, ty);

                if(newHorizontalTile > TILE_INVISIBLE_BLOCK && newHorizontalTile < TILE_BG_GOAL_ZONE){
                    if(self->tileCollisionHandler(self, newHorizontalTile, newTx, ty, (self->xspeed > 0))){
                        newX=((tx + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
                    }
                }

                if(!self->falling) {
                    uint8_t newBelowTile=getTile(self->tilemap, tx, ty + 1);

                    if(!isSolid(newBelowTile)){
                        self->falling = true;
                    }
                }
            }
        }

    }

    self->x = newX+self->xspeed;
    self->y = newY+self->yspeed;
}

void applyDamping(entity_t *self){
    if(!self->falling) {
        if(self->xspeed > 0) {
            self->xspeed -= self->xDamping;
            
            if(self->xspeed < 0) {
                self->xspeed = 0;
            }
        } else if(self->xspeed < 0) {
            self->xspeed += self->xDamping;
            
            if(self->xspeed > 0) {
                self->xspeed = 0;
            }
        }
    }

    if(self->gravityEnabled){
        return;
    }

    if(self->yspeed > 0) {
        self->yspeed -= self->yDamping;
        
        if(self->yspeed < 0) {
            self->yspeed = 0;
        }
    } else if(self->yspeed < 0) {
        self->yspeed += self->yDamping;
        
        if(self->yspeed > 0) {
            self->yspeed = 0;
        }
    }
}

void applyGravity(entity_t *self){
    if(!self->gravityEnabled || !self->falling) {
        return;
    }

    self->yspeed += self->gravity;

    if(self->yspeed > self->yMaxSpeed){
        self->yspeed = self->yMaxSpeed;
    }
}

void despawnWhenOffscreen(entity_t *self){
    if(
        (self->x >> SUBPIXEL_RESOLUTION) < (self->tilemap->mapOffsetX - DESPAWN_THRESHOLD) || 
        (self->x >> SUBPIXEL_RESOLUTION) > (self->tilemap->mapOffsetX + TILEMAP_DISPLAY_WIDTH_PIXELS + DESPAWN_THRESHOLD) || 
        (self->y >> SUBPIXEL_RESOLUTION) < (self->tilemap->mapOffsetY - DESPAWN_THRESHOLD) || 
        (self->y >> SUBPIXEL_RESOLUTION) > (self->tilemap->mapOffsetY + TILEMAP_DISPLAY_HEIGHT_PIXELS + DESPAWN_THRESHOLD)
    ) {
        destroyEntity(self, true);
    }
}

void destroyEntity(entity_t *self, bool respawn) {
    if(respawn && !(self->homeTileX == 0 && self->homeTileY == 0)){
        self->tilemap->map[self->homeTileY * self->tilemap->mapWidth + self->homeTileX] = self->type << 7;
    }
    
    //self->entityManager->activeEntities--;
    self->active = false;
}

void animatePlayer(entity_t * self){
    if(self->falling){
        if(self->yspeed < 0) {
            //Jumping
            self->spriteIndex = 4;
        } else {
            //Falling
            self->spriteIndex = 1;

        }
    } else if(self->xspeed != 0) {
        //Running
        self->spriteFlipHorizontal=(self->xspeed > 0)? 0 : 1;
        self->spriteIndex = 1+((self->spriteIndex + 1) % 3);
    } else {
        //Standing
        self->spriteIndex = 0;
    }
}

void detectEntityCollisions(entity_t *self){
    for(uint8_t i=0; i < MAX_ENTITIES; i++)
    {
        entity_t *checkEntity = &(self->entityManager->entities[i]);
        if(checkEntity->active && checkEntity != self)
        {
            uint32_t dist = abs(self->x - checkEntity->x) + abs(self->y - checkEntity->y);
            
            if(dist < 256) {
                self->collisionHandler(self, checkEntity);
            }
        }
    }
}

void playerCollisionHandler(entity_t *self, entity_t *other){
    switch(other->type) {
        case ENTITY_TEST:
                self->yspeed = -64;
                self->falling = true;

                other->xspeed = -other->xspeed;
                break;
        default:
            ;
    }
}

void enemyCollisionHandler(entity_t *self, entity_t *other){
    switch(other->type) {
        case ENTITY_TEST:
                self->xspeed = -self->xspeed;
                break;
        default:
            ;
    }
}

void dummyCollisionHandler(entity_t *self, entity_t *other){
    return;
}

bool playerTileCollisionHandler(entity_t *self, uint8_t tileId, uint8_t tx, uint8_t ty, uint8_t direction){
     switch(tileId){
        case TILE_CONTAINER_1 ... TILE_CONTAINER_3: ;
            entity_t* hitBlock = createEntity(self->entityManager, ENTITY_HIT_BLOCK, (tx * TILE_SIZE) + HALF_TILE_SIZE, (ty * TILE_SIZE) + HALF_TILE_SIZE);
           
            if(hitBlock != NULL){
                
                setTile(self->tilemap, tx, ty, TILE_INVISIBLE_BLOCK);
                hitBlock->homeTileX = tx;
                hitBlock->homeTileY = ty;
                hitBlock->jumpPower = tileId;

                switch(direction){
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
                        hitBlock->yspeed = 64;
                        break;
                    default:
                        break;
                }
            }
            break;
        case TILE_COIN_1 ... TILE_COIN_3:
            setTile(self->tilemap, tx, ty, TILE_EMPTY);
            self->gameData->coins++;
            self->gameData->score+=50;
            buzzer_play(&sndCoin);
            break;
        default:
            break;
     }
     
     if(isSolid(tileId)) {
        switch(direction){
            case 0: //LEFT
                self->xspeed = 0;
                break;
            case 1: //RIGHT
                self->xspeed = 0;
                break;
            case 2: //UP
                self->yspeed = 0;
                break;
            case 4: //DOWN
                //Landed on platform
                self->falling = false;
                self->yspeed = 0;
                break;
            default: //Should never hit
                return false;
        }
        //trigger tile collision resolution
        return true;
    }

    return false;
}

bool enemyTileCollisionHandler(entity_t *self, uint8_t tileId, uint8_t tx, uint8_t ty, uint8_t direction){
    if(isSolid(tileId)) {
        switch(direction){
            case 0: //LEFT
                self->xspeed = -self->xspeed;
                break;
            case 1: //RIGHT
                self->xspeed = -self->xspeed;
                break;
            case 2: //UP
                self->yspeed = 0;
                break;
            case 4: //DOWN
                //Landed on platform
                self->falling = false;
                self->yspeed = 0;
                break;
            default: //Should never hit
                return false;
        }
        //trigger tile collision resolution
        return true;
    }

    return false;
}

bool dummyTileCollisionHandler(entity_t *self, uint8_t tileId, uint8_t tx, uint8_t ty, uint8_t direction){
    return false;
}
