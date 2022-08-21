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

#include "common_typedef.h"
#include "tilemap.h"
#include "gameData.h"
#include "entityManager.h"
#include "leveldef.h"

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

typedef void (*gameUpdateFuncton_t)(platformer_t *self);
struct platformer_t
{
    display_t * disp;

    font_t ibm_vga8;
    font_t radiostars;

    tilemap_t tilemap;
    entityManager_t entityManager;
    gameData_t gameData;

    int8_t scrolltesttimer;
    int16_t scroll_xspeed;
    int16_t scroll_yspeed;

    int16_t btnState;
    int16_t prevBtnState;

    int32_t frameTimer;

    gameUpdateFuncton_t update;
};

//==============================================================================
// Function Prototypes
//==============================================================================
void drawPlatformerHud(display_t* d, font_t* font, gameData_t* gameData);
void drawPlatformerTitleScreen(display_t* d, font_t* font, gameData_t* gameData);

//==============================================================================
// Variables
//==============================================================================

platformer_t * platformer;

swadgeMode modePlatformer =
{
    .modeName = "Platformer",
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

static leveldef_t leveldef[2] ={ 
    {
        .filename = "level1-1.bin",
        .timeLimit = 300,
        .checkpointTimeLimit = 150
    },{
        .filename = "level1-1.bin",
        .timeLimit = 300,
        .checkpointTimeLimit = 150
    }
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
    platformer->prevBtnState = 0;

    loadFont("radiostars.font", &platformer->radiostars);

    initializeTileMap(&(platformer->tilemap));

    loadMapFromFile(&(platformer->tilemap), leveldef[0].filename);

    initializeGameData(&(platformer->gameData));
    initializeEntityManager(&(platformer->entityManager), &(platformer->tilemap), &(platformer->gameData));

    platformer->tilemap.entityManager = &(platformer->entityManager);
    platformer->tilemap.tileSpawnEnabled = true;

    platformer->update = &updateTitleScreen;
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
        /*platformer->disp->clearPx();
    
        updateEntities(&(platformer->entityManager));

        drawTileMap(platformer->disp, &(platformer->tilemap));
        drawEntities(platformer->disp, &(platformer->entityManager));
        drawPlatformerHud(platformer->disp, &(platformer->radiostars), &(platformer->gameData));

        platformer->prevBtnState = platformer->btnState;
        platformer->gameData.prevBtnState = platformer->prevBtnState;*/

        platformer->update(platformer);
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
    platformer->gameData.btnState = evt->state;
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

void updateGame(platformer_t *self){
    // Clear the display
    self->disp->clearPx();

    updateEntities(&(self->entityManager));

    drawTileMap(self->disp, &(self->tilemap));
    drawEntities(self->disp, &(self->entityManager));
    drawPlatformerHud(self->disp, &(self->radiostars), &(self->gameData));

    self->prevBtnState = self->btnState;
    self->gameData.prevBtnState = self->prevBtnState;
}

void drawPlatformerHud(display_t* d, font_t* font, gameData_t* gameData){
    char coinStr[8];
    snprintf(coinStr, sizeof(coinStr) - 1, "%02d", gameData->coins);

    char scoreStr[8];
    snprintf(scoreStr, sizeof(scoreStr) - 1, "%06d", gameData->score);

    drawText(d, font, c555, coinStr, 141, 16);
    drawText(d, font, c555, scoreStr, 16, 16);
}

void updateTitleScreen(platformer_t *self){
    // Clear the display
    self->disp->clearPx();

    //Handle inputs
    if(
        ((self->gameData.btnState & BTN_B) && !(self->gameData.prevBtnState & BTN_B))
        ||
        ((self->gameData.btnState & BTN_A) && !(self->gameData.prevBtnState & BTN_B))
    ){
        self->update = &updateGame;
    }

    drawPlatformerTitleScreen(self->disp,  &(self->radiostars), &(self->gameData));

    self->prevBtnState = self->btnState;
    self->gameData.prevBtnState = self->prevBtnState;
}

void drawPlatformerTitleScreen(display_t* d, font_t* font, gameData_t* gameData){
    drawText(d, font, c555, "Super\n Swadge\n Land", 16, 32);
    drawText(d, font, c555, "Press A or B to start", 0, 128);
}
