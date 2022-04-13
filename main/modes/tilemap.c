//==============================================================================
// Includes
//==============================================================================

#include "tilemap.h"

//==============================================================================
// Constants
//==============================================================================
#define WRAP(x,u) (((x < 0) ? x + (u + 1) * ((-(x)) / (u + 1) + 1) : x) % (u + 1))

#define TILEMAP_BUFFER_WIDTH_PIXELS 256
#define TILEMAP_BUFFER_HEIGHT_PIXELS 256
#define TILEMAP_BUFFER_WIDTH_TILES 16
#define TILEMAP_BUFFER_HEIGHT_TILES 16

#define TILE_SIZE 16

#define MAP_WIDTH 32
#define MAP_HEIGHT 32

//==============================================================================
// Functions Prototypes
//==============================================================================

void initializeTileMap(tilemap_t * tilemap);
void drawTileMap(display_t * disp, tilemap_t * tilemap);
void scrollTileMap(tilemap_t * tilemap, int16_t x, int16_t y);
void updateTileMapColumn(tilemap_t * tilemap, int16_t column);
void updateTileMapRow(tilemap_t * tilemap, int16_t row);
void drawTile(tilemap_t * tilemap, uint8_t tileId, int16_t x, int16_t y);

//==============================================================================
// Functions
//==============================================================================

void initializeTileMap(tilemap_t * tilemap) 
{
    tilemap->tilemapOffsetX = 0;
    tilemap->tilemapOffsetY = 0;
    
    loadBlankWsg(&tilemap->tilemap_buffer, TILEMAP_BUFFER_WIDTH_PIXELS, TILEMAP_BUFFER_HEIGHT_PIXELS);
    loadWsg("tiles.wsg", &tilemap->tiles);

    for (int y=0; y < TILEMAP_BUFFER_HEIGHT_TILES; y++) 
    {

        for (int x=0; x < TILEMAP_BUFFER_WIDTH_TILES; x++) 
        {
            drawTile(tilemap, esp_random() % 5, x * TILE_SIZE, y * TILE_SIZE);
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

        int16_t newUpdateColumn = WRAP(-tilemap->tilemapOffsetX - ((x > 0)?TILE_SIZE:0), TILEMAP_BUFFER_WIDTH_PIXELS) >> 4;

        if(newUpdateColumn != currentUpdateColumn) {
            updateTileMapColumn(tilemap, newUpdateColumn);
        }

    }

    if(y != 0){

        int16_t currentUpdateRow = WRAP(-tilemap->tilemapOffsetY - ((y > 0)?TILE_SIZE:0), TILEMAP_BUFFER_HEIGHT_PIXELS) >> 4;
        
        tilemap->tilemapOffsetY = WRAP(tilemap->tilemapOffsetY - y, TILEMAP_BUFFER_HEIGHT_PIXELS);

        int16_t newUpdateRow= WRAP(-tilemap->tilemapOffsetY - ((y > 0)?TILE_SIZE:0), TILEMAP_BUFFER_HEIGHT_PIXELS) >> 4;

        if(currentUpdateRow != newUpdateRow) {
            updateTileMapRow(tilemap, newUpdateRow);
        }

    }
}

void updateTileMapColumn(tilemap_t * tilemap, int16_t column){
    for (int y=0; y < TILEMAP_BUFFER_HEIGHT_TILES; y++) 
    {
        drawTile(tilemap, esp_random() % 5, column * TILE_SIZE, y * TILE_SIZE);
    }
}

void updateTileMapRow(tilemap_t * tilemap, int16_t row){
    for (int x=0; x < TILEMAP_BUFFER_WIDTH_TILES; x++) 
    {
        drawTile(tilemap, esp_random() % 5, x * TILE_SIZE, row * TILE_SIZE);
    }
}

void drawTile(tilemap_t * tilemap, uint8_t tileId, int16_t x, int16_t y)
{
    drawPartialWsgIntoWsg(&tilemap->tiles, &tilemap->tilemap_buffer, tileId * TILE_SIZE, 0, (tileId * TILE_SIZE) + TILE_SIZE, TILE_SIZE, x, y);
}