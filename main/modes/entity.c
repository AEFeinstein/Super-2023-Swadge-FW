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


    self->x += self->xspeed;
    self->y += self->yspeed;
};