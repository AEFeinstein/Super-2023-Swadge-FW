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
#define CLAMP(x,l,u) ((x) < l ? l : ((x) > u ? u : (x)))

#define TILEMAP_DISPLAY_WIDTH_PIXELS 240   //The screen size
#define TILEMAP_DISPLAY_HEIGHT_PIXELS 240  //The screen size
#define TILEMAP_DISPLAY_WIDTH_TILES 16     //The screen size in tiles + 1
#define TILEMAP_DISPLAY_HEIGHT_TILES 16    //The screen size in tiles + 1

#define TILE_SIZE 16
#define TILE_SIZE_IN_POWERS_OF_2 4

//==============================================================================
// Functions
//==============================================================================

void initializeTileMap(tilemap_t * tilemap) 
{
    tilemap->mapOffsetX = 0;
    tilemap->mapOffsetY = 0;

    tilemap->tileSpawnEnabled = false;
    
    loadTiles(tilemap);

    loadMapFromFile(tilemap, "level_test.bin");
}

void drawTileMap(display_t * disp, tilemap_t * tilemap)
{
    for(uint16_t y = (tilemap->mapOffsetY >> TILE_SIZE_IN_POWERS_OF_2); y < (tilemap->mapOffsetY >> TILE_SIZE_IN_POWERS_OF_2) + TILEMAP_DISPLAY_HEIGHT_TILES; y++)
    {
        if(y >= tilemap->mapHeight){
            break;
        }
        
        for(uint16_t x = (tilemap->mapOffsetX >> TILE_SIZE_IN_POWERS_OF_2); x < (tilemap->mapOffsetX >> TILE_SIZE_IN_POWERS_OF_2) + TILEMAP_DISPLAY_WIDTH_TILES; x++)
        {
            uint8_t tile = tilemap->map[(y * tilemap->mapWidth) + x];

            //Test animated tiles
            if(tile == 3 || tile == 7){
                tile += ((tilemap->mapOffsetX >> TILE_SIZE_IN_POWERS_OF_2) % 3);
            }

            //Draw only non-garbage tiles
            if(tile > 0 && tile < 10)
            {
                drawWsg(disp, &tilemap->tiles[tile], x * TILE_SIZE - tilemap->mapOffsetX, y * TILE_SIZE - tilemap->mapOffsetY, false, false, 0);
            } else if(tile > 127 && tilemap->tileSpawnEnabled) {
                tileSpawnEntity(tilemap, tile >> 7, x, y);
            }
        }
    }
}

void scrollTileMap(tilemap_t * tilemap, int16_t x, int16_t y) {
    if(x != 0){
        tilemap->mapOffsetX = CLAMP(tilemap->mapOffsetX + x, tilemap->minMapOffsetX, tilemap->maxMapOffsetX);
    }

    if(y != 0){
        tilemap->mapOffsetY = CLAMP(tilemap->mapOffsetY + y, tilemap->minMapOffsetY, tilemap->maxMapOffsetY);
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

    tilemap->minMapOffsetX = 0;
    tilemap->maxMapOffsetX = width * TILE_SIZE - TILEMAP_DISPLAY_WIDTH_PIXELS;
    
    tilemap->minMapOffsetY = 0;
    tilemap->maxMapOffsetY = height * TILE_SIZE - TILEMAP_DISPLAY_HEIGHT_PIXELS;

    free(buf);

    return true;
}

bool loadTiles(tilemap_t * tilemap){
    loadWsg("tile001.wsg", &tilemap->tiles[1]);
    loadWsg("tile002.wsg", &tilemap->tiles[2]);
    loadWsg("tile003.wsg", &tilemap->tiles[3]);
    loadWsg("tile004.wsg", &tilemap->tiles[4]);
    loadWsg("tile005.wsg", &tilemap->tiles[5]);
    loadWsg("tile006.wsg", &tilemap->tiles[6]);
    loadWsg("tile007.wsg", &tilemap->tiles[7]);
    loadWsg("tile008.wsg", &tilemap->tiles[8]);
    loadWsg("tile009.wsg", &tilemap->tiles[9]);
}

void tileSpawnEntity(tilemap_t * tilemap, uint8_t objectIndex, uint8_t tx, uint8_t ty) {
    entity_t *entityCreated = createEntity(tilemap->entityManager, objectIndex, (tx << TILE_SIZE_IN_POWERS_OF_2) + 8, (ty << TILE_SIZE_IN_POWERS_OF_2) + 8);

    if(entityCreated != NULL) {
        entityCreated->homeTileX = tx;
        entityCreated->homeTileY = ty;
        tilemap->map[ty * tilemap->mapWidth + tx] = 0;
    }
}