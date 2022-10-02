/*
*   mode_racer.c
*
*   Created on: Sep 12, 2022
*       Author: Jonathan Moriarty
*/

#include <stdlib.h> // calloc
#include <stdio.h> // snprintf
#include <math.h> // sin
#include <string.h> // memset

#include "swadgeMode.h"  // swadge mode
#include "mode_main_menu.h" // return to main menu
#include "mode_racer.h"
#include "esp_timer.h" // timer functions
#include "esp_log.h" // debug logging functions
#include "display.h" // display functions and draw text
#include "bresenham.h"  // draw shapes
#include "linked_list.h" // custom linked list
#include "nvs_manager.h" // saving and loading high scores and last scores
#include "musical_buzzer.h" // music and sfx
#include "led_util.h" // leds

// 240 x 280

// any defines go here.

// controls (title)
#define BTN_TITLE_START_SCORES BTN_B
#define BTN_TITLE_START_GAME BTN_A
#define BTN_TITLE_EXIT_MODE SELECT

// controls (game)
#define BTN_GAME_DROP BTN_B
#define BTN_GAME_ROTATE BTN_A

// controls (scores)
#define BTN_SCORES_CLEAR_SCORES BTN_B
#define BTN_SCORES_START_TITLE BTN_A

// controls (gameover)
#define BTN_GAMEOVER_START_TITLE BTN_B
#define BTN_GAMEOVER_START_GAME BTN_A

// update task info.
#define UPDATE_TIME_MS 16
#define DISPLAY_REFRESH_MS 400 // This is a best guess for syncing LED FX with OLED FX.

// time info.
#define MS_TO_US_FACTOR 1000
#define S_TO_MS_FACTOR 1000
#define US_TO_MS_FACTOR 0.001
#define MS_TO_S_FACTOR 0.001

// useful display.
#define DISPLAY_HALF_HEIGHT 120

// title screen

#define EXIT_MODE_HOLD_TIME (2 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

// score screen
#define CLEAR_SCORES_HOLD_TIME (5 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define NUM_R_HIGH_SCORES 10 // Track this many highest scores.

// game screen


// game input


// scoring


// LED FX
#define MODE_LED_BRIGHTNESS 0.125 // Factor that decreases overall brightness of LEDs since they are a little distracting at full brightness.

// any typedefs go here.

// mode state
typedef enum
{
    R_TITLE,   // title screen
    R_GAME,    // play the actual game
    R_SCORES,  // high scores
    R_GAMEOVER // game over
} racerState_t;

typedef struct
{
    // Title screen vars.
    int64_t exitTimer;
    int64_t lastExitTimer;
    bool holdingExit;

    // Score screen vars.
    int64_t clearScoreTimer;
    int64_t lastClearScoreTimer;
    bool holdingClearScore;
    
    // Game state vars.
    
    // Score related vars.
    int32_t score; // The current score this game.
    int32_t highScores[NUM_R_HIGH_SCORES];
    bool newHighScore;

    // Gameover vars.

    // Input vars.
    accel_t rAccel;
    accel_t rLastAccel;
    accel_t rLastTestAccel;

    uint16_t rButtonState;
    uint16_t rLastButtonState;

    int64_t modeStartTime; // time mode started in microseconds.
    int64_t stateStartTime; // time the most recent state started in microseconds.
    int64_t deltaTime; // time elapsed since last update in microseconds.
    int64_t modeTime; // total time the mode has been running in microseconds.
    int64_t stateTime; // total time the state has been running in microseconds.
    uint32_t modeFrames; // total number of frames elapsed in this mode.
    uint32_t stateFrames; // total number of frames elapsed in this state.

    // Game state.
    racerState_t currState;

    // LED FX vars.
    led_t leds[NUM_LEDS];

    // Display pointer.
    display_t* disp;

    // fonts.
    font_t ibm_vga8;
    font_t radiostars;
} racer_t;

// Struct pointer.
racer_t* racer;

// function prototypes go here.

// mode hook functions.
void rInit(display_t* disp);
void rDeInit(void);
void rButtonCallback(buttonEvt_t* evt);
void rAccelerometerCallback(accel_t* accel);

