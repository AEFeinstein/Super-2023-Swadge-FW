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
#include "led_util.h"
#include "palette.h"
#include "nvs_manager.h"

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

static const song_t sndMenuSelect =
{
    .notes =
    {
        {C_5, 50},{C_4, 50}
    },
    .numNotes = 2,
    .shouldLoop = false
};

static const song_t sndMenuConfirm =
{
    .notes =
    {
        {C_6, 50},{C_5, 50},{C_4, 50}
    },
    .numNotes = 2,
    .shouldLoop = false
};

static const song_t sndMenuDeny =
{
    .notes =
    {
        {C_3, 50},{SILENCE, 50},{C_3, 50}
    },
    .numNotes = 3,
    .shouldLoop = false
};

static const paletteColor_t highScoreNewEntryColors[3] = {c050, c055, c005};

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

    font_t radiostars;

    tilemap_t tilemap;
    entityManager_t entityManager;
    gameData_t gameData;

    uint8_t menuState;
    uint8_t menuSelection;

    int16_t btnState;
    int16_t prevBtnState;

    int32_t frameTimer;

    platformerHighScores_t highScores;

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
void loadPlatformerHighScores(platformer_t* self);
void savePlatformerHighScores(platformer_t* self);
void drawPlatformerHighScores(display_t *d, font_t *font, platformerHighScores_t *highScores, gameData_t *gameData);
uint8_t getHighScoreRank(platformerHighScores_t *highScores, uint32_t newScore);
void insertScoreIntoHighScores(platformerHighScores_t *highScores, uint32_t newScore, char newInitials[], uint8_t rank);
void changeStateNameEntry(platformer_t *self);
void updateNameEntry(platformer_t *self);
void drawNameEntry(display_t *d, font_t *font, gameData_t *gameData);
void changeStateShowHighScores(platformer_t *self);
void updateShowHighScores(platformer_t *self);
void drawShowHighScores(display_t *d, font_t *font, uint8_t menuState);

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

static leveldef_t leveldef[4] = {
    {.filename = "level1-1.bin",
     .timeLimit = 180,
     .checkpointTimeLimit = 90},
    {.filename = "level1-2.bin",
     .timeLimit = 120,
     .checkpointTimeLimit = 90},
    {.filename = "level1-3.bin",
     .timeLimit = 180,
     .checkpointTimeLimit = 90},
    {.filename = "debug.bin",
     .timeLimit = 180,
     .checkpointTimeLimit = 90}};

#define NUM_LEVELS 4

led_t platLeds[NUM_LEDS];

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

    platformer->menuState = 0;
    platformer->menuSelection = 0;
    platformer->btnState = 0;
    platformer->prevBtnState = 0;

    loadPlatformerHighScores(platformer);
    //insertScoreIntoHighScores(&(platformer->highScores), 1000000, "EFV", getHighScoreRank(&(platformer->highScores), 1000000));

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
    savePlatformerHighScores(platformer);
    freeFont(&platformer->radiostars);

    // TODO
    // freeWsg(platformer->tilemap->tiles);
    // freeWsg(platformer->tilemap->tilemap_buffer);

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
    fillDisplayArea( self->disp, 0, 0, 280, 240, self->gameData.bgColor);

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

    updateComboTimer(&(self->gameData));

    self->prevBtnState = self->btnState;
    self->gameData.prevBtnState = self->prevBtnState;
}

void drawPlatformerHud(display_t *d, font_t *font, gameData_t *gameData)
{
    char coinStr[8];
    snprintf(coinStr, sizeof(coinStr) - 1, "C:%02d", gameData->coins);

    char scoreStr[32];
    snprintf(scoreStr, sizeof(scoreStr) - 1, "%06d", gameData->score);

    char levelStr[15];
    snprintf(levelStr, sizeof(levelStr) - 1, "Level %d-%d", gameData->world, gameData->level);

    char livesStr[8];
    snprintf(livesStr, sizeof(livesStr) - 1, "x%d", gameData->lives);

    char timeStr[10];
    snprintf(timeStr, sizeof(timeStr) - 1, "T:%03d", gameData->countdown);

    if(gameData->frameCount > 10) {
        drawText(d, font, c500, "1UP", 24, 2);
    }
    
    drawText(d, font, c555, livesStr, 56, 2);
    drawText(d, font, c555, coinStr, 160, 16);
    drawText(d, font, c555, scoreStr, 8, 16);
    drawText(d, font, c555, levelStr, 152, 2);
    drawText(d, font, c555, timeStr, 220, 16);

    if(gameData->comboTimer == 0){
        return;
    }

    snprintf(scoreStr, sizeof(scoreStr) - 1, "+%d (x%d)", gameData->comboScore, gameData->combo);
    drawText(d, font, c050, scoreStr, 8, 30);
}

