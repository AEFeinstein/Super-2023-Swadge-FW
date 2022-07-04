//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include "entity.h"
#include "tilemap.h"
#include "gameData.h"
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

//==============================================================================
// Functions
//==============================================================================
void initializeEntity(entity_t * entity, tilemap_t * tilemap, gameData_t * gameData){
    entity->active = false;
    entity->tilemap = tilemap;
    entity->gameData = gameData;
    entity->homeTileX = 0;
    entity->homeTileY = 0;
    entity->gravity = false;
    entity->falling = false;
};

void updatePlayer(entity_t * self) {
    self->spriteIndex = (self->spriteIndex + 1) % 3;

    if(self->gameData->btnState & LEFT){
        self->xspeed -= 16;

        if(self->xspeed < -self->xMaxSpeed){
            self->xspeed = -self->xMaxSpeed;
        }

    } else if(self->gameData->btnState & RIGHT){
        self->xspeed += 16;

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

    if(!self->falling && !(self->gameData->prevBtnState & BTN_A) && (self->gameData->btnState & BTN_A)) {
         self->yspeed = -256;
         self->falling = true;
    }

    moveEntityWithTileCollisions(self);
    applyGravity(self);
    applyDamping(self);
};

void updateTestObject(entity_t * self) {
    self->spriteIndex = (self->spriteIndex + 1) % 3;

    /*
    self->xspeed += 1;
    if(self->xspeed > 32) {
        self->xspeed = -32;
    }

    self->yspeed += 1;
    if(self->yspeed > 64) {
        self->yspeed = -64;
    }
    */
   
    self->x += self->xspeed;
    self->y += self->yspeed;

    despawnWhenOffscreen(self);
    //moveEntityWithTileCollisions(self);
};

void moveEntityWithTileCollisions(entity_t * self){
    
    uint16_t newX = self->x;
    uint16_t newY = self->y;
    uint8_t tx = TO_TILE_COORDS(self->x >> SUBPIXEL_RESOLUTION);
    uint8_t ty = TO_TILE_COORDS(self->y >> SUBPIXEL_RESOLUTION);
    bool collision = false;

    if(self->yspeed != 0) {
        int16_t hcof = ( ((self->x >> SUBPIXEL_RESOLUTION) % TILE_SIZE) - HALF_TILE_SIZE);

        //Handle halfway though tile
        //uint8_t at = self->tilemap->map[ty * self->tilemap->mapWidth + tx + SIGNOF(hcof)];
        uint8_t at = getTile(self->tilemap, tx + SIGNOF(hcof), ty);

        if(at > 0) {
            collision = true;
            newX=((tx + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
        }

        uint8_t newTy = TO_TILE_COORDS(((self->y + self->yspeed) >> SUBPIXEL_RESOLUTION) + SIGNOF(self->yspeed) * HALF_TILE_SIZE);

        if(newTy != ty) {
            //uint8_t newVerticalTile = self->tilemap->map[newTy * self->tilemap->mapWidth + tx];
            uint8_t newVerticalTile = getTile(self->tilemap, tx, newTy);

            if(newVerticalTile > 0) {

                if(self->yspeed > 0) {
                    //Landed on platform
                    self->falling = false;
                }

                collision = true;
                self->yspeed = 0;
                newY=((ty + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
            }
        }
    }

    if(self->xspeed != 0) {
        int16_t vcof = ( ((self->y >> SUBPIXEL_RESOLUTION) % TILE_SIZE) - HALF_TILE_SIZE);

        //Handle halfway though tile
        //uint8_t att = self->tilemap->map[(ty + SIGNOF(vcof)) * self->tilemap->mapWidth + tx];
        uint8_t att = getTile(self->tilemap, tx, ty + SIGNOF(vcof));

        if(att > 0) {
            collision = true;
            newY=((ty + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
        }

        //Handle outside of tile
        uint8_t newTx = TO_TILE_COORDS(((self->x + self->xspeed) >> SUBPIXEL_RESOLUTION) + SIGNOF(self->xspeed) * HALF_TILE_SIZE);

        if(newTx != tx) {
            //uint8_t newHorizontalTile = self->tilemap->map[ty * self->tilemap->mapWidth + newTx];
            uint8_t newHorizontalTile = getTile(self->tilemap, newTx, ty);

            if(newHorizontalTile > 0) {
                collision = true;
                self->xspeed = 0;
                newX=((tx + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
            }

            if(!self->falling) {
                //uint8_t newBelowTile=self->tilemap->map[(ty + 1) * self->tilemap->mapWidth + tx];
                uint8_t newBelowTile=getTile(self->tilemap, tx, ty + 1);

                if(!newBelowTile){
                    self->falling = true;
                }
            }
        }
    }

    //Are we inside a block? Push self out of block
    //uint8_t t = self->tilemap->map[ty * self->tilemap->mapWidth + tx];
    uint8_t t = getTile(self->tilemap, tx, ty);
 
    if(t > 0){
        self->yspeed = -self->yspeed;
        self->xspeed = -self->xspeed;
    }

    self->x = newX+self->xspeed;
    self->y = newY+self->yspeed;
}

void applyDamping(entity_t *self){
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