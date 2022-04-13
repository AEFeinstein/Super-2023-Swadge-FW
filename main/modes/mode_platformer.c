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

#include "tilemap.h"

//==============================================================================
// Constants
//==============================================================================


//==============================================================================
// Functions Prototypes
//==============================================================================

void platformerEnterMode(display_t * disp);
void platformerExitMode(void);
void platformerMainLoop(int64_t elapsedUs);
void platformerButtonCb(buttonEvt_t* evt);
void platformerCb(const char* opt);

//==============================================================================
// Structs
//==============================================================================


typedef struct
{
    display_t * disp;

    font_t ibm_vga8;
    font_t radiostars;

    tilemap_t tilemap;

    int8_t scrolltesttimer;
    int16_t scroll_xspeed;
    int16_t scroll_yspeed;

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
    
    platformer->scrolltesttimer = 1;
    platformer->scroll_xspeed = 0;
    platformer->scroll_yspeed = 0;

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
  
    platformer->scrolltesttimer --;
    if (platformer->scrolltesttimer < 0) {
        platformer->scrolltesttimer = 127;

        platformer->scroll_xspeed = -2 + (esp_random() % 5);
        platformer->scroll_yspeed = -2 + (esp_random() % 5);
    }

    scrollTileMap(&(platformer->tilemap), platformer->scroll_xspeed, platformer->scroll_yspeed);

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
            /*case UP:
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
            }*/
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
