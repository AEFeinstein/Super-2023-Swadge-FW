//==============================================================================
// Includes
//==============================================================================

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "swadgeMode.h"
#include "musical_buzzer.h"
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

static const song_t sndGameStart =
{
    .notes =
    {
        {C_4, 50},{SILENCE, 50},{C_4, 100},{SILENCE, 200},{C_5, 100},
        {SILENCE, 100},{E_4, 100},{SILENCE, 100},{E_4, 100},{SILENCE, 100},
        {G_4, 100},{C_5, 100},{SILENCE, 100},{G_4, 100}
    },
    .numNotes = 14,
    .shouldLoop = false
};

static const song_t sndDie =
{
    .notes =
    {
        {C_SHARP_5, 100},{A_SHARP_4, 100},{G_SHARP_4, 200},{F_SHARP_4, 100},
        {D_SHARP_4, 100},{SILENCE, 100},{D_4, 100},{SILENCE, 100},
        {C_SHARP_4, 100}
    },
    .numNotes = 9,
    .shouldLoop = false
};

//==============================================================================
// Functions Prototypes
//==============================================================================

void platformerEnterMode(display_t *disp);
void platformerExitMode(void);
void platformerMainLoop(int64_t elapsedUs);
void platformerButtonCb(buttonEvt_t *evt);
void platformerCb(const char *opt);

//==============================================================================
// Structs
//==============================================================================

typedef void (*gameUpdateFuncton_t)(platformer_t *self);
struct platformer_t
{
    display_t *disp;

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
void drawPlatformerHud(display_t *d, font_t *font, gameData_t *gameData);
void drawPlatformerTitleScreen(display_t *d, font_t *font, gameData_t *gameData);
void changeStateReadyScreen(platformer_t *self);
void updateReadyScreen(platformer_t *self);
void drawReadyScreen(display_t *d, font_t *font, gameData_t *gameData);
void changeStateGame(platformer_t *self);
void detectGameStateChange(platformer_t *self);
void changeStateDead(platformer_t *self);
void updateDead(platformer_t *self);
void changeStateGameOver(platformer_t *self);
void updateGameOver(platformer_t *self);
void drawGameOver(display_t *d, font_t *font, gameData_t *gameData);
void changeStateTitleScreen(platformer_t *self);
void changeStateLevelClear(platformer_t *self);
void updateLevelClear(platformer_t *self);
void drawLevelClear(display_t *d, font_t *font, gameData_t *gameData);
void changeStateGameClear(platformer_t *self);
void updateGameClear(platformer_t *self);
void drawGameClear(display_t *d, font_t *font, gameData_t *gameData);

//==============================================================================
// Variables
//==============================================================================

platformer_t *platformer;

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
        .fnTemperatureCallback = NULL};

static leveldef_t leveldef[2] = {
    {.filename = "level1-1.bin",
     .timeLimit = 180,
     .checkpointTimeLimit = 90},
    {.filename = "level1-2.bin",
     .timeLimit = 120,
     .checkpointTimeLimit = 90}};

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 *
 */
void platformerEnterMode(display_t *disp)
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

    // TODO
    // freeWsg(platformer->tilemap->tiles);
    // freeWsg(platformer->tilemap->tilemap_buffer);

    // free(platformer->tilemap);
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
    if (platformer->frameTimer >= 50000)
    {
        platformer->frameTimer -= 50000;
        platformer->update(platformer);
    }
}

/**
 * @brief TODO
 *
 * @param evt
 */
void platformerButtonCb(buttonEvt_t *evt)
{
    platformer->btnState = evt->state;
    platformer->gameData.btnState = evt->state;
}

/**
 * @brief TODO
 *
 * @param opt
 */
void platformerCb(const char *opt)
{
    ESP_LOGI("MNU", "%s", opt);
}

void updateGame(platformer_t *self)
{
    // Clear the display
    self->disp->clearPx();

    updateEntities(&(self->entityManager));

    drawTileMap(self->disp, &(self->tilemap));
    drawEntities(self->disp, &(self->entityManager));
    detectGameStateChange(self);
    drawPlatformerHud(self->disp, &(self->radiostars), &(self->gameData));

    self->gameData.frameCount++;
    if(self->gameData.frameCount > 20){
        self->gameData.frameCount = 0;
        self->gameData.countdown--;
    }

    self->prevBtnState = self->btnState;
    self->gameData.prevBtnState = self->prevBtnState;
}

