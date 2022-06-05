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
#define TO_TILE_COORDS(x) (x >> SUBPIXEL_RESOLUTION >> TILE_SIZE_IN_POWERS_OF_2)

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
    uint8_t tx = TO_TILE_COORDS(self->x);
    uint8_t ty = TO_TILE_COORDS((self->y + (15 << 4) * ((self->y >> 4) % TILE_SIZE > 8)));

    if(self->yspeed != 0) {
        uint8_t newTy = TO_TILE_COORDS((self->y + self->yspeed));

        //if(newTx != tx) {
            uint8_t newVerticalTile = self->tilemap->map[(newTy + (self->yspeed > 0)) * self->tilemap->mapWidth + tx];

            if(newVerticalTile > 0) {
                newY=((newTy + (self->yspeed < 0)) << SUBPIXEL_RESOLUTION) * TILE_SIZE;// - HALF_TILE_SIZE;
                self->yspeed = 0;
            }
        //}
    }

    if(self->xspeed != 0) {
        uint8_t newTx = TO_TILE_COORDS((self->x + self->xspeed));

        //if(newTx != tx) {
            uint8_t newHorizontalTile = self->tilemap->map[ty * self->tilemap->mapWidth + newTx + (self->xspeed > 0)];

            if(newHorizontalTile > 0) {
                newX=((newTx + (self->xspeed < 0)) << SUBPIXEL_RESOLUTION) * TILE_SIZE;// - HALF_TILE_SIZE;
                self->xspeed = 0;
            }
        //}
    }

    self->x = newX+self->xspeed;
    self->y = newY+self->yspeed;
};