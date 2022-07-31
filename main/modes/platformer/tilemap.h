#ifndef _TILEMAP_H_
#define _TILEMAP_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "display.h"
#include "common_typedef.h"
#include "entityManager.h"

//==============================================================================
// Enums
//==============================================================================
typedef enum {
    TILE_EMPTY,
    TILE_CTRL_RIGHT,
    TILE_CTRL_LEFT,
    TILE_CTRL_DOWN,
    TILE_CTRL_UP,
    TILE_CTRL_0x5,
    TILE_CTRL_0x6,
    TILE_CTRL_0x7,
    TILE_CTRL_0x8,
    TILE_CTNR_POW1,
    TILE_CTNR_POW2,
    TILE_CTNR_POW3,
    TILE_CTNR_10COIN,
    TILE_CTNR_1UP,
    TILE_CTNR_0xE,
    TILE_INVISIBLE_BLOCK,
    TILE_GRASS,
    TILE_GROUND,
    TILE_BRICK_BLOCK,
    TILE_BLOCK,
    TILE_METAL_BLOCK,
    TILE_METAL_PIPE_H,
    TILE_METAL_PIPE_V,
    TILE_SOLID_0x17,
    TILE_SOLID_0X18,
    TILE_SOLID_0x19,
    TILE_SOLID_0x1A,
    TILE_GOAL_100PTS,
    TILE_GOAL_500PTS,
    TILE_GOAL_1000PTS,
    TILE_GOAL_2000PTS,
    TILE_GOAL_5000PTS,
    TILE_CONTAINER_1,
    TILE_CONTAINER_2,
    TILE_CONTAINER_3,
    TILE_CONTAINER1_1,
    TILE_CONTAINER1_2,
    TILE_CONTAINER1_3,
    TILE_CONTAINER2_1,
    TILE_CONTAINER2_2,
    TILE_CONTAINER2_3,
    TILE_SOLID_ANIMATED_0x29_1,
    TILE_SOLID_ANIMATED_0x29_2,
    TILE_SOLID_ANIMATED_0x29_3,
    TILE_SOLID_ANIMATED_0x2C_1,
    TILE_SOLID_ANIMATED_0x2C_2,
    TILE_SOLID_ANIMATED_0x2C_3,
    TILE_UNUSED_0x2F,
    TILE_COIN_1,
    TILE_COIN_2,
    TILE_COIN_3,
    TILE_LAVA_1,
    TILE_LAVA_2,
    TILE_LAVA_3,
    TILE_WATER1,
    TILE_WATER2,
    TILE_WATER3,
    TILE_LADDER,
    TILE_CHAIN,
    TILE_SPIKES,
    TILE_GOAL_ZONE
} tileIndex_t;

//==============================================================================
// Structs
//==============================================================================

 struct tilemap_t
{
    wsg_t tiles[36];

    uint8_t * map;
    uint8_t mapWidth;
    uint8_t mapHeight;

    int16_t mapOffsetX;
    int16_t mapOffsetY;
    
    int16_t minMapOffsetX;
    int16_t maxMapOffsetX;
    int16_t minMapOffsetY;
    int16_t maxMapOffsetY;

    bool tileSpawnEnabled;
    int16_t executeTileSpawnColumn;
    int16_t executeTileSpawnRow;

    entityManager_t *entityManager;

    uint8_t animationFrame;
    int16_t animationTimer;
};

//==============================================================================
// Prototypes
//==============================================================================
void initializeTileMap(tilemap_t * tilemap);
void drawTileMap(display_t * disp, tilemap_t * tilemap);
void scrollTileMap(tilemap_t * tilemap, int16_t x, int16_t y);
void drawTile(tilemap_t * tilemap, uint8_t tileId, int16_t x, int16_t y);
bool loadMapFromFile(tilemap_t * tilemap, char * name);
bool loadTiles(tilemap_t * tilemap);
void tileSpawnEntity(tilemap_t * tilemap, uint8_t objectIndex, uint8_t tx, uint8_t ty);
uint8_t getTile(tilemap_t *tilemap, uint8_t tx, uint8_t ty);
void setTile(tilemap_t *tilemap, uint8_t tx, uint8_t ty, uint8_t newTileId);
bool isSolid(uint8_t tileId);

#endif