void drawPlatformerHud(display_t *d, font_t *font, gameData_t *gameData)
{
    char coinStr[8];
    snprintf(coinStr, sizeof(coinStr) - 1, "C:%02d", gameData->coins);

    char scoreStr[8];
    snprintf(scoreStr, sizeof(scoreStr) - 1, "%06d", gameData->score);

    char levelStr[12];
    snprintf(levelStr, sizeof(levelStr) - 1, "Level %d-%d", gameData->world, gameData->level);

    char livesStr[8];
    snprintf(livesStr, sizeof(livesStr) - 1, "x%d", gameData->lives);

    char timeStr[8];
    snprintf(timeStr, sizeof(timeStr) - 1, "T:%03d", gameData->countdown);

    if(gameData->frameCount > 10) {
        drawText(d, font, c500, "1UP", 16, 2);
    }
    
    drawText(d, font, c555, livesStr, 48, 2);
    drawText(d, font, c555, coinStr, 160, 16);
    drawText(d, font, c555, scoreStr, 8, 16);
    drawText(d, font, c555, levelStr, 160, 2);
    drawText(d, font, c555, timeStr, 224, 16);
}

void updateTitleScreen(platformer_t *self)
{
    // Clear the display
    self->disp->clearPx();

    self->gameData.frameCount++;
    if(self->gameData.frameCount > 20){
        self->gameData.frameCount = 0;
    }

    // Handle inputs
    if (
        ((self->gameData.btnState & BTN_B) && !(self->gameData.prevBtnState & BTN_B)) ||
        ((self->gameData.btnState & BTN_A) && !(self->gameData.prevBtnState & BTN_B)))
    {
        initializeGameData(&(self->gameData));
        changeStateReadyScreen(self);
    }

    drawPlatformerTitleScreen(self->disp, &(self->radiostars), &(self->gameData));

    self->prevBtnState = self->btnState;
    self->gameData.prevBtnState = self->prevBtnState;
}

void drawPlatformerTitleScreen(display_t *d, font_t *font, gameData_t *gameData)
{
    drawText(d, font, c555, "Super Swadge Land", 40, 32);

    if (gameData->frameCount < 10)
    {
        drawText(d, font, c555, "Press A or B to start", 20, 128);
    }
}

void changeStateReadyScreen(platformer_t *self){
    self->gameData.frameCount = 0;
    buzzer_play_bgm(&sndGameStart);
    self->update=&updateReadyScreen;
}

void updateReadyScreen(platformer_t *self){
    // Clear the display
    self->disp->clearPx();
    
    self->gameData.frameCount++;
    if(self->gameData.frameCount > 60){
        changeStateGame(self);
    }

    drawReadyScreen(self->disp, &(self->radiostars), &(self->gameData));
}

void drawReadyScreen(display_t *d, font_t *font, gameData_t *gameData){
    drawPlatformerHud(d, font, gameData);
    drawText(d, font, c555, "Get Ready!", 80, 128);
}

void changeStateGame(platformer_t *self){
    self->gameData.frameCount = 0;

    deactivateAllEntities(&(self->entityManager));

    uint16_t levelIndex = (self->gameData.world-1) * 4 + (self->gameData.level-1);
    loadMapFromFile(&(platformer->tilemap), leveldef[levelIndex].filename);
    self->gameData.countdown = leveldef[levelIndex].timeLimit;

    entityManager_t * entityManager = &(self->entityManager);
    entityManager->viewEntity = createPlayer(entityManager, entityManager->tilemap->warps[0].x * 16, entityManager->tilemap->warps[0].y * 16);
    entityManager->playerEntity = entityManager->viewEntity;

    self->update = &updateGame;
}

void detectGameStateChange(platformer_t *self){
    if(!self->gameData.changeState){
        return;
    }

    switch (self->gameData.changeState)
    {
        case ST_DEAD:
            changeStateDead(self);
            break;

        case ST_READY_SCREEN:
            changeStateReadyScreen(self);
            break;
        
        case ST_LEVEL_CLEAR:
            changeStateLevelClear(self);
            break;

        default:
            break;
    }

    self->gameData.changeState = 0;
}

