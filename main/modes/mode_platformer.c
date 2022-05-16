//==============================================================================
// Includes
//==============================================================================

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "swadgeMode.h"
#include "mode_platformer.h"
#include "aabb_utils.h"
#include "bresenham.h"
#include "esp_random.h"

#include "tilemap.h"
#include "entityManager.h"

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
    entityManager_t entityManager;

    int8_t scrolltesttimer;
    int16_t scroll_xspeed;
    int16_t scroll_yspeed;

    int32_t btnState;

    int32_t frameTimer;
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
    
    platformer->scrolltesttimer = 127;
    platformer->scroll_xspeed = 2;
    platformer->scroll_yspeed = 0;
    platformer->btnState = 0;

    initializeTileMap(&(platformer->tilemap));
    initializeEntityManager(&(platformer->entityManager), &(platformer->tilemap));
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
    // Execute logic at 20fps
    platformer->frameTimer += elapsedUs;
    if(platformer->frameTimer >= 50000)
    {
        platformer->frameTimer -= 50000;

        // Clear the display
        platformer->disp->clearPx();
    
        //platformer->btnState |= RIGHT;

        // Draw the display
        if(platformer->btnState & LEFT){
            scrollTileMap(&(platformer->tilemap),-4,0);
        } else if(platformer->btnState & RIGHT){
            scrollTileMap(&(platformer->tilemap),4,0);
        }
 
 
        if(platformer->btnState & UP){
            scrollTileMap(&(platformer->tilemap),0,-4);
        } else if(platformer->btnState & DOWN){
            scrollTileMap(&(platformer->tilemap),0,4);
        }
 
        updateEntities(&(platformer->entityManager));

        drawTileMap(platformer->disp, &platformer->tilemap);
        drawEntities(platformer->disp, &(platformer->entityManager));
    }
}

/**
 * @brief TODO
 *
 * @param evt
 */
void platformerButtonCb(buttonEvt_t* evt)
{
    platformer->btnState = evt->state;
    
    /*if(evt->down)
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
    }*/
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