// game loop functions.
static void rUpdate(int64_t elapsedUs);

// handle inputs.
void rTitleInput(void);
void rGameInput(void);
void rScoresInput(void);
void rGameoverInput(void);

// update any input-unrelated logic.
void rTitleUpdate(void);
void rGameUpdate(void);
void rScoresUpdate(void);
void rGameoverUpdate(void);

// draw the frame.
void rTitleDisplay(void);
void rGameDisplay(void);
void rScoresDisplay(void);
void rGameoverDisplay(void);

// helper functions.

// mode state management.
void rChangeState(racerState_t newState);

// input checking.
bool rIsButtonPressed(uint8_t button);
bool rIsButtonReleased(uint8_t button);
bool rIsButtonDown(uint8_t button);
bool rIsButtonUp(uint8_t button);

// drawing functions.
void plotGround(display_t* disp, int16_t leftSrc, int16_t leftDst, int16_t rightSrc, int16_t rightDst,
                           int16_t y0, int16_t y1, int32_t numVerticalLines, int32_t numHorizontalLines, double lineTweenTimeS,
                           uint32_t currentTimeUS,
                           paletteColor_t col);
uint16_t getCenteredTextX(font_t* font, const char* text, int16_t x0, int16_t x1);
void getNumCentering(font_t* font, const char* text, int16_t achorX0, int16_t anchorX1, int16_t* textX0,
                     int16_t* textX1);

// score operations.
void rGetHighScores(void);
void rSetHighScores(void);

// game logic operations.

// Mode struct hook.
swadgeMode modeRacer =
{
    .modeName = "Racer",
    .fnEnterMode = rInit,
    .fnExitMode = rDeInit,
    .fnMainLoop = rUpdate,
    .fnButtonCallback = rButtonCallback,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = rAccelerometerCallback,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};

void rInit(display_t* disp)
{
    // Allocate and zero memory.
    racer = calloc(1, sizeof(racer_t));

    // Save the display pointer.
    racer->disp = disp;

    // Load some fonts.
    loadFont("ibm_vga8.font", &(racer->ibm_vga8));
    loadFont("radiostars.font", &(racer->radiostars));

    // Initialize a lot of variables.


    // Input vars.
    racer->rAccel.x = 0;
    racer->rAccel.y = 0;
    racer->rAccel.z = 0;

    racer->rLastAccel.x = 0;
    racer->rLastAccel.y = 0;
    racer->rLastAccel.z = 0;

    racer->rLastTestAccel.x = 0;
    racer->rLastTestAccel.y = 0;
    racer->rLastTestAccel.z = 0;

    racer->rButtonState = 0;
    racer->rLastButtonState = 0;

    // Reset mode time tracking.
    racer->modeStartTime = esp_timer_get_time();
    racer->stateStartTime = 0;
    racer->deltaTime = 0;
    racer->modeTime = 0;
    racer->stateTime = 0;
    racer->modeFrames = 0;
    racer->stateFrames = 0;

    // Game state.
    racer->currState = R_TITLE;

    rChangeState(R_TITLE);
}

void rDeInit(void)
{
    freeFont(&(racer->ibm_vga8));
    freeFont(&(racer->radiostars));

    buzzer_stop();

    free(racer);
}

void rButtonCallback(buttonEvt_t* evt)
{
    racer->rButtonState = evt->state;  // Set the state of all buttons
}

void rAccelerometerCallback(accel_t* accel)
{
    // Set the accelerometer values
    racer->rAccel.x = accel->x;
    racer->rAccel.y = accel->y;
    racer->rAccel.z = accel->z;
}

