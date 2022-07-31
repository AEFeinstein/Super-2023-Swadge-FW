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
    tilemap->executeTileSpawnColumn = -1;
    tilemap->executeTileSpawnRow = -1;

    tilemap->animationFrame = 0;
    tilemap->animationTimer = 7;

    loadTiles(tilemap);

    loadMapFromFile(tilemap, "level1-1.bin");
}

void drawTileMap(display_t * disp, tilemap_t * tilemap)
{
    tilemap->animationTimer--;
    if(tilemap->animationTimer < 0){
        tilemap->animationFrame = ((tilemap->animationFrame + 1) % 3);
        tilemap->animationTimer = 7;
    }

    for(uint16_t y = (tilemap->mapOffsetY >> TILE_SIZE_IN_POWERS_OF_2); y < (tilemap->mapOffsetY >> TILE_SIZE_IN_POWERS_OF_2) + TILEMAP_DISPLAY_HEIGHT_TILES; y++)
    {
        if(y >= tilemap->mapHeight){
            break;
        }
        
        for(uint16_t x = (tilemap->mapOffsetX >> TILE_SIZE_IN_POWERS_OF_2); x < (tilemap->mapOffsetX >> TILE_SIZE_IN_POWERS_OF_2) + TILEMAP_DISPLAY_WIDTH_TILES; x++)
        {
            if(x >= tilemap->mapWidth){
                break;
            }

            uint8_t tile = tilemap->map[(y * tilemap->mapWidth) + x];

            //Test animated tiles
            if(tile == 32 || tile == 48){
                tile += tilemap->animationFrame;
            }

            //Draw only non-garbage tiles
            if(tile > 15 && tile < 51)
            {
                drawWsg(disp, &tilemap->tiles[tile - 16], x * TILE_SIZE - tilemap->mapOffsetX, y * TILE_SIZE - tilemap->mapOffsetY, false, false, 0);
            } else if(tile > 127 && tilemap->tileSpawnEnabled && (tilemap->executeTileSpawnColumn == x || tilemap->executeTileSpawnRow == y) ) {
                tileSpawnEntity(tilemap, tile >> 7, x, y);
            }
        }
    }
}

