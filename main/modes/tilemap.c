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
    tilemap->tilemapOffsetX = 0;
    tilemap->tilemapOffsetY = 0;
    tilemap->mapOffsetX = 0;
    tilemap->mapOffsetY = 0;
    
    loadBlankWsg(&tilemap->tilemap_buffer, TILEMAP_BUFFER_WIDTH_PIXELS, TILEMAP_BUFFER_HEIGHT_PIXELS);
    loadWsg("tiles.wsg", &tilemap->tiles);

    loadMapFromFile(tilemap, "level_test.bin");

    for (int y=0; y < TILEMAP_BUFFER_HEIGHT_TILES; y++) 
    {

        for (int x=0; x < TILEMAP_BUFFER_WIDTH_TILES; x++) 
        {
            drawTile(tilemap, tilemap->map[(y * tilemap->mapWidth) + x], x * TILE_SIZE, y * TILE_SIZE);
        }

    }
}

void drawTileMap(display_t * disp, tilemap_t * tilemap)
{
    drawWsgTiled(disp, &(tilemap->tilemap_buffer), tilemap->tilemapOffsetX, tilemap->tilemapOffsetY);
}

void scrollTileMap(tilemap_t * tilemap, int16_t x, int16_t y) {
    if(x != 0){

        int16_t currentUpdateColumn = WRAP(-tilemap->tilemapOffsetX - ((x > 0)?TILE_SIZE:0), TILEMAP_BUFFER_WIDTH_PIXELS) >> 4;
        
        tilemap->tilemapOffsetX = WRAP(tilemap->tilemapOffsetX - x, TILEMAP_BUFFER_WIDTH_PIXELS);
        tilemap->mapOffsetX += x;

        int16_t newUpdateColumn = WRAP(-tilemap->tilemapOffsetX - ((x > 0)?TILE_SIZE:0), TILEMAP_BUFFER_WIDTH_PIXELS) >> 4;

        //TODO
        //BUG: Because currentUpdateColumn and newUpdateColumn 
        //     are constrained to the range 0..TILEMAP_BUFFER_WIDTH_IN_TILES,
        //     the updateColumnDelta won't always calculate as intended

        int8_t updateColumnDelta = newUpdateColumn - currentUpdateColumn;

        if(updateColumnDelta != 0) {
            updateTileMapColumn(tilemap, newUpdateColumn, updateColumnDelta);
        }

    }

    if(y != 0){

        int16_t currentUpdateRow = WRAP(-tilemap->tilemapOffsetY - ((y > 0)?TILE_SIZE:0), TILEMAP_BUFFER_HEIGHT_PIXELS) >> 4;
        
        tilemap->tilemapOffsetY = WRAP(tilemap->tilemapOffsetY - y, TILEMAP_BUFFER_HEIGHT_PIXELS);
        tilemap->mapOffsetY += y;

        int16_t newUpdateRow= WRAP(-tilemap->tilemapOffsetY - ((y > 0)?TILE_SIZE:0), TILEMAP_BUFFER_HEIGHT_PIXELS) >> 4;

        int8_t updateRowDelta = newUpdateRow - currentUpdateRow;

        if(currentUpdateRow != newUpdateRow) {
            updateTileMapRow(tilemap, newUpdateRow, updateRowDelta);
        }

    }
}

void updateTileMapColumn(tilemap_t * tilemap, int16_t column, int8_t updateColumnDelta){
    for (int y=0; y < TILEMAP_BUFFER_HEIGHT_TILES; y++) 
    {
        uint8_t tile = tilemap->map[(y * tilemap->mapWidth) + (tilemap->mapOffsetX >> 4) + ((updateColumnDelta > 0) ? TILEMAP_BUFFER_WIDTH_TILES : -1)];
        drawTile(tilemap, tile, column * TILE_SIZE, y * TILE_SIZE);
    }
}

void updateTileMapRow(tilemap_t * tilemap, int16_t row, int8_t updateRowDelta){
    for (int x=0; x < TILEMAP_BUFFER_WIDTH_TILES; x++) 
    {
        uint8_t tile = tilemap->map[( ( (tilemap->mapOffsetY >> 4) + ((updateRowDelta > 0) ? TILEMAP_BUFFER_HEIGHT_TILES : -1) )  * tilemap->mapWidth) + x];
        drawTile(tilemap, tile, x * TILE_SIZE, row * TILE_SIZE);
    }
}

void drawTile(tilemap_t * tilemap, uint8_t tileId, int16_t x, int16_t y)
{
    drawPartialWsgIntoWsg(&tilemap->tiles, &tilemap->tilemap_buffer, tileId * TILE_SIZE, 0, (tileId * TILE_SIZE) + TILE_SIZE, TILE_SIZE, x, y);
}

bool loadMapFromFile(tilemap_t * tilemap, char * name)
{
    if(tilemap->map != NULL){
        free(&(tilemap->map));
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
