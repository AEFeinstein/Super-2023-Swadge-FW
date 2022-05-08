//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <esp_log.h>
#include <string.h>

#include "tilemap.h"
#include "esp_random.h"

#include "../../components/hdw-spiffs/spiffs_manager.h"

//==============================================================================
// Constants
//==============================================================================
#define WRAP(x,u) (((x < 0) ? x + (u + 1) * ((-(x)) / (u + 1) + 1) : x) % (u + 1))

#define TILEMAP_BUFFER_WIDTH_PIXELS 256
#define TILEMAP_BUFFER_HEIGHT_PIXELS 256
#define TILEMAP_BUFFER_WIDTH_TILES 16
#define TILEMAP_BUFFER_HEIGHT_TILES 16

#define TILE_SIZE 16

//==============================================================================
// Functions
//==============================================================================

void initializeTileMap(tilemap_t * tilemap) 
{
    tilemap->mapOffsetX = 0;
    tilemap->mapOffsetY = 0;
    
    loadTiles(tilemap);

    loadMapFromFile(tilemap, "level_test.bin");
}

void drawTileMap(display_t * disp, tilemap_t * tilemap)
{
    for(uint16_t y = (tilemap->mapOffsetY >> 4); y < (tilemap->mapOffsetY >> 4) + TILEMAP_BUFFER_HEIGHT_TILES; y++)
    {
        for(uint16_t x = (tilemap->mapOffsetX >> 4); x < (tilemap->mapOffsetX >> 4) + TILEMAP_BUFFER_WIDTH_TILES; x++)
        {
            uint8_t tile = tilemap->map[(y * tilemap->mapWidth) + x];
            if(tile > 0)
            {
                drawWsg(disp, &tilemap->tiles[tile], x * TILE_SIZE - tilemap->mapOffsetX, y * TILE_SIZE - tilemap->mapOffsetY);
            }
        }
    }
}

void scrollTileMap(tilemap_t * tilemap, int16_t x, int16_t y) {
    if(x != 0){
        //TODO:
        //Prevent scrolling out of bounds
        tilemap->mapOffsetX += x;
    }

    if(y != 0){
        //TODO:
        //Prevent scrolling out of bounds
        tilemap->mapOffsetY += y;
    }
}

bool loadMapFromFile(tilemap_t * tilemap, char * name)
{
    if(tilemap->map != NULL){
        free(tilemap->map);
    }
    
    uint8_t * buf = NULL;
    size_t sz;
    if(!spiffsReadFile(name, &buf, &sz))
    {
        ESP_LOGE("MAP", "Failed to read %s", name);
        return false;
    }

    uint8_t width = buf[0];
    uint8_t height = buf[1];

    tilemap->map = (uint8_t *)malloc(sizeof(uint8_t) * width * height);
    memcpy(tilemap->map, &buf[2], width * height - 2);

    tilemap->mapWidth = width;
    tilemap->mapHeight = height;

    free(buf);

    return true;
}

bool loadTiles(tilemap_t * tilemap){
    loadWsg("tile000.wsg", &tilemap->tiles[0]);
    loadWsg("tile001.wsg", &tilemap->tiles[1]);
    loadWsg("tile002.wsg", &tilemap->tiles[2]);
    loadWsg("tile003.wsg", &tilemap->tiles[3]);
    loadWsg("tile004.wsg", &tilemap->tiles[4]);
    loadWsg("tile005.wsg", &tilemap->tiles[5]);
}