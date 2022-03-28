//==============================================================================
// Includes
//==============================================================================

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "swadgeMode.h"
#include "mode_fighter.h"
#include "aabb_utils.h"
#include "bresenham.h"
#include "esp_random.h"

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

typedef struct 
{
    wsg_t tilemap_buffer;
    wsg_t tiles;

    int16_t tilemapOffsetX;
    int16_t tilemapOffsetY;
} tilemap_t;

//==============================================================================
// Functions Prototypes
//==============================================================================

void platformerEnterMode(display_t * disp);
void platformerExitMode(void);
void platformerMainLoop(int64_t elapsedUs);
void platformerButtonCb(buttonEvt_t* evt);
void platformerCb(const char* opt);
void initializeTileMap(tilemap_t * tilemap);
void drawTileMap(display_t * disp, tilemap_t * tilemap);

//==============================================================================
// Structs
//==============================================================================



typedef struct
{
    display_t * disp;

    font_t ibm_vga8;
    font_t radiostars;

    tilemap_t tilemap;

} platformer_t;


//==============================================================================
// Variables
//==============================================================================

platformer_t * platformer;

swadgeMode modePlatformer =
{
    .modeName = "platformer",
    .fnEnterMode = platformerEnterMode,
    .fnExitMode = platformerExitMode,
    .fnMainLoop = platformerMainLoop,
    .fnButtonCallback = platformerButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 *
 */
void platformerEnterMode(display_t * disp)
{
    // Allocate memory for this mode
    platformer = (platformer_t *)malloc(sizeof(platformer_t));
    memset(platformer, 0, sizeof(platformer_t));

    // Save a pointer to the display
    platformer->disp = disp;
    

    initializeTileMap(&(platformer->tilemap));
}


/**
 * @brief TODO
 *
 */
void platformerExitMode(void)
{
    freeFont(&platformer->ibm_vga8);
    freeFont(&platformer->radiostars);
    
    //TODO
    //freeWsg(platformer->tilemap->tiles);
    //freeWsg(platformer->tilemap->tilemap_buffer);

    //free(platformer->tilemap);
    free(platformer);
}

/**
 * @brief TODO
 *
 * @param elapsedUs
 */
void platformerMainLoop(int64_t elapsedUs)
{
    platformer->disp->clearPx();
  
    //scrollTileMap(&(platformer->tilemap), -1, -1);

    drawTileMap(platformer->disp, &(platformer->tilemap));

    //drawWsg(platformer->disp, &platformer->block, 16,16);
}

/**
 * @brief TODO
 *
 * @param evt
 */
void platformerButtonCb(buttonEvt_t* evt)
{
    if(evt->down)
    {
        switch (evt->button)
        {
            case UP:
                scrollTileMap(&(platformer->tilemap),0,-1);
                break;
            case DOWN:
                scrollTileMap(&(platformer->tilemap),0,1);
                break;
            case LEFT:
                scrollTileMap(&(platformer->tilemap),-1,0);
                break;
            case RIGHT:
                scrollTileMap(&(platformer->tilemap),1,0);
                break;
            case BTN_A:
            {
                break;
            }
            default:
            {
                break;
            }
        }
    }
}

/**
 * @brief TODO
 * 
 * @param opt 
 */
void platformerCb(const char* opt)
{
    ESP_LOGI("MNU", "%s", opt);
}

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
            drawTile(tilemap, esp_random() % 4, x * TILE_SIZE, y * TILE_SIZE);
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
        
        tilemap->tilemapOffsetX -= x;

        int16_t newUpdateColumn = WRAP(-tilemap->tilemapOffsetX - ((x > 0)?TILE_SIZE:0), TILEMAP_BUFFER_WIDTH_PIXELS) >> 4;

        if(newUpdateColumn != currentUpdateColumn) {
            updateTileMapColumn(tilemap, newUpdateColumn);
        }

    }

    if(y != 0){

        int16_t currentUpdateRow = WRAP(-tilemap->tilemapOffsetY - ((y > 0)?TILE_SIZE:0), TILEMAP_BUFFER_HEIGHT_PIXELS) >> 4;
        
        tilemap->tilemapOffsetY -= y;

        int16_t newUpdateRow= WRAP(-tilemap->tilemapOffsetY - ((y > 0)?TILE_SIZE:0), TILEMAP_BUFFER_HEIGHT_PIXELS) >> 4;

        if(currentUpdateRow != newUpdateRow) {
            updateTileMapRow(tilemap, newUpdateRow);
        }

    }
}

void updateTileMapColumn(tilemap_t * tilemap, int16_t column){
    for (int y=0; y < TILEMAP_BUFFER_HEIGHT_TILES; y++) 
    {
        drawTile(tilemap, esp_random() % 4, column * TILE_SIZE, y * TILE_SIZE);
    }
}

void updateTileMapRow(tilemap_t * tilemap, int16_t row){
    for (int x=0; x < TILEMAP_BUFFER_WIDTH_TILES; x++) 
    {
        drawTile(tilemap, esp_random() % 4, x * TILE_SIZE, row * TILE_SIZE);
    }
}

void drawTile(tilemap_t * tilemap, uint8_t tileId, int16_t x, int16_t y)
{
    drawPartialWsgIntoWsg(&tilemap->tiles, &tilemap->tilemap_buffer, tileId * TILE_SIZE, 0, (tileId * TILE_SIZE) + TILE_SIZE, TILE_SIZE, x, y);
}