static void rUpdate(int64_t elapsedUs __attribute__((unused)))
{
    // Update time tracking.
    // NOTE: delta time is in microseconds.
    // UPDATE time is in milliseconds.

    int64_t newModeTime = esp_timer_get_time() - racer->modeStartTime;
    int64_t newStateTime = esp_timer_get_time() - racer->stateStartTime;
    racer->deltaTime = newModeTime - racer->modeTime;
    racer->modeTime = newModeTime;
    racer->stateTime = newStateTime;
    racer->modeFrames++;
    racer->stateFrames++;

    // Handle Input (based on the state)
    switch( racer->currState )
    {
        case R_TITLE:
        {
            rTitleInput();
            break;
        }
        case R_GAME:
        {
            rGameInput();
            break;
        }
        case R_SCORES:
        {
            rScoresInput();
            break;
        }
        case R_GAMEOVER:
        {
            rGameoverInput();
            break;
        }
        default:
            break;
    };

    // Mark what our inputs were the last time we acted on them.
    racer->rLastButtonState = racer->rButtonState;
    racer->rLastAccel = racer->rAccel;

    // Handle Game Logic (based on the state)
    switch( racer->currState )
    {
        case R_TITLE:
        {
            rTitleUpdate();
            break;
        }
        case R_GAME:
        {
            rGameUpdate();
            break;
        }
        case R_SCORES:
        {
            rScoresUpdate();
            break;
        }
        case R_GAMEOVER:
        {
            rGameoverUpdate();
            break;
        }
        default:
            break;
    };

    // Handle Drawing Frame (based on the state)
    switch( racer->currState )
    {
        case R_TITLE:
        {
            rTitleDisplay();
            break;
        }
        case R_GAME:
        {
            rGameDisplay();
            break;
        }
        case R_SCORES:
        {
            rScoresDisplay();
            break;
        }
        case R_GAMEOVER:
        {
            rGameoverDisplay();
            break;
        }
        default:
            break;
    };

    // Guidelines.
    //plotLine(racer->disp, 0, 0, 0, 240, c200, 5);
    //plotLine(racer->disp, 140, 0, 140, 240, c200, 5);
    //plotLine(racer->disp, 280, 0, 280, 240, c200, 5);
    //plotLine(racer->disp, 0, 120, 280, 120, c200, 5);

    // Draw debug FPS counter.
    /*double seconds = ((double)stateTime * (double)US_TO_MS_FACTOR * (double)MS_TO_S_FACTOR);
    int32_t fps = (int)((double)stateFrames / seconds);
    snprintf(uiStr, sizeof(uiStr), "FPS: %d", fps);
    drawText(racer->disp->w - getTextWidth(uiStr, TOM_THUMB) - 1, racer->disp->h - (1 * (ibm_vga8.h + 1)), uiStr, TOM_THUMB, c555);*/
}

void rTitleInput(void)
{
    // accel = tilt something on screen like you would a tetrad.
    
    // button a = start game
    if(rIsButtonPressed(BTN_TITLE_START_GAME))
    {
        rChangeState(R_GAME);
    }
    // button b = go to score screen
    else if(rIsButtonPressed(BTN_TITLE_START_SCORES))
    {
        rChangeState(R_SCORES);
    }

    // button select = exit mode
    racer->lastExitTimer = racer->exitTimer;
    if(racer->holdingExit && rIsButtonDown(BTN_TITLE_EXIT_MODE))
    {
        racer->exitTimer += racer->deltaTime;
        if (racer->exitTimer >= EXIT_MODE_HOLD_TIME)
        {
            racer->exitTimer = 0;
            switchToSwadgeMode(&modeMainMenu);
        }
    }
    else if(rIsButtonUp(BTN_TITLE_EXIT_MODE))
    {
        racer->exitTimer = 0;
    }
    // this is added to prevent people holding select from the previous screen and accidentally quitting the game.
    else if(rIsButtonPressed(BTN_TITLE_EXIT_MODE))
    {
        racer->holdingExit = true;
    }
}

void rGameInput(void)
{
    
}

void rScoresInput(void)
{
    racer->lastClearScoreTimer = racer->clearScoreTimer;
    // button a = hold to clear scores.
    if(racer->holdingClearScore && rIsButtonDown(BTN_SCORES_CLEAR_SCORES))
    {
        racer->clearScoreTimer += racer->deltaTime;
        if (racer->clearScoreTimer >= CLEAR_SCORES_HOLD_TIME)
        {
            racer->clearScoreTimer = 0;
            memset(racer->highScores, 0, NUM_R_HIGH_SCORES * sizeof(uint32_t));
        }
    }
    else if(rIsButtonUp(BTN_SCORES_CLEAR_SCORES))
    {
        racer->clearScoreTimer = 0;
    }
    // This is added to prevent people holding left from the previous screen from accidentally clearing their scores.
    else if(rIsButtonPressed(BTN_SCORES_CLEAR_SCORES))
    {
        racer->holdingClearScore = true;
    }

    // button b = go to title screen
    if(rIsButtonPressed(BTN_SCORES_START_TITLE))
    {
        rChangeState(R_TITLE);
    }
}