void scrollTileMap(tilemap_t * tilemap, int16_t x, int16_t y) {
    if(x != 0){
        uint8_t oldTx = tilemap->mapOffsetX >> TILE_SIZE_IN_POWERS_OF_2;
        tilemap->mapOffsetX = CLAMP(tilemap->mapOffsetX + x, tilemap->minMapOffsetX, tilemap->maxMapOffsetX);
        uint8_t newTx = tilemap->mapOffsetX >> TILE_SIZE_IN_POWERS_OF_2;

        if(newTx > oldTx) {
            tilemap->executeTileSpawnColumn = oldTx + TILEMAP_DISPLAY_WIDTH_TILES;
        } else if(newTx < oldTx) {
            tilemap->executeTileSpawnColumn = newTx;
        } else {
            tilemap->executeTileSpawnColumn = -1;
        }
    }

    if(y != 0){
        uint8_t oldTy = tilemap->mapOffsetY >> TILE_SIZE_IN_POWERS_OF_2;
        tilemap->mapOffsetY = CLAMP(tilemap->mapOffsetY + y, tilemap->minMapOffsetY, tilemap->maxMapOffsetY);
        uint8_t newTy = tilemap->mapOffsetY >> TILE_SIZE_IN_POWERS_OF_2;

        if(newTy > oldTy) {
            tilemap->executeTileSpawnRow = oldTy + TILEMAP_DISPLAY_HEIGHT_TILES;
        } else if(newTy < oldTy) {
            tilemap->executeTileSpawnRow = newTy;
        } else {
            tilemap->executeTileSpawnRow = -1;
        }
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
    //tiles 0-15 are invisible tiles; 
    //remember to shift out first 4 bits of tile index before drawing tile
    loadWsg("tile016.wsg", &tilemap->tiles[0]);
    loadWsg("tile017.wsg", &tilemap->tiles[1]);
    loadWsg("tile018.wsg", &tilemap->tiles[2]);
    loadWsg("tile019.wsg", &tilemap->tiles[3]);
    loadWsg("tile020.wsg", &tilemap->tiles[4]);
    loadWsg("tile021.wsg", &tilemap->tiles[5]);
    loadWsg("tile022.wsg", &tilemap->tiles[6]);
    loadWsg("tile023.wsg", &tilemap->tiles[7]);
    
    //These tiles have not been designed yet
    loadWsg("tile016.wsg", &tilemap->tiles[8]);
    loadWsg("tile016.wsg", &tilemap->tiles[9]);
    loadWsg("tile016.wsg", &tilemap->tiles[10]);
    loadWsg("tile016.wsg", &tilemap->tiles[11]);
    loadWsg("tile016.wsg", &tilemap->tiles[12]);
    loadWsg("tile016.wsg", &tilemap->tiles[13]);
    loadWsg("tile016.wsg", &tilemap->tiles[14]);
    loadWsg("tile016.wsg", &tilemap->tiles[15]);

    loadWsg("tile032.wsg", &tilemap->tiles[16]);
    loadWsg("tile033.wsg", &tilemap->tiles[17]);
    loadWsg("tile034.wsg", &tilemap->tiles[18]);
    loadWsg("tile016.wsg", &tilemap->tiles[19]);
    loadWsg("tile016.wsg", &tilemap->tiles[20]);
    loadWsg("tile016.wsg", &tilemap->tiles[21]);
    loadWsg("tile016.wsg", &tilemap->tiles[22]);
    loadWsg("tile016.wsg", &tilemap->tiles[23]);

    loadWsg("tile016.wsg", &tilemap->tiles[24]);
    loadWsg("tile016.wsg", &tilemap->tiles[25]);
    loadWsg("tile016.wsg", &tilemap->tiles[26]);
    loadWsg("tile016.wsg", &tilemap->tiles[27]);
    loadWsg("tile016.wsg", &tilemap->tiles[28]);
    loadWsg("tile016.wsg", &tilemap->tiles[29]);
    loadWsg("tile016.wsg", &tilemap->tiles[30]);
    loadWsg("tile016.wsg", &tilemap->tiles[31]);

    loadWsg("tile048.wsg", &tilemap->tiles[32]);
    loadWsg("tile049.wsg", &tilemap->tiles[33]);
    loadWsg("tile050.wsg", &tilemap->tiles[34]);
}

void tileSpawnEntity(tilemap_t * tilemap, uint8_t objectIndex, uint8_t tx, uint8_t ty) {
    entity_t *entityCreated = createEntity(tilemap->entityManager, objectIndex, (tx << TILE_SIZE_IN_POWERS_OF_2) + 8, (ty << TILE_SIZE_IN_POWERS_OF_2) + 8);

    if(entityCreated != NULL) {
        entityCreated->homeTileX = tx;
        entityCreated->homeTileY = ty;
        tilemap->map[ty * tilemap->mapWidth + tx] = 0;
    }
}

uint8_t getTile(tilemap_t *tilemap, uint8_t tx, uint8_t ty){
    //ty = CLAMP(ty, 0, tilemap->mapHeight - 1);

    if(/*ty < 0 ||*/ ty >= tilemap->mapHeight){
        return 0;
    }

    if(/*tx < 0 ||*/ tx >= tilemap->mapWidth){
        return 1;
    }

    return tilemap->map[ty * tilemap->mapWidth + tx];
}

void setTile(tilemap_t *tilemap, uint8_t tx, uint8_t ty, uint8_t newTileId){
    //ty = CLAMP(ty, 0, tilemap->mapHeight - 1);

    if(ty >= tilemap->mapHeight || tx >= tilemap->mapWidth){
        return;
    }

    tilemap->map[ty * tilemap->mapWidth + tx] = newTileId;
}

bool isSolid(uint8_t tileId) {
    switch(tileId) {
        case TILE_EMPTY ... TILE_CTNR_0xE:
            return false;
            break;
        case TILE_INVISIBLE_BLOCK ... TILE_UNUSED_0x2F:
            return true;
            break;
        default:
            return false;
    }
}

bool isInteractive(uint8_t tileId){
    return tileId > TILE_CTNR_0xE && tileId < 59;
}