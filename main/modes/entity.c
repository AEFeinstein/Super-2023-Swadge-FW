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
};

void updateTestObject(entity_t * self) {
    self->spriteIndex = (self->spriteIndex + 1) % 3;

    /*self->xspeed += 1;
    if(self->xspeed > 32) {
        self->xspeed = -32;
    }

    self->yspeed += 1;
    if(self->yspeed > 64) {
        self->yspeed = -64;
    }*/

    if(self->gameData->btnState & LEFT){
        self->xspeed -= 2;
    } else if(self->gameData->btnState & RIGHT){
        self->xspeed += 2;
    }


    if(self->gameData->btnState & UP){
        self->yspeed -= 2;
    } else if(self->gameData->btnState & DOWN){
        self->yspeed += 2;
    }

    int16_t newX = self->x;
    int16_t newY = self->y;
    uint8_t tx = TO_TILE_COORDS(self->x >> SUBPIXEL_RESOLUTION);
    uint8_t ty = TO_TILE_COORDS(self->y >> SUBPIXEL_RESOLUTION);
    bool collision = false;

    if(self->yspeed != 0) {
        int16_t hcof = ( ((self->x >> SUBPIXEL_RESOLUTION) % TILE_SIZE) - HALF_TILE_SIZE);

        //Handle halfway though tile
        uint8_t at = self->tilemap->map[ty * self->tilemap->mapWidth + tx + SIGNOF(hcof)];

        if(at > 0) {
            collision = true;
            newX=((tx + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
        }

        uint8_t newTy = TO_TILE_COORDS(((self->y + self->yspeed) >> SUBPIXEL_RESOLUTION) + SIGNOF(self->yspeed) * HALF_TILE_SIZE);

        if(newTy != ty) {
            uint8_t newVerticalTile = self->tilemap->map[newTy * self->tilemap->mapWidth + tx];

            if(newVerticalTile > 0) {
                collision = true;
                self->yspeed = 0;
                newY=((ty + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
            }
        }
    }

    if(self->xspeed != 0) {
        int16_t vcof = ( ((self->y >> SUBPIXEL_RESOLUTION) % TILE_SIZE) - HALF_TILE_SIZE);

        //Handle halfway though tile
        uint8_t att = self->tilemap->map[(ty + SIGNOF(vcof)) * self->tilemap->mapWidth + tx];

        if(att > 0) {
            collision = true;
            newY=((ty + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
        }

        //Handle outside of tile
        uint8_t newTx = TO_TILE_COORDS(((self->x + self->xspeed) >> SUBPIXEL_RESOLUTION) + SIGNOF(self->xspeed) * HALF_TILE_SIZE);

        if(newTx != tx) {
            uint8_t newHorizontalTile = self->tilemap->map[ty * self->tilemap->mapWidth + newTx];

            if(newHorizontalTile > 0) {
                collision = true;
                self->xspeed = 0;
                newX=((tx + 1) * TILE_SIZE - HALF_TILE_SIZE) << SUBPIXEL_RESOLUTION;
            }
        }
    }

    //Handle literal corner case
    if(!collision && self->xspeed !=0 && self->yspeed != 0){
        uint8_t t = self->tilemap->map[ty * self->tilemap->mapWidth + tx];

        if(t > 0){
            self->yspeed = -self->yspeed;
        }
    }

    //Quick and broken way to the view follow the sprite
    if(self->x > (120 << 4)){
        scrollTileMap(self->tilemap, (self->xspeed >> 4), 0);
    }

    self->x = newX+self->xspeed;
    self->y = newY+self->yspeed;
};