void rGameoverInput(void)
{
    // button a = start game
    if(rIsButtonPressed(BTN_GAMEOVER_START_GAME))
    {
        rChangeState(R_GAME);
    }
    // button b = go to title screen
    else if(rIsButtonPressed(BTN_GAMEOVER_START_TITLE))
    {
        rChangeState(R_TITLE);
    }
}

void rTitleUpdate(void)
{
    
}

void rGameUpdate(void)
{
    // Refresh the tetrads grid.
    
    // land tetrad
    // update score
    // start clear animation
    // end clear animation
    // clear lines
    // spawn new active tetrad
}

void rScoresUpdate(void)
{

}

void rGameoverUpdate(void)
{
    
}

void rTitleDisplay(void)
{
    // Clear the display.
    racer->disp->clearPx();
}

void rGameDisplay(void)
{
    // Clear the display.
    racer->disp->clearPx();

    // fill the BG sides and play area.
    //fillDisplayArea(racer->disp, 0, 0, racer->disp->w, racer->disp->h, c001);
    //fillDisplayArea(racer->disp, GRID_X, 0, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, racer->disp->h, c000);

    // Draw the BG FX.
    // Goal: noticeable speed-ups when level increases and when soft drop is being held or released.
    //plotGround(racer->disp, GRID_X, 0, xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), racer->disp->w - 1, 0, racer->disp->h, 3, 3,
    //                      5.0,
    //                      racer->modeTime, c112);

    
}

void rScoresDisplay(void)
{
    
}

void rGameoverDisplay(void)
{
    
}

// helper functions.

void rChangeState(racerState_t newState)
{
    racerState_t prevState = racer->currState;
    racer->currState = newState;
    racer->stateStartTime = esp_timer_get_time();
    racer->stateTime = 0;
    racer->stateFrames = 0;

    switch( racer->currState )
    {
        case R_TITLE:
            racer->exitTimer = 0;
            racer->holdingExit = false;


            // If we've come to the title from the game, stop all sound and restart title theme.
            if (prevState != R_SCORES)
            {
                buzzer_stop();
                //buzzer_play_bgm(&titleMusic);
            }

            break;
        case R_GAME:
            // All game restart functions happen here.


            buzzer_stop();
            //buzzer_play_sfx(&gameStartSting);

            break;
        case R_SCORES:

            racer->clearScoreTimer = 0;
            racer->holdingClearScore = false;


            break;
        case R_GAMEOVER:
            // Update high score if needed.

            buzzer_stop();

            break;
        default:
            break;
    };
}

bool rIsButtonPressed(uint8_t button)
{
    return (racer->rButtonState & button) && !(racer->rLastButtonState & button);
}

bool rIsButtonReleased(uint8_t button)
{
    return !(racer->rButtonState & button) && (racer->rLastButtonState & button);
}

bool rIsButtonDown(uint8_t button)
{
    return racer->rButtonState & button;
}

bool rIsButtonUp(uint8_t button)
{
    return !(racer->rButtonState & button);
}

void rGetHighScores(void)
{
    char keyStr[32] = {0};
    for (int32_t i = 0; i < NUM_R_HIGH_SCORES; i++)
    {
        snprintf(keyStr, sizeof(keyStr), "r_high_score_%d", i);
        if (!readNvs32(keyStr, &(racer->highScores[i])))
        {
            racer->highScores[i] = 0;
        }
    }
}

void rSetHighScores(void)
{
    char keyStr[32] = {0};
    for (int32_t i = 0; i < NUM_R_HIGH_SCORES; i++)
    {
        snprintf(keyStr, sizeof(keyStr), "r_high_score_%d", i);
        writeNvs32(keyStr, racer->highScores[i]);
    }
}