void updateTitleScreen(platformer_t *self)
{
    // Clear the display
    self->disp->clearPx();

    self->gameData.frameCount++;
   

    // Handle inputs
    switch(platformer->menuState){
        case 0:{ 
            if(self->gameData.frameCount > 200){
                changeStateShowHighScores(self);
            }
            else if (
                (self->gameData.btnState & START)
                &&
                !(self->gameData.prevBtnState & START)
            )
            {
                buzzer_play_sfx(&sndMenuConfirm);
                platformer->menuState = 1;
            }

            break;
        }
        case 1:{
            if (
                self->gameData.btnState & START
                &&
                !(self->gameData.prevBtnState & START)
            )
            {
                uint16_t levelIndex = (self->gameData.world-1) * 4 + (self->gameData.level-1);
                if(levelIndex >= NUM_LEVELS){
                    buzzer_play_sfx(&sndMenuDeny);
                    break;
                }

                initializeGameDataFromTitleScreen(&(self->gameData));
                changeStateReadyScreen(self);
            } else if (
                    (
                    self->gameData.btnState & UP
                    &&
                    !(self->gameData.prevBtnState & UP)
                )
            )
            {
                if(platformer->menuSelection > 0){
                    platformer->menuSelection--;
                    buzzer_play_sfx(&sndMenuSelect);
                }
            } else if (
                    (
                    self->gameData.btnState & DOWN
                    &&
                    !(self->gameData.prevBtnState & DOWN)
                )
            )
            {
                if(platformer->menuSelection < 1){
                    platformer->menuSelection++;
                    buzzer_play_sfx(&sndMenuSelect);
                }
            } else if (
                    (
                    self->gameData.btnState & LEFT
                    &&
                    !(self->gameData.prevBtnState & LEFT)
                )
            )
            {
                if(platformer->menuSelection == 1){
                    platformer->gameData.level--;
                    if(platformer->gameData.level < 1){
                        platformer->gameData.level = 4;
                        if(platformer->gameData.world > 1){
                            platformer->gameData.world--;
                        }
                    }
                    buzzer_play_sfx(&sndMenuSelect);
                }
            } else if (
                    (
                    self->gameData.btnState & RIGHT
                    &&
                    !(self->gameData.prevBtnState & RIGHT)
                )
            )
            {
                if(platformer->menuSelection == 1){
                    platformer->gameData.level++;
                    if(platformer->gameData.level > 4){
                        platformer->gameData.level = 1;
                        if(platformer->gameData.world < 8){
                            platformer->gameData.world++;
                        }
                    }
                    buzzer_play_sfx(&sndMenuSelect);
                }
            } else if (
                    (
                    self->gameData.btnState & BTN_B
                    &&
                    !(self->gameData.prevBtnState & BTN_B)
                )
            )
            {
                platformer->menuState = 0;
                buzzer_play_sfx(&sndMenuConfirm);
            }
            break;
        }
        default:
            platformer->menuState = 0;
            break;
    }


    scrollTileMap(&(platformer->tilemap), 2, 0);
    if(self->tilemap.mapOffsetX >= self->tilemap.maxMapOffsetX && self->frameTimer > 19){
        self->tilemap.mapOffsetX = 0;
    }
    

    drawPlatformerTitleScreen(self->disp, &(self->radiostars), &(self->gameData));
    self->prevBtnState = self->btnState;
    self->gameData.prevBtnState = self->prevBtnState;
}

void drawPlatformerTitleScreen(display_t *d, font_t *font, gameData_t *gameData)
{
    drawTileMap(d,&(platformer->tilemap));

    drawText(d, font, c555, "Super Swadge Land", 40, 32);

    switch(platformer->menuState){
        case 0: {
            if ((gameData->frameCount % 20 ) < 10)
            {
                drawText(d, font, c555, "- Press START button -", 20, 128);
            }
            break;
        }
         
        case 1: {
            drawText(d, font, c555, "Start Game", 48, 128);

            char levelStr[24];
            snprintf(levelStr, sizeof(levelStr) - 1, "Level Select: %d-%d", gameData->world, gameData->level);
            drawText(d, font, c555, levelStr, 48, 144);

            drawText(d, font, c555, "->", 32, 128 + platformer->menuSelection * 16);

            break;
        }
        
        default:
            break;
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

    deactivateAllEntities(&(self->entityManager), false);

    uint16_t levelIndex = (self->gameData.world-1) * 4 + (self->gameData.level-1);
    loadMapFromFile(&(platformer->tilemap), leveldef[levelIndex].filename);
    self->gameData.countdown = leveldef[levelIndex].timeLimit;

    entityManager_t * entityManager = &(self->entityManager);
    entityManager->viewEntity = createPlayer(entityManager, entityManager->tilemap->warps[0].x * 16, entityManager->tilemap->warps[0].y * 16);
    
    entityManager->playerEntity = entityManager->viewEntity;
    viewFollowEntity(&(self->tilemap),entityManager->playerEntity);
    updateLedsHpMeter(&(self->entityManager),&(self->gameData));

    self->tilemap.executeTileSpawnAll = true;

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
    self->gameData.combo = 0;
    self->gameData.comboTimer = 0;

    buzzer_play_bgm(&sndDie);

    self->update=&updateDead;
}

void updateDead(platformer_t *self){
    // Clear the display
    fillDisplayArea( self->disp, 0, 0, 280, 240, self->gameData.bgColor);
    
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
        changeStateNameEntry(self);
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
    fillDisplayArea( self->disp, 0, 0, 280, 240, self->gameData.bgColor);
    
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
            if(levelIndex >= NUM_LEVELS){
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
    drawText(d, font, c555, "Thanks for playing.", 24, 48);
    drawText(d, font, c555, "Many more battle scenes", 8, 96);
    drawText(d, font, c555, "will soon be available!", 8, 112);
    drawText(d, font, c555, "Bonus 100000pts per life!", 8, 160);
}

void loadPlatformerHighScores(platformer_t* self)
{
    size_t size = sizeof(platformerHighScores_t);
    // Try reading the value
    if(false == readNvsBlob("pf_scores", &(self->highScores), &(size)))
    {
        // Value didn't exist, so write the default
        self->highScores.scores[0] = 100000;
        self->highScores.scores[1] = 80000;
        self->highScores.scores[2] = 40000;
        self->highScores.scores[3] = 20000;
        self->highScores.scores[4] = 10000;

        for(uint8_t i=0; i<NUM_PLATFORMER_HIGH_SCORES; i++){
            self->highScores.initials[i][0] = 'J' + i;
            self->highScores.initials[i][1] = 'P' - i;
            self->highScores.initials[i][2] = 'V' + i;
        }
    }
}

void savePlatformerHighScores(platformer_t* self){
    size_t size = sizeof(platformerHighScores_t);
    writeNvsBlob( "pf_scores", &(self->highScores), size);
}

void drawPlatformerHighScores(display_t *d, font_t *font, platformerHighScores_t *highScores, gameData_t *gameData){
    drawText(d, font, c555, "RANK  SCORE  NAME", 48, 96);
    for(uint8_t i=0; i<NUM_PLATFORMER_HIGH_SCORES; i++){
        char rowStr[32];
        snprintf(rowStr, sizeof(rowStr) - 1, "%d   %06d   %c%c%c", i+1, highScores->scores[i], highScores->initials[i][0], highScores->initials[i][1], highScores->initials[i][2]);
        drawText(d, font, (gameData->rank == i) ? highScoreNewEntryColors[gameData->frameCount % 3] : c555, rowStr, 60, 128 + i*16);
    }
}

uint8_t getHighScoreRank(platformerHighScores_t *highScores, uint32_t newScore){
    uint8_t i;
    for(i=0; i<NUM_PLATFORMER_HIGH_SCORES; i++){
        if(highScores->scores[i] < newScore){
            break;
        }
    }

    return i;
}

void insertScoreIntoHighScores(platformerHighScores_t *highScores, uint32_t newScore, char newInitials[], uint8_t rank){

    if(rank >= NUM_PLATFORMER_HIGH_SCORES){
        return;
    }

    for(uint8_t i=NUM_PLATFORMER_HIGH_SCORES - 1; i>rank; i--){
        highScores->scores[i] = highScores->scores[i-1];
        highScores->initials[i][0] = highScores->initials[i-1][0];
        highScores->initials[i][1] = highScores->initials[i-1][1];
        highScores->initials[i][2] = highScores->initials[i-1][2];
    }

    highScores->scores[rank] = newScore;
    highScores->initials[rank][0] = newInitials[0];
    highScores->initials[rank][1] = newInitials[1];
    highScores->initials[rank][2] = newInitials[2];

}

void changeStateNameEntry(platformer_t *self){
    self->gameData.frameCount = 0;
    uint8_t rank = getHighScoreRank(&(self->highScores),self->gameData.score);
    self->gameData.rank = rank;
    self->menuState = 0;

    if(rank >= NUM_PLATFORMER_HIGH_SCORES){
        self->menuSelection = 0;
        changeStateShowHighScores(self);
        return;
    }

    
    self->menuSelection = self->gameData.initials[0];
    self->update=&updateNameEntry;
}

void updateNameEntry(platformer_t *self){
    self->disp->clearPx();

    if(
        self->gameData.btnState & LEFT
        &&
        !(self->gameData.prevBtnState & LEFT)
    ) {
        self->menuSelection--;

        if(self->menuSelection < 30){
            self->menuSelection = 90;
        }

         self->gameData.initials[self->menuState]=self->menuSelection;
    } else if(
        self->gameData.btnState & RIGHT
        &&
        !(self->gameData.prevBtnState & RIGHT)
    ) {
        self->menuSelection++;

        if(self->menuSelection > 90){
            self->menuSelection = 30;
        }

         self->gameData.initials[self->menuState]=self->menuSelection;
    } else if(
        self->gameData.btnState & BTN_B
        &&
        !(self->gameData.prevBtnState & BTN_B)
    ) {
        if(self->menuState > 0){
            self->menuState--;
        }
    } else if(
        self->gameData.btnState & BTN_A
        &&
        !(self->gameData.prevBtnState & BTN_A)
    ) {
        self->menuState++;
        
        if(self->menuState >2){
            insertScoreIntoHighScores(&(self->highScores), self->gameData.score, self->gameData.initials, self->gameData.rank);
            savePlatformerHighScores(self);
            changeStateShowHighScores(self);
        } else {
            self->menuSelection = self->gameData.initials[self->menuState];
        }
    }
    
    drawNameEntry(self->disp, &(self->radiostars), &(self->gameData));

    self->prevBtnState = self->btnState;
    self->gameData.prevBtnState = self->prevBtnState;
}

void drawNameEntry(display_t *d, font_t *font, gameData_t *gameData){
    drawText(d, font, c555, "Enter your initials!", 48, 64);

    char rowStr[32];
    snprintf(rowStr, sizeof(rowStr) - 1, "%d   %06d   %c%c%c", gameData->rank+1, gameData->score, gameData->initials[0], gameData->initials[1], gameData->initials[2]);
    drawText(d, font, c555, rowStr, 64, 128);
}

void changeStateShowHighScores(platformer_t *self){
    self->gameData.frameCount = 0;
    self->update=&updateShowHighScores;
}

void updateShowHighScores(platformer_t *self){
    self->disp->clearPx();
    self->gameData.frameCount++;

    if(self->gameData.frameCount > 100){
        changeStateTitleScreen(self);
    }

    drawShowHighScores(self->disp, &(self->radiostars), self->menuState);
    drawPlatformerHighScores(self->disp, &(self->radiostars), &(self->highScores), &(self->gameData));
}

void drawShowHighScores(display_t *d, font_t *font, uint8_t menuState){
    if(menuState == 3){
        drawText(d, font, c555, "Your name registrated.", 24, 32);
    } else {
        drawText(d, font, c555, "Do your best!", 72, 32);
    }
}