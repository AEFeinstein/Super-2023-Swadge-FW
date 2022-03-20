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

    wsg_t block;
    wsg_t test_wsg_into_wsg;

    int16_t tilemapOffsetX;
    int16_t tilemapOffsetY;


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
    
    loadWsg("block.wsg", &platformer->block);

    loadBlankWsg(&platformer->test_wsg_into_wsg, 128, 128);
    drawWsgIntoWsg(&platformer->block, &platformer->test_wsg_into_wsg, 13, 25);
    drawWsgIntoWsg(&platformer->block, &platformer->test_wsg_into_wsg, 114, 0);

    platformer->tilemapOffsetX=0;
    platformer->tilemapOffsetY=0;

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

    platformer->tilemapOffsetX--;
    platformer->tilemapOffsetY--;

    drawWsgTiled(platformer->disp, &platformer->test_wsg_into_wsg, platformer->tilemapOffsetX, platformer->tilemapOffsetY);
    drawWsg(platformer->disp, &platformer->block, 16, 16);

    drawTileMap(platformer->disp, &(platformer->tilemap));
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
            case DOWN:
            case LEFT:
            case RIGHT:
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
    loadWsg("block.wsg", &tilemap->tiles);

    for (int y=0; y < TILEMAP_BUFFER_HEIGHT_TILES; y++) 
    {

        for (int x=0; x < TILEMAP_BUFFER_WIDTH_TILES; x++) 
        {

            if( esp_random() % 2) 
            {
                drawWsgIntoWsg(&tilemap->tiles, &tilemap->tilemap_buffer, x * TILE_SIZE, y * TILE_SIZE);
            }

        }

    }
}

void drawTileMap(display_t * disp, tilemap_t * tilemap)
{
    drawWsgTiled(disp, &(tilemap->tilemap_buffer), &tilemap->tilemapOffsetX, &tilemap->tilemapOffsetY);
}