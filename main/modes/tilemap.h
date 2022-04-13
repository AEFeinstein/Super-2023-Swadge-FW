#ifndef _TILEMAP_H_
#define _TILEMAP_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "display.h"

//==============================================================================
// Structs
//==============================================================================

typedef struct 
{
    wsg_t tilemap_buffer;
    wsg_t tiles;

    int16_t tilemapOffsetX;
    int16_t tilemapOffsetY;
} tilemap_t;

//==============================================================================
// Prototypes
//==============================================================================
void initializeTileMap(tilemap_t * tilemap);
void drawTileMap(display_t * disp, tilemap_t * tilemap);
void scrollTileMap(tilemap_t * tilemap, int16_t x, int16_t y);
void updateTileMapColumn(tilemap_t * tilemap, int16_t column);
void updateTileMapRow(tilemap_t * tilemap, int16_t row);
void drawTile(tilemap_t * tilemap, uint8_t tileId, int16_t x, int16_t y);



#endif