void changeStateDead(platformer_t *self){
    self->gameData.frameCount = 0;
    self->gameData.lives--;
    buzzer_play_bgm(&sndDie);

    self->update=&updateDead;
}

void updateDead(platformer_t *self){
    // Clear the display
    self->disp->clearPx();
    
    self->gameData.frameCount++;
    if(self->gameData.frameCount > 60){
        if(self->gameData.lives > 0){
            changeStateReadyScreen(self);
        } else {
            changeStateGameOver(self);
        }
    }

    updateEntities(&(self->entityManager));
    drawTileMap(self->disp, &(self->tilemap));
    drawEntities(self->disp, &(self->entityManager));
    drawPlatformerHud(self->disp, &(self->radiostars), &(self->gameData));
}

void updateGameOver(platformer_t *self){
    // Clear the display
    self->disp->clearPx();
    
    self->gameData.frameCount++;
    if(self->gameData.frameCount > 60){
        changeStateTitleScreen(self);
    }

    drawGameOver(self->disp, &(self->radiostars), &(self->gameData));
}

void changeStateGameOver(platformer_t *self){
    self->gameData.frameCount = 0;
    self->update=&updateGameOver;    
}

void drawGameOver(display_t *d, font_t *font, gameData_t *gameData){
    drawPlatformerHud(d, font, gameData);
    drawText(d, font, c555, "Game Over", 80, 128);
}

void changeStateTitleScreen(platformer_t *self){
    self->gameData.frameCount = 0;
    self->update=&updateTitleScreen;
}

void changeStateLevelClear(platformer_t *self){
    self->gameData.frameCount = 0;
    self->update=&updateLevelClear;
}

void updateLevelClear(platformer_t *self){
    // Clear the display
    self->disp->clearPx();
    
    self->gameData.frameCount++;

    if(self->gameData.frameCount > 20){
        if(self->gameData.countdown > 0){
            self->gameData.countdown--;
            self->gameData.score += 50;
        } else if(self->gameData.frameCount % 20 == 0) {
            //Hey look, it's a frame rule!
            
            //Advance to the next level
            self->gameData.level += 1;
            if(self->gameData.level > 4){
                self->gameData.world++;
                self->gameData.level = 1;
            }

            uint16_t levelIndex = (self->gameData.world-1) * 4 + (self->gameData.level-1);
            if(levelIndex >= 2){
                changeStateGameClear(self);
            } else {
                changeStateReadyScreen(self);
            }
        }
    }

    updateEntities(&(self->entityManager));
    drawTileMap(self->disp, &(self->tilemap));
    drawEntities(self->disp, &(self->entityManager));
    drawPlatformerHud(self->disp, &(self->radiostars), &(self->gameData));
    drawLevelClear(self->disp, &(self->radiostars), &(self->gameData));
}

void drawLevelClear(display_t *d, font_t *font, gameData_t *gameData){
    drawPlatformerHud(d, font, gameData);
    drawText(d, font, c555, "Well done!", 80, 128);
}

void changeStateGameClear(platformer_t *self){
    self->gameData.frameCount = 0;
    self->update=&updateGameClear;
}

void updateGameClear(platformer_t *self){
    // Clear the display
    self->disp->clearPx();
    
    self->gameData.frameCount++;

    if(self->gameData.frameCount > 40){
        if(self->gameData.lives > 0){
            if(self->gameData.frameCount % 20 == 0){
                self->gameData.lives--;
                self->gameData.score += 100000;
            }
        } else if(self->gameData.frameCount % 80 == 0) {
            changeStateGameOver(self);
        }
    }

    drawPlatformerHud(self->disp, &(self->radiostars), &(self->gameData));
    drawGameClear(self->disp, &(self->radiostars), &(self->gameData));
}

void drawGameClear(display_t *d, font_t *font, gameData_t *gameData){
    drawPlatformerHud(d, font, gameData);
    drawText(d, font, c555, "Thanks for playing.", 24, 32);
    drawText(d, font, c555, "Many more battle scenes", 8, 80);
    drawText(d, font, c555, "will soon be available!", 8, 96);
    drawText(d, font, c555, "Bonus 100000pts per life!", 8, 144);
}