/*
*   mode_tiltrads.c
*
*   Created on: Aug 2, 2019
*   Ported to 2023 Swadge: August 27, 2022
*       Author: Jonathan Moriarty
*/

#include <stdlib.h>
#include <stdio.h> // snprintf
#include <math.h> // sin
#include <string.h>// memset

#include "swadgeMode.h"  // swadge mode
#include "mode_main_menu.h" // return to main menu
#include "mode_tiltrads.h"
#include "esp_timer.h" // timer functions
#include "esp_log.h" // debug logging functions
#include "display.h" // display functions and draw text
#include "bresenham.h"  // draw shapes
#include "linked_list.h" // custom linked list
#include "nvs_manager.h" // saving and loading high scores and last scores
#include "musical_buzzer.h" // music and sfx
#include "led_util.h" // leds

//NOTES:
// Decided not to handle cascade clears that result from falling tetrads after clears. Closer to target behavior.


// Suppress warnings related to consting of some parameters in function declarations.
// Do second pass on accelerometer code.

// Balance all gameplay and control variables based on feedback. (3-4 minute playtime, t99 round target)
// Test to make sure mode is not a battery killer.
// Test to make sure there are no bugs.

//TODO: tiltrads
//TODO: fonts and ui placement for
//TODO: gameover screen fix
//TODO: menu FX fixes.
//TODO: FX for score screen.

// any defines go here.

//#define NO_STRESS_TRIS // Debug mode that when enabled, stops tetrads from dropping automatically, they will only drop when the drop button is pressed. Useful for testing line clears.

// controls (title)
#define BTN_TITLE_START_SCORES BTN_A
#define BTN_TITLE_START_GAME BTN_B
#define BTN_TITLE_QUIT_MODE SELECT

// controls (game)
#define BTN_GAME_ROTATE BTN_B
#define BTN_GAME_DROP BTN_A

// controls (scores)
#define BTN_SCORES_CLEAR_SCORES BTN_A
#define BTN_SCORES_START_TITLE BTN_B

// controls (gameover)
#define BTN_GAMEOVER_START_TITLE BTN_A
#define BTN_GAMEOVER_START_GAME BTN_B

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

// color palette.
//TODO: const colors

// title screen
#define TUTORIAL_GRID_COLS 10
#define TUTORIAL_GRID_ROWS 26

#define TITLE_LEVEL 5 // The level used for calculating drop speed on the title screen.

// score screen
#define SCORE_SCREEN_TITLE_Y 30
#define SCORE_SCREEN_SCORE_Y 40
#define CLEAR_SCORES_HOLD_TIME (5 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define NUM_TT_HIGH_SCORES 10 // Track this many highest scores.

// game screen
#define EMPTY 0
#define ROTATE_DIR 1 // 1 for clockwise, -1 for anti-clockwise.
#define NUM_ROTATIONS 4
#define NUM_TETRAD_TYPES 7

#define TETRAD_SPAWN_ROT 0
#define TETRAD_SPAWN_X 3
#define TETRAD_SPAWN_Y 0
#define TETRAD_GRID_SIZE 4

#define CLEAR_LINES_ANIM_TIME (0.5 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

// game input
#define ACCEL_SEG_SIZE 25 // higher value more or less means less sensitive.
#define ACCEL_JITTER_GUARD 14 // higher = less sensitive.
#define SOFT_DROP_FACTOR 8
#define SOFT_DROP_FX_FACTOR 2

// playfield
// 240 x 280
#define GRID_X 80
#define GRID_Y -14 // NOTE: This works, which is surprising, and potentially concerning.
#define GRID_UNIT_SIZE 12
#define GRID_COLS 10
#define GRID_ROWS 21

#define NEXT_GRID_X 208
#define NEXT_GRID_Y 15
#define NEXT_GRID_COLS 5
#define NEXT_GRID_ROWS 5

// scoring, all of these are (* level)
#define SCORE_SINGLE 100
#define SCORE_DOUBLE 300
#define SCORE_TRIPLE 500
#define SCORE_QUAD 800
// this is per cell
#define SCORE_SOFT_DROP 1
// this is (* count * level)
#define SCORE_COMBO 50

// difficulty scaling
#define LINE_CLEARS_PER_LEVEL 5

// LED FX
#define MODE_LED_BRIGHTNESS 0.125 // Factor that decreases overall brightness of LEDs since they are a little distracting at full brightness.

// Music and SFX
//TODO: Is more needed now that field is larger?
#define NUM_LAND_FX 16

// any typedefs go here.

// mode state
typedef enum
{
    TT_TITLE,   // title screen
    TT_GAME,    // play the actual game
    TT_SCORES,  // high scores
    TT_GAMEOVER // game over
} tiltradsState_t;

// type of randomization behavior
typedef enum
{
    RANDOM, // Pure random
    BAG,    // 7 Bag
    POOL    // 35 Pool with 6 rolls
} tetradRandomizer_t;

// type of tetrad
typedef enum
{
    I_TETRAD = 1,
    O_TETRAD = 2,
    T_TETRAD = 3,
    J_TETRAD = 4,
    L_TETRAD = 5,
    S_TETRAD = 6,
    Z_TETRAD = 7
} tetradType_t;

// tetrad colors
// tetrad color pairs:
// border, fill

// magenta   c505, c303
// orange    c530, c410

// ice-blue  c335, c113
// yellow    c540, c321

// red       c500, c300
// green     c452, c032

// white     c555, c333
// blue      c112, c001
const paletteColor_t borderColors[7] =
{
    c555, //white
    c500, //red
    c505, //magenta
    c335, //ice-blue
    c540, //yellow
    c530, //orange
    c452  //green
};

const paletteColor_t fillColors[7] =
{
    c333, //white
    c300, //red
    c303, //magenta
    c113, //ice-blue
    c321, //yellow
    c420, //orange
    c032  //green
};

// coordinates on the playfield grid, not the screen.
typedef struct
{
    int16_t c;
    int16_t r;
} coord_t;

// tetrads
typedef struct
{
    tetradType_t type;
    uint32_t gridValue; // When taking up space on a larger grid of multiple tetrads, used to distinguish tetrads from each other.
    int32_t rotation;
    coord_t topLeft;
    uint32_t shape[TETRAD_GRID_SIZE][TETRAD_GRID_SIZE];
} tetrad_t;

const uint32_t iTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] =
{
    {   {0, 0, 0, 0},
        {1, 1, 1, 1},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {   {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 0}
    },
    {   {0, 0, 0, 0},
        {0, 0, 0, 0},
        {1, 1, 1, 1},
        {0, 0, 0, 0}
    },
    {   {0, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 1, 0, 0}
    }
};

const uint32_t oTetradRotations [1][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] =
{
    {   {0, 1, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    }
};

const uint32_t tTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] =
{
    {   {0, 1, 0, 0},
        {1, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {   {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 1, 0, 0},
        {0, 0, 0, 0}
    },
    {   {0, 0, 0, 0},
        {1, 1, 1, 0},
        {0, 1, 0, 0},
        {0, 0, 0, 0}
    },
    {   {0, 1, 0, 0},
        {1, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 0, 0}
    }
};

const uint32_t jTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] =
{
    {   {1, 0, 0, 0},
        {1, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {   {0, 1, 1, 0},
        {0, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 0, 0}
    },
    {   {0, 0, 0, 0},
        {1, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 0}
    },
    {   {0, 1, 0, 0},
        {0, 1, 0, 0},
        {1, 1, 0, 0},
        {0, 0, 0, 0}
    }
};

const uint32_t lTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] =
{
    {   {0, 0, 1, 0},
        {1, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {   {0, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0}
    },
    {   {0, 0, 0, 0},
        {1, 1, 1, 0},
        {1, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {   {1, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 0, 0}
    }
};

const uint32_t sTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] =
{
    {   {0, 1, 1, 0},
        {1, 1, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {   {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 0}
    },
    {   {0, 0, 0, 0},
        {0, 1, 1, 0},
        {1, 1, 0, 0},
        {0, 0, 0, 0}
    },
    {   {1, 0, 0, 0},
        {1, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 0, 0}
    }
};

const uint32_t zTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] =
{
    {   {1, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {   {0, 0, 1, 0},
        {0, 1, 1, 0},
        {0, 1, 0, 0},
        {0, 0, 0, 0}
    },
    {   {0, 0, 0, 0},
        {1, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0}
    },
    {   {0, 1, 0, 0},
        {1, 1, 0, 0},
        {1, 0, 0, 0},
        {0, 0, 0, 0}
    }
};

/*
    J, L, S, T, Z TETRAD
    0>>1    ( 0, 0) (-1, 0) (-1, 1) ( 0,-2) (-1,-2)
    1>>2    ( 0, 0) ( 1, 0) ( 1,-1) ( 0, 2) ( 1, 2)
    2>>3    ( 0, 0) ( 1, 0) ( 1, 1) ( 0,-2) ( 1,-2)
    3>>0    ( 0, 0) (-1, 0) (-1,-1) ( 0, 2) (-1, 2)

    I TETRAD
    0>>1    ( 0, 0) (-2, 0) ( 1, 0) (-2,-1) ( 1, 2)
    1>>2    ( 0, 0) (-1, 0) ( 2, 0) (-1, 2) ( 2,-1)
    2>>3    ( 0, 0) ( 2, 0) (-1, 0) ( 2, 1) (-1,-2)
    3>>0    ( 0, 0) ( 1, 0) (-2, 0) ( 1,-2) (-2, 1)
*/

// NOTE: These tables need to be updated if anti-clockwise rotation needs to be a supported option.
const coord_t iTetradRotationTests [4][5] =
{
    {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},
    {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},
    {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},
    {{0, 0}, {1, 0}, {-2, 1}, {1, 2}, {-2, -1}}
};

const coord_t otjlszTetradRotationTests [4][5] =
{
    {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},
    {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},
    {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},
    {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}
};

// Music / SFX

const song_t singleLineClearSFX  =
{
    .notes = {
        {.note = C_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 8,
    .shouldLoop = false
};

const song_t doubleLineClearSFX  =
{
    .notes = {
        {.note = C_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 8,
    .shouldLoop = false
};

const song_t tripleLineClearSFX  =
{
    .notes = {
        {.note = C_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 8,
    .shouldLoop = false
};

const song_t quadLineClearSFX  =
{
    .notes = {
        {.note = C_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 20,
    .shouldLoop = false
};

const song_t lineOneSFX  =
{
    .notes = {
        {.note = C_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineTwoSFX  =
{
    .notes = {
        {.note = C_SHARP_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineThreeSFX  =
{
    .notes = {
        {.note = D_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineFourSFX  =
{
    .notes = {
        {.note = D_SHARP_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};


const song_t lineFiveSFX  =
{
    .notes = {
        {.note = E_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineSixSFX  =
{
    .notes = {
        {.note = F_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineSevenSFX  =
{
    .notes = {
        {.note = F_SHARP_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineEightSFX  =
{
    .notes = {
        {.note = G_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineNineSFX  =
{
    .notes = {
        {.note = G_SHARP_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineTenSFX  =
{
    .notes = {
        {.note = A_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineElevenSFX  =
{
    .notes = {
        {.note = A_SHARP_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineTwelveSFX  =
{
    .notes = {
        {.note = B_4, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineThirteenSFX  =
{
    .notes = {
        {.note = C_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineFourteenSFX  =
{
    .notes = {
        {.note = C_SHARP_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineFifteenSFX  =
{
    .notes = {
        {.note = D_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t lineSixteenSFX  =
{
    .notes = {
        {.note = D_SHARP_5, .timeMs = 132},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 2,
    .shouldLoop = false
};

// This reverses the order and 0 indexes the land SFXs for easier use in-code.
const song_t* landSFX[NUM_LAND_FX] =
{
    &lineSixteenSFX,
    &lineFifteenSFX,
    &lineFourteenSFX,
    &lineThirteenSFX,
    &lineTwelveSFX,
    &lineElevenSFX,
    &lineTenSFX,
    &lineNineSFX,
    &lineEightSFX,
    &lineSevenSFX,
    &lineSixSFX,
    &lineFiveSFX,
    &lineFourSFX,
    &lineThreeSFX,
    &lineTwoSFX,
    &lineOneSFX
};

const song_t titleMusic = {
    .notes = {
        {.note = C_6, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = SILENCE, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 256,
    .shouldLoop = true
};

const song_t gameStartSting  =
{
    .notes = {
        {.note = C_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 8,
    .shouldLoop = false
};

const song_t gameOverSting  =
{
    .notes = {
        {.note = C_7, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_6, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_6, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_SHARP_5, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_SHARP_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 159},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 16,
    .shouldLoop = false
};

const led_t titleColor =
{
    .r = 0x00,
    .g = 0xFF,
    .b = 0xFF
};

const led_t highScoreColor =
{
    .r = 0xFF,
    .g = 0xFF,
    .b = 0x00
};

//TODO: have these match the new colors.
const led_t tetradColors[NUM_TETRAD_TYPES] =
{
    // I_TETRAD
    {
        .r = 0x00,
        .g = 0xFF,
        .b = 0xFF
    },
    // O_TETRAD
    {
        .r = 0xFF,
        .g = 0xFF,
        .b = 0x00
    },
    // T_TETRAD
    {
        .r = 0xFF,
        .g = 0x00,
        .b = 0xFF
    },
    // J_TETRAD
    {
        .r = 0x00,
        .g = 0x00,
        .b = 0xFF
    },
    // L_TETRAD
    {
        .r = 0xFF,
        .g = 0xA5,
        .b = 0x00
    },
    // S_TETRAD
    {
        .r = 0x00,
        .g = 0xFF,
        .b = 0x00
    },
    // Z_TETRAD
    {
        .r = 0xFF,
        .g = 0x00,
        .b = 0x00
    },
};

const led_t gameoverColor =
{
    .r = 0xFF,
    .g = 0x00,
    .b = 0x00
};

const led_t clearColor =
{
    .r = 0xFF,
    .g = 0xFF,
    .b = 0xFF
};

// Randomizer vars.
tetradRandomizer_t randomizer = POOL;

//BAG
int32_t typeBag[NUM_TETRAD_TYPES] = {I_TETRAD, J_TETRAD, L_TETRAD, O_TETRAD, S_TETRAD, T_TETRAD, Z_TETRAD};
int32_t bagIndex;

//POOL
int32_t typePool[35];
int32_t typeHistory[4];
const int32_t firstType[4]  = {I_TETRAD, J_TETRAD, L_TETRAD, T_TETRAD};
list_t* typeOrder;

// Title screen vars.
// TODO: these are redundant, and could be removed to reduce memory footprint.
tetrad_t tutorialTetrad;
uint32_t tutorialTetradsGrid[TUTORIAL_GRID_ROWS][TUTORIAL_GRID_COLS];

// Score screen vars.
int64_t clearScoreTimer;
int64_t lastClearScoreTimer;
bool holdingClearScore;

// Game state vars.
uint32_t tetradsGrid[GRID_ROWS][GRID_COLS];
uint32_t nextTetradGrid[NEXT_GRID_ROWS][NEXT_GRID_COLS];
tetrad_t activeTetrad;
tetradType_t nextTetradType;
list_t* landedTetrads;
bool activeTetradChange; // Tracks if the active tetrad changed in some way between frames, useful for redraw logic.
uint32_t tetradCounter; // Used for distinguishing tetrads on the full grid, and for counting how many total tetrads have landed.
int64_t dropTimer;  // The timer for dropping the current tetrad one level.
int64_t dropTime; // The amount of time it takes for a tetrad to drop. Changes based on the level.
int64_t dropFXTime; // This is specifically used for handling the perspective effect correctly with regards to increasing dropSpeed.

// Score related vars.
uint32_t linesClearedTotal; // The number of lines cleared total.
uint32_t linesClearedLastDrop; // The number of lines cleared the last time a tetrad landed. (Used for combos)
uint32_t comboCount; // The combo counter for successive line clears.
uint32_t currentLevel; // The current difficulty level, increments every 10 line clears.
int32_t score; // The current score this game.
int32_t highScores[NUM_TT_HIGH_SCORES];
bool newHighScore;

// Clear animation vars.
bool inClearAnimation;
int64_t clearTimer;
int64_t clearTime;

// Score screen ui vars.
//int16_t score0X;
//int16_t score1X;
//int16_t score2X;
//int16_t lastScoreX;

// Gameover vars.
int16_t gameoverScoreX;
int32_t gameoverLEDAnimCycle;

// Input vars.
accel_t ttAccel = {0};
accel_t ttLastAccel = {0};
accel_t ttLastTestAccel = {0};

uint16_t ttButtonState = 0;
uint16_t ttLastButtonState = 0;

// Timer vars.
//static os_timer_t timerHandleUpdate = {0};

int64_t modeStartTime = 0; // time mode started in microseconds.
int64_t stateStartTime = 0; // time the most recent state started in microseconds.
int64_t deltaTime = 0; // time elapsed since last update in microseconds.
int64_t modeTime = 0;  // total time the mode has been running in microseconds.
int64_t stateTime = 0; // total time the state has been running in microseconds.
uint32_t modeFrames = 0; // total number of frames elapsed in this mode.
uint32_t stateFrames = 0; // total number of frames elapsed in this state.

// Game state.
tiltradsState_t currState = TT_TITLE;

// LED FX vars.
led_t leds[NUM_LEDS];

// Display pointer.
display_t* display;

// fonts.
font_t mmFont;
font_t tom_thumb;
font_t ibm_vga8;
font_t radiostars;

// function prototypes go here.
void ttInit(display_t* disp);
void ttDeInit(void);
void ttButtonCallback(buttonEvt_t* evt);
void ttAccelerometerCallback(accel_t* accel);

// game loop functions.
static void ttUpdate(int64_t elapsedUs);

// handle inputs.
void ttTitleInput(void);
void ttGameInput(void);
void ttScoresInput(void);
void ttGameoverInput(void);

// update any input-unrelated logic.
void ttTitleUpdate(void);
void ttGameUpdate(void);
void ttScoresUpdate(void);
void ttGameoverUpdate(void);

// draw the frame.
void ttTitleDisplay(void);
void ttGameDisplay(void);
void ttScoresDisplay(void);
void ttGameoverDisplay(void);

// helper functions.

// mode state management.
void ttChangeState(tiltradsState_t newState);

// input checking.
bool ttIsButtonPressed(uint8_t button);
bool ttIsButtonReleased(uint8_t button);
bool ttIsButtonDown(uint8_t button);
bool ttIsButtonUp(uint8_t button);

// grid management.
void copyGrid(coord_t srcOffset, uint8_t srcCols, uint8_t srcRows, const uint32_t src[][srcCols],
                                uint8_t dstCols, uint8_t dstRows, uint32_t dst[][dstCols]);
void transferGrid(coord_t srcOffset, uint8_t srcCols, uint8_t srcRows,
                                    const uint32_t src[][srcCols], uint8_t dstCols, uint8_t dstRows, uint32_t dst[][dstCols], uint32_t transferVal);
void clearGrid(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols]);
void refreshTetradsGrid(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols],
        list_t* fieldTetrads, tetrad_t* movingTetrad, bool includeMovingTetrad);
int16_t xFromGridCol(int16_t x0, int16_t gridCol, uint8_t unitSize);
int16_t yFromGridRow(int16_t y0, int16_t gridRow, uint8_t unitSize);

// tetrad operations.
bool rotateTetrad(tetrad_t* tetrad, int32_t newRotation, uint8_t gridCols, uint8_t gridRows,
                                    uint32_t gridData[][gridCols]);
void softDropTetrad(void);
bool moveTetrad(tetrad_t* tetrad, uint8_t gridCols, uint8_t gridRows,
                                  uint32_t gridData[][gridCols]);
bool dropTetrad(tetrad_t* tetrad, uint8_t gridCols, uint8_t gridRows,
                                  uint32_t gridData[][gridCols]);
tetrad_t spawnTetrad(tetradType_t type, uint32_t gridValue, coord_t gridCoord, int32_t rotation);
void spawnNextTetrad(tetrad_t* newTetrad, tetradRandomizer_t randomType, uint32_t gridValue,
                                       uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols]);
int32_t getLowestActiveRow(tetrad_t* tetrad);
int32_t getHighestActiveRow(tetrad_t* tetrad);
int32_t getFallDistance(tetrad_t* tetrad, uint8_t gridCols, uint8_t gridRows,
        const uint32_t gridData[][gridCols]);

// drawing functions.
void plotSquare(display_t* disp, int16_t x0, int16_t y0, uint8_t size, paletteColor_t col);
void plotGrid(display_t* disp, int16_t x0, int16_t y0, uint8_t unitSize, uint8_t gridCols, uint8_t gridRows,
                                uint32_t gridData[][gridCols], bool clearLineAnimation, paletteColor_t col);
void plotTetrad(display_t* disp, int16_t x0, int16_t y0, uint8_t unitSize, uint8_t tetradCols, uint8_t tetradRows,
                                  uint32_t shape[][tetradCols], uint8_t tetradFill, int32_t fillRotation, paletteColor_t borderColor, paletteColor_t fillColor);
void plotPerspectiveEffect(display_t* disp, int16_t leftSrc, int16_t leftDst, int16_t rightSrc, int16_t rightDst,
        int16_t y0, int16_t y1, int32_t numVerticalLines, int32_t numHorizontalLines, double lineTweenTimeS,
        uint32_t currentTimeUS,
        paletteColor_t col);
uint16_t getCenteredTextX(font_t* font, const char* text, int16_t x0, int16_t x1);
void getNumCentering(font_t* font, const char* text, int16_t achorX0, int16_t anchorX1, int16_t* textX0, int16_t* textX1);

// randomizer operations.
void initTypeOrder(void);
void clearTypeOrder(void);
void deInitTypeOrder(void);
void initTetradRandomizer(tetradRandomizer_t randomType);
int32_t getNextTetradType(tetradRandomizer_t randomType, int32_t index);
void shuffle(int32_t length, int32_t array[length]);

// score operations.
void loadHighScores(void);
void saveHighScores(void);
bool updateHighScores(uint32_t newScore);

// Tiltrads score functions.
void ttGetHighScores(void);
void ttSetHighScores(void);
//uint32_t ttGetLastScore(void);
//void ttSetLastScore(uint32_t newLastScore);

// game logic operations.
void initLandedTetrads(void);
void clearLandedTetrads(void);
void deInitLandedTetrads(void);

void startClearAnimation(int32_t numLineClears);
void stopClearAnimation(void);

uint32_t getDropTime(uint32_t level);
double getDropFXTimeFactor(uint32_t level);

bool isLineCleared(int32_t line, uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols]);
int32_t checkLineClears(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols],
        list_t* fieldTetrads);
int32_t clearLines(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols],
                                     list_t* fieldTetrads);

bool checkCollision(coord_t newPos, uint8_t tetradCols, uint8_t tetradRows,
                                      const uint32_t shape[][tetradCols], uint8_t gridCols, uint8_t gridRows, const uint32_t gridData[][gridCols],
                                      uint32_t selfGridValue);

// LED FX functions.
void singlePulseLEDs(uint8_t numLEDs, led_t fxColor, double progress);
void blinkLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time);
void alternatingPulseLEDS(uint8_t numLEDs, led_t fxColor, uint32_t time);
void dancingLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time);
void countdownLEDs(uint8_t numLEDs, led_t fxColor, double progress);
void clearLEDs(uint8_t numLEDs);
void applyLEDBrightness(uint8_t numLEDs, double brightness);

// Mode struct hook.
swadgeMode modeTiltrads =
{
    .modeName = "Tiltrads",
    .fnEnterMode = ttInit,
    .fnExitMode = ttDeInit,
    .fnMainLoop = ttUpdate,
    .fnButtonCallback = ttButtonCallback,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = ttAccelerometerCallback,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
    //.menuImageData = mnu_tiltrads_0,
    //.menuImageLen = sizeof(mnu_tiltrads_0)
};

void ttInit(display_t* disp)
{
    // Give us responsive input. Supposedly no longer needed with this HW. TODO: confirm on device.
    //enableDebounce(false); 

    // Reset mode time tracking.
    modeStartTime = esp_timer_get_time();
    modeTime = 0;
    modeFrames = 0;

    // Save a pointer to the display.
    display = disp;

    // Load some fonts
    loadFont("tom_thumb.font", &tom_thumb);
    loadFont("ibm_vga8.font", &ibm_vga8);
    loadFont("radiostars.font", &radiostars);
    loadFont("mm.font", &mmFont);

    // Grab any memory we need.
    initLandedTetrads();
    initTypeOrder();

    // Reset state stuff.
    ttChangeState(TT_TITLE);
}

void ttDeInit(void)
{
    freeFont(&tom_thumb);
    freeFont(&ibm_vga8);
    freeFont(&radiostars);
    freeFont(&mmFont);  

    buzzer_stop();

    deInitLandedTetrads();
    deInitTypeOrder();
}

void ttButtonCallback(buttonEvt_t* evt)
{
    ttButtonState = evt->state;  // Set the state of all buttons
}

void ttAccelerometerCallback(accel_t* accel)
{
    ttAccel.x = accel->x;   // Set the accelerometer values
    ttAccel.y = accel->y;
    ttAccel.z = accel->z;
}

static void ttUpdate(int64_t elapsedUs __attribute__((unused)))
{
    // Update time tracking.
    // NOTE: delta time is in microseconds.
    // UPDATE time is in milliseconds.

    int64_t newModeTime = esp_timer_get_time() - modeStartTime;
    int64_t newStateTime = esp_timer_get_time() - stateStartTime;
    deltaTime = newModeTime - modeTime;
    modeTime = newModeTime;
    stateTime = newStateTime;
    modeFrames++;
    stateFrames++;

    // Handle Input (based on the state)
    switch( currState )
    {
        case TT_TITLE:
        {
            ttTitleInput();
            break;
        }
        case TT_GAME:
        {
            ttGameInput();
            break;
        }
        case TT_SCORES:
        {
            ttScoresInput();
            break;
        }
        case TT_GAMEOVER:
        {
            ttGameoverInput();
            break;
        }
        default:
            break;
    };

    // Mark what our inputs were the last time we acted on them.
    ttLastButtonState = ttButtonState;
    ttLastAccel = ttAccel;

    // Handle Game Logic (based on the state)
    switch( currState )
    {
        case TT_TITLE:
        {
            ttTitleUpdate();
            break;
        }
        case TT_GAME:
        {
            ttGameUpdate();
            break;
        }
        case TT_SCORES:
        {
            ttScoresUpdate();
            break;
        }
        case TT_GAMEOVER:
        {
            ttGameoverUpdate();
            break;
        }
        default:
            break;
    };

    // Handle Drawing Frame (based on the state)
    switch( currState )
    {
        case TT_TITLE:
        {
            ttTitleDisplay();
            break;
        }
        case TT_GAME:
        {
            ttGameDisplay();
            break;
        }
        case TT_SCORES:
        {
            ttScoresDisplay();
            break;
        }
        case TT_GAMEOVER:
        {
            ttGameoverDisplay();
            break;
        }
        default:
            break;
    };

    //plotLine(display, 0, 0, 0, 240, c200, 5);
    //plotLine(display, 140, 0, 140, 240, c200, 5);
    //plotLine(display, 280, 0, 280, 240, c200, 5);
    //plotLine(display, 0, 120, 280, 120, c200, 5);
    // Draw debug FPS counter.
    /*double seconds = ((double)stateTime * (double)US_TO_MS_FACTOR * (double)MS_TO_S_FACTOR);
    int32_t fps = (int)((double)stateFrames / seconds);
    snprintf(uiStr, sizeof(uiStr), "FPS: %d", fps);
    drawText(display->w - getTextWidth(uiStr, TOM_THUMB) - 1, display->h - (1 * (tom_thumb.h + 1)), uiStr, TOM_THUMB, c555);*/
}

void ttTitleInput(void)
{
    //accel = tilt something on screen like you would a tetrad.
    moveTetrad(&tutorialTetrad, TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tutorialTetradsGrid);

    // button a = start game
    if(ttIsButtonPressed(BTN_TITLE_START_GAME))
    {
        ttChangeState(TT_GAME);
    }
    // button b = go to score screen
    else if(ttIsButtonPressed(BTN_TITLE_START_SCORES))
    {
        ttChangeState(TT_SCORES);
    }
    // button start = exit mode
    else if(ttIsButtonPressed(BTN_TITLE_QUIT_MODE))
    {
        switchToSwadgeMode(&modeMainMenu);
    }
}

void ttGameInput(void)
{
    // Reset the check for if the active tetrad moved, dropped, or landed.
    activeTetradChange = false;

    // Refresh the tetrads grid.
    refreshTetradsGrid(GRID_COLS, GRID_ROWS, tetradsGrid, landedTetrads, &(activeTetrad), false);

    // Only respond to input when the clear animation isn't running.
    if (!inClearAnimation)
    {
        //button a = rotate piece
        if(ttIsButtonPressed(BTN_GAME_ROTATE))
        {
            activeTetradChange = rotateTetrad(&activeTetrad, activeTetrad.rotation + ROTATE_DIR, GRID_COLS, GRID_ROWS,
                                              tetradsGrid);
        }

#ifdef NO_STRESS_TRIS
        if(ttIsButtonPressed(BTN_GAME_DROP))
        {
            dropTimer = dropTime;
        }
#else
        //button b = soft drop piece
        if(ttIsButtonDown(BTN_GAME_DROP))
        {
            softDropTetrad();
        }
#endif

        // Only move tetrads left and right when the fast drop button isn't being held down.
        if(ttIsButtonUp(BTN_GAME_DROP))
        {
            activeTetradChange = activeTetradChange || moveTetrad(&activeTetrad, GRID_COLS, GRID_ROWS, tetradsGrid);
        }
    }
}

void ttScoresInput(void)
{
    lastClearScoreTimer = clearScoreTimer;
    //button a = hold to clear scores.
    if(holdingClearScore && ttIsButtonDown(BTN_SCORES_CLEAR_SCORES))
    {
        clearScoreTimer += deltaTime;
        if (clearScoreTimer >= CLEAR_SCORES_HOLD_TIME)
        {
            clearScoreTimer = 0;
            memset(highScores, 0, NUM_TT_HIGH_SCORES * sizeof(uint32_t));
            saveHighScores();
            loadHighScores();
            //ttSetLastScore(0);

            /*char uiStr[32] = {0};
            int16_t x0 = 0;
            int16_t x1 = display->w - 1;
            snprintf(uiStr, sizeof(uiStr), "1. %d", highScores[0]);
            score0X = getCenteredTextX(&ibm_vga8, uiStr, x0, x1);
            snprintf(uiStr, sizeof(uiStr), "2. %d", highScores[1]);
            score1X = getCenteredTextX(&ibm_vga8, uiStr, x0, x1);
            snprintf(uiStr, sizeof(uiStr), "3. %d", highScores[2]);
            score2X = getCenteredTextX(&ibm_vga8, uiStr, x0, x1);*/
            // snprintf(uiStr, sizeof(uiStr), "YOUR LAST SCORE: %d", ttGetLastScore());
            //lastScoreX = getCenteredTextX(x0, x1, uiStr, tom_thumb);
        }
    }
    else if(ttIsButtonUp(BTN_SCORES_CLEAR_SCORES))
    {
        clearScoreTimer = 0;
    }
    // This is added to prevent people holding left from the previous screen from accidentally clearing their scores.
    else if(ttIsButtonPressed(BTN_SCORES_CLEAR_SCORES))
    {
        holdingClearScore = true;
    }

    //button b = go to title screen
    if(ttIsButtonPressed(BTN_SCORES_START_TITLE))
    {
        ttChangeState(TT_TITLE);
    }
}

void ttGameoverInput(void)
{
    //button a = start game
    if(ttIsButtonPressed(BTN_GAMEOVER_START_GAME))
    {
        ttChangeState(TT_GAME);
    }
    //button b = go to title screen
    else if(ttIsButtonPressed(BTN_GAMEOVER_START_TITLE))
    {
        ttChangeState(TT_TITLE);
    }
}

void ttTitleUpdate(void)
{
    // Refresh the tetrads grid.
    refreshTetradsGrid(TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tutorialTetradsGrid, landedTetrads, &(tutorialTetrad),
                       false);

    dropTimer += deltaTime;

    if (dropTimer >= dropTime)
    {
        dropTimer = 0;

        // If we couldn't drop, then we've landed.
        if (!dropTetrad(&tutorialTetrad, TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tutorialTetradsGrid))
        {
            // Spawn the next tetrad.
            spawnNextTetrad(&tutorialTetrad, BAG, 0, TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tutorialTetradsGrid);

            // Reset the drop info to whatever is appropriate for the current level.
            dropTime = getDropTime(TITLE_LEVEL);
            dropTimer = 0;
        }
    }

    dancingLEDs(NUM_LEDS, titleColor, stateTime);
}

void ttGameUpdate(void)
{
    //Refresh the tetrads grid.
    refreshTetradsGrid(GRID_COLS, GRID_ROWS, tetradsGrid, landedTetrads, &(activeTetrad), false);

    // land tetrad
    // update score
    // start clear animation
    // end clear animation
    // clear lines
    // spawn new active tetrad.

    if (inClearAnimation)
    {
        clearTimer += deltaTime;

        double clearProgress = (double)clearTimer / (double)clearTime;
        singlePulseLEDs(NUM_LEDS, clearColor, clearProgress);

        if (clearTimer >= clearTime)
        {
            stopClearAnimation();

            // Actually clear the lines.
            clearLines(GRID_COLS, GRID_ROWS, tetradsGrid, landedTetrads);

            // Spawn the next tetrad.
            spawnNextTetrad(&activeTetrad, randomizer, tetradCounter, GRID_COLS, GRID_ROWS, tetradsGrid);

            // Reset the drop info to whatever is appropriate for the current level.
            dropTime = getDropTime(currentLevel);
            dropTimer = 0;

            // There's a new active tetrad, so that's a change.
            activeTetradChange = true;
        }
    }
    else
    {
#ifndef NO_STRESS_TRIS
        dropTimer += deltaTime;
#endif

        // Update the LED FX.
        // Progress is the drop time for this row. (Too fast)
        //double dropProgress = (double)dropTimer / (double)dropTime;

        // Progress is how close it is to landing on the floor (Too nebulous or unhelpful?)
        double totalFallTime = (GRID_ROWS - 1) * dropTime;
        int32_t fallDistance = getFallDistance(&(activeTetrad), GRID_COLS, GRID_ROWS, tetradsGrid);

        double totalFallProgress = totalFallTime - (((fallDistance + 1) * dropTime) - dropTimer);
        double countdownProgress = totalFallProgress / totalFallTime;

        //NOTE: this check is here because under unknown circumstances the math above can produce bad countdownProgress values, causing a slight flicker when a tetrad lands.
        // Ideally the math above should be fixed, but this is an acceptable fix for now.
        if (countdownProgress >= 0.0 && countdownProgress <= 1.0)
        {
            countdownLEDs(NUM_LEDS, tetradColors[activeTetrad.type - 1], countdownProgress);
        }

        if (dropTimer >= dropTime)
        {
            dropTimer = 0;

            // The active tetrad has either dropped or landed, redraw required either way.
            activeTetradChange = true;

            if (ttIsButtonDown(BTN_GAME_DROP))
            {
                score += SCORE_SOFT_DROP;
            }

            // If we couldn't drop, then we've landed.
            if (!dropTetrad(&(activeTetrad), GRID_COLS, GRID_ROWS, tetradsGrid))
            {
                // Land the current tetrad.
                tetrad_t* landedTetrad = malloc(sizeof(tetrad_t));
                landedTetrad->type = activeTetrad.type;
                landedTetrad->gridValue = activeTetrad.gridValue;
                landedTetrad->rotation = activeTetrad.rotation;
                landedTetrad->topLeft = activeTetrad.topLeft;

                coord_t origin;
                origin.c = 0;
                origin.r = 0;
                copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                         landedTetrad->shape);

                push(landedTetrads, landedTetrad);

                tetradCounter++;

                // Check for any clears now that the new tetrad has landed.
                uint32_t linesClearedThisDrop = checkLineClears(GRID_COLS, GRID_ROWS, tetradsGrid, landedTetrads);

                int32_t landingSFX;

                switch( linesClearedThisDrop )
                {
                    case 1:
                        score += SCORE_SINGLE * (currentLevel + 1);
                        buzzer_play_sfx(&singleLineClearSFX);
                        break;
                    case 2:
                        score += SCORE_DOUBLE * (currentLevel + 1);
                        buzzer_play_sfx(&doubleLineClearSFX);
                        break;
                    case 3:
                        score += SCORE_TRIPLE * (currentLevel + 1);
                        buzzer_play_sfx(&tripleLineClearSFX);
                        break;
                    case 4:
                        score += SCORE_QUAD * (currentLevel + 1);
                        buzzer_play_sfx(&quadLineClearSFX);
                        break;
                    case 0:
                        // Full grid height is 17, we have 16 sfx, offset results by 1 so that sfx[15] correctly plays at the playfield floor.
                        landingSFX = getLowestActiveRow(landedTetrad) - 1;
                        if (landingSFX < 0)
                        {
                            landingSFX = 0;
                        }
                        if (landingSFX > NUM_LAND_FX - 1)
                        {
                            landingSFX = NUM_LAND_FX - 1;
                        }
                        buzzer_play_sfx(landSFX[landingSFX]);
                        break;
                    default:    // Are more than 4 line clears possible? I don't think so.
                        break;
                }

                // This code assumes building combo, and combos are sums of lines cleared.
                if (linesClearedLastDrop > 0 && linesClearedThisDrop > 0)
                {
                    comboCount += linesClearedThisDrop;
                    score += SCORE_COMBO * comboCount * (currentLevel + 1);
                }
                else
                {
                    comboCount = 0;
                }

                // Increase total number of lines cleared.
                linesClearedTotal += linesClearedThisDrop;

                // Update the level if necessary.
                currentLevel = linesClearedTotal / LINE_CLEARS_PER_LEVEL;

                // Keep track of the last number of line clears.
                linesClearedLastDrop = linesClearedThisDrop;

                if (linesClearedThisDrop > 0)
                {
                    // Start the clear animation.
                    startClearAnimation(linesClearedThisDrop);
                }
                else
                {
                    // Spawn the next tetrad.
                    spawnNextTetrad(&activeTetrad, randomizer, tetradCounter, GRID_COLS, GRID_ROWS, tetradsGrid);

                    // Reset the drop info to whatever is appropriate for the current level.
                    dropTime = getDropTime(currentLevel);
                    dropTimer = 0;
                }
            }

            // Clear out empty tetrads.
            node_t* current = landedTetrads->last;
            for (int32_t t = landedTetrads->length - 1; t >= 0; t--)
            {
                tetrad_t* currentTetrad = (tetrad_t*)current->val;
                bool empty = true;

                // Go from bottom-to-top on each position of the tetrad.
                for (int32_t tr = TETRAD_GRID_SIZE - 1; tr >= 0; tr--)
                {
                    for (int32_t tc = 0; tc < TETRAD_GRID_SIZE; tc++)
                    {
                        if (currentTetrad->shape[tr][tc] != EMPTY)
                        {
                            empty = false;
                        }
                    }
                }

                // Adjust the current counter.
                current = current->prev;

                // Remove the empty tetrad.
                if (empty)
                {
                    tetrad_t* emptyTetrad = removeIdx(landedTetrads, t);
                    free(emptyTetrad);
                }
            }

            refreshTetradsGrid(GRID_COLS, GRID_ROWS, tetradsGrid, landedTetrads, &(activeTetrad), false);

            // Handle cascade from tetrads that can now fall freely.
            /*bool possibleCascadeClear = false;
            for (int32_t t = 0; t < numLandedTetrads; t++)
            {
                // If a tetrad could drop, then more clears might have happened.
                if (dropTetrad(&(landedTetrads[t]), GRID_COLS, GRID_ROWS, tetradsGrid))
                {
                    possibleCascadeClear = true;
                }
            }


            if (possibleCascadeClear)
            {
                // Check for any clears now that this new
                checkLineClears(GRID_COLS, GRID_ROWS, tetradsGrid, landedTetrads);
            }*/
        }
    }

    // Drop FX time advances by the normal amount.
    dropFXTime += deltaTime;
    // Drop FX time advances by deltaTime * SOFT_DROP_FX_FACTOR(2) when the soft drop button is being held down. (Happens in softDropTetrad)

    // Drop FX time advances a little bit more according to the currentLevel.
    dropFXTime += (deltaTime * getDropFXTimeFactor(currentLevel));

    // Check if we have a new high score.
    newHighScore = score > highScores[0];
}

void ttScoresUpdate(void)
{
    // Update the LED FX.
    alternatingPulseLEDS(NUM_LEDS, highScoreColor, modeTime);
}

void ttGameoverUpdate(void)
{
    // Update the LED FX.
    blinkLEDs(NUM_LEDS, gameoverColor, stateTime);
}

void ttTitleDisplay(void)
{
    // Clear the display.
    display->clearPx();

    //c112, light grey blue
    //c211, light grey red
    //c021, forest green
    //c221, grey yellow
    //c102

    //c001 = dark blue
    //c112 = light blue border
    //c000 = black
    //c540 = bright yellow

    // fill sides and play area.
    fillDisplayArea(display, 0, 0, display->w, display->h, c001);
    fillDisplayArea(display, GRID_X, 0, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, display->h, c000);

    // Draw demo-scene title FX.
    plotPerspectiveEffect(display, GRID_X, 0, xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), display->w - 1, 0, display->h, 3, 3,
                          2.0,
                          stateTime, c112);

    // SCORES   START
    /*int16_t scoresAreaX0 = 30;
    int16_t scoresAreaY0 = display->h - (tom_thumb.h + 2);
    int16_t scoresAreaX1 = 30 + textWidth(&tom_thumb, "SCORES");
    int16_t scoresAreaY1 = display->h - 1;*/
    //fillDisplayArea(display, scoresAreaX0, scoresAreaY0, scoresAreaX1, scoresAreaY1, BACKGROUND);
    int16_t scoresTextX = 30;
    int16_t scoresTextY = display->h - (tom_thumb.h + 1);
    drawText(display, &tom_thumb, c540, "SCORES", scoresTextX, scoresTextY);

    /*int16_t startAreaX0 = display->w - textWidth(&tom_thumb, "START") - 30;
    int16_t startAreaY0 = display->h - (tom_thumb.h + 2);
    int16_t startAreaX1 = display->w - 31;
    int16_t startAreaY1 = display->h - 1;*/
    //fillDisplayArea(display, startAreaX0, startAreaY0, startAreaX1, startAreaY1, BACKGROUND);
    int16_t startTextX = display->w - textWidth(&tom_thumb, "START") - 30;
    int16_t startTextY = display->h - (tom_thumb.h + 1);
    drawText(display, &tom_thumb, c540, "START", startTextX, startTextY);

    // Clear the grid data (may not want to do this every frame)
    refreshTetradsGrid(TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tutorialTetradsGrid, landedTetrads, &(tutorialTetrad),
                       true);

    // Draw the active tetrad.
    plotTetrad(display, xFromGridCol(GRID_X, tutorialTetrad.topLeft.c, GRID_UNIT_SIZE),
               yFromGridRow(GRID_Y, tutorialTetrad.topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
               tutorialTetrad.shape, tutorialTetrad.type, tutorialTetrad.rotation, borderColors[tutorialTetrad.type-1], fillColors[tutorialTetrad.type-1]);

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(display, GRID_X, GRID_Y, GRID_UNIT_SIZE, TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tutorialTetradsGrid, false, c224);

    // TILTRADS

    /*uint8_t titleAreaX0 = getCenteredTextX(&radiostars, "TILTRADS", 0, display->w);
    uint8_t titleAreaY0 = DISPLAY_HALF_HEIGHT - radiostars.h - 3;
    uint8_t titleAreaX1 = getCenteredTextX(&radiostars, "TILTRADS", 0, display->w) + textWidth(&radiostars, "TILTRADS");
    uint8_t titleAreaY1 = DISPLAY_HALF_HEIGHT - 1;
    fillDisplayArea(display, titleAreaX0, titleAreaY0, titleAreaX1, titleAreaY1, c000);*/

    uint8_t titleTextX = getCenteredTextX(&radiostars, "TILTRADS", 0, display->w);
    uint8_t titleTextY = DISPLAY_HALF_HEIGHT - radiostars.h - 2;
    drawText(display, &radiostars, c540, "TILTRADS", titleTextX, titleTextY);

    //Fill in the floor of the grid on-screen for visual consistency.
    plotLine(display, GRID_X, display->h - 1, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, display->h - 1, c224, 0); //TODO: why is the -1 needed, is my math off?
}

void ttGameDisplay(void)
{
    // Clear the display
    display->clearPx();

    //c001 = dark blue
    //c112 = faded blue effect
    //c224 = light blue border
    //c000 = black
    //c540 = bright yellow

    //tetrad color pairs:
    //border, fill
    // white     c555, c333
    // yellow    c540, c321
    // blue      c112, c001
    // magenta   c505, c303
    // ice-blue  c335, c113
    // red       c500, c300
    // orange    c520, c410


    // fill the BG sides and play area.
    fillDisplayArea(display, 0, 0, display->w, display->h, c001);
    fillDisplayArea(display, GRID_X, 0, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, display->h, c000);

    // Draw the BG FX.
    // Goal: noticeable speed-ups when level increases and when soft drop is being held or released.
    plotPerspectiveEffect(display, GRID_X, 0, xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), display->w - 1, 0, display->h, 3, 3,
                          5.0,
                          dropFXTime, c112);

    // Draw the active tetrad.
    plotTetrad(display, xFromGridCol(GRID_X, activeTetrad.topLeft.c, GRID_UNIT_SIZE),
               yFromGridRow(GRID_Y, activeTetrad.topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
               activeTetrad.shape, activeTetrad.type, activeTetrad.rotation, borderColors[activeTetrad.type-1], fillColors[activeTetrad.type-1]);

    // Draw all the landed tetrads.
    node_t* current = landedTetrads->first;
    for (int32_t t = 0; t < landedTetrads->length; t++)
    {
        tetrad_t* currentTetrad = (tetrad_t*)current->val;
        plotTetrad(display, xFromGridCol(GRID_X, currentTetrad->topLeft.c, GRID_UNIT_SIZE),
                   yFromGridRow(GRID_Y, currentTetrad->topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                   currentTetrad->shape, currentTetrad->type, currentTetrad->rotation, borderColors[currentTetrad->type-1], fillColors[currentTetrad->type-1]);
        current = current->next;
    }

    // Clear the grid data (may not want to do this every frame)
    refreshTetradsGrid(GRID_COLS, GRID_ROWS, tetradsGrid, landedTetrads, &(activeTetrad), true);

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(display, GRID_X, GRID_Y, GRID_UNIT_SIZE, GRID_COLS, GRID_ROWS, tetradsGrid, inClearAnimation, c224);

    // Draw the UI.
    int16_t currY = 0;
    int16_t xPad = 2;
    int16_t yPad = 1;

    // NEXT
    currY = NEXT_GRID_Y - 2 - tom_thumb.h;
    int16_t nextHeaderTextStart = getCenteredTextX(&tom_thumb, "NEXT", 200, display->w);
    int16_t nextHeaderTextEnd = nextHeaderTextStart + textWidth(&tom_thumb, "NEXT");
    //fillDisplayArea(display, nextHeaderTextStart - xPad, currY - yPad, nextHeaderTextEnd + xPad,
    //                currY + (tom_thumb.h - 1) + yPad, c000);
    drawText(display, &tom_thumb, c540, "NEXT", nextHeaderTextStart, currY);

    // Fill area of grid background.
    fillDisplayArea(display, NEXT_GRID_X, NEXT_GRID_Y, xFromGridCol(NEXT_GRID_X, NEXT_GRID_COLS, GRID_UNIT_SIZE),
                    yFromGridRow(NEXT_GRID_Y, NEXT_GRID_ROWS, GRID_UNIT_SIZE), c000);

    // Draw the next tetrad.
    coord_t nextTetradPoint;
    nextTetradPoint.c = 1;
    nextTetradPoint.r = 1;
    tetrad_t nextTetrad = spawnTetrad(nextTetradType, tetradCounter + 1, nextTetradPoint, TETRAD_SPAWN_ROT);
    plotTetrad(display, xFromGridCol(NEXT_GRID_X, nextTetradPoint.c, GRID_UNIT_SIZE),
               yFromGridRow(NEXT_GRID_Y, nextTetradPoint.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
               nextTetrad.shape, nextTetrad.type, nextTetrad.rotation, borderColors[nextTetrad.type-1], fillColors[nextTetrad.type-1]);

    // Draw the grid holding the next tetrad.
    clearGrid(NEXT_GRID_COLS, NEXT_GRID_ROWS, nextTetradGrid);
    copyGrid(nextTetrad.topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, nextTetrad.shape, NEXT_GRID_COLS, NEXT_GRID_ROWS,
             nextTetradGrid);
    plotGrid(display, NEXT_GRID_X, NEXT_GRID_Y, GRID_UNIT_SIZE, NEXT_GRID_COLS, NEXT_GRID_ROWS, nextTetradGrid, false, c224);

    // Draw the left-side score UI.

    char uiStr[32] = {0};

    int16_t numFieldStart = 0;
    int16_t numFieldEnd = 0;

    //HIGH
    currY = NEXT_GRID_Y - 2 - tom_thumb.h;
    int16_t highScoreHeaderTextStart = getCenteredTextX(&tom_thumb, newHighScore ? "HIGH (NEW)" : "HIGH", 0, 80);
    int16_t highScoreHeaderTextEnd = highScoreHeaderTextStart + textWidth(&tom_thumb, newHighScore ? "HIGH (NEW)" : "HIGH"); 

    /*if (newHighScore)
    {
        fillDisplayArea(display, highScoreHeaderTextStart, currY - yPad, highScoreHeaderTextEnd + xPad,
                        currY + (tom_thumb.h - 1) + yPad, c000);
    }
    else
    {
        fillDisplayArea(display, highScoreHeaderTextStart - xPad, currY - yPad, highScoreHeaderTextEnd + xPad,
                        currY + (tom_thumb.h - 1) + yPad, c000);
    }*/

    drawText(display, &tom_thumb, c540, newHighScore ? "HIGH (NEW)" : "HIGH", highScoreHeaderTextStart, currY);

    //99999
    currY += (tom_thumb.h + 1);
    snprintf(uiStr, sizeof(uiStr), "%d", newHighScore ? score : highScores[0]);
    getNumCentering(&tom_thumb, uiStr, 0, GRID_X, &numFieldStart, &numFieldEnd);
    //fillDisplayArea(display, numFieldStart - xPad, currY, numFieldEnd + xPad, currY + tom_thumb.h + yPad, c000);
    drawText(display, &tom_thumb, c540, uiStr, numFieldStart, currY);

    //SCORE
    currY += tom_thumb.h + (tom_thumb.h - 1);
    uint8_t scoreHeaderTextStart = getCenteredTextX(&tom_thumb, "SCORE", 0, 80);
    uint8_t scoreHeaderTextEnd = scoreHeaderTextStart + textWidth(&tom_thumb, "SCORE");
    //fillDisplayArea(display, scoreHeaderTextStart - xPad, currY - yPad, scoreHeaderTextEnd + xPad,
    //                currY + (tom_thumb.h - 1) + yPad, c000);
    drawText(display, &tom_thumb, c540, "SCORE", scoreHeaderTextStart, currY);

    //99999
    currY += (tom_thumb.h + 1);
    snprintf(uiStr, sizeof(uiStr), "%d", score);
    getNumCentering(&tom_thumb, uiStr, 0, GRID_X, &numFieldStart, &numFieldEnd);
    //fillDisplayArea(display, numFieldStart - xPad, currY, numFieldEnd + xPad, currY + tom_thumb.h + yPad, c000);
    drawText(display, &tom_thumb, c540, uiStr, numFieldStart, currY);

    //LINES
    currY += tom_thumb.h + (tom_thumb.h - 1) + 1;
    uint8_t linesHeaderTextStart = getCenteredTextX(&tom_thumb, "LINES", 0, 80);
    uint8_t linesHeaderTextEnd = linesHeaderTextStart + textWidth(&tom_thumb, "LINES");
    //fillDisplayArea(display, linesHeaderTextStart - xPad, currY - yPad, linesHeaderTextEnd + xPad,
    //                currY + (tom_thumb.h - 1) + yPad, c000);
    drawText(display, &tom_thumb, c540, "LINES", linesHeaderTextStart, currY);

    //999
    currY += (tom_thumb.h + 1);
    snprintf(uiStr, sizeof(uiStr), "%d", linesClearedTotal);
    getNumCentering(&tom_thumb, uiStr, 0, GRID_X, &numFieldStart, &numFieldEnd);
    //fillDisplayArea(display, numFieldStart - xPad, currY, numFieldEnd + xPad, currY + tom_thumb.h + yPad, c000);
    drawText(display, &tom_thumb, c540, uiStr, numFieldStart, currY);

    //LEVEL
    currY = yFromGridRow(NEXT_GRID_Y, NEXT_GRID_ROWS, GRID_UNIT_SIZE) + 3;
    uint8_t levelHeaderTextStart = getCenteredTextX(&tom_thumb, "LEVEL", 200, display->w); //TODO: fix magic number
    uint8_t levelHeaderTextEnd = levelHeaderTextStart + textWidth(&tom_thumb, "LEVEL");
    //fillDisplayArea(display, levelHeaderTextStart - xPad, currY - yPad, levelHeaderTextEnd + xPad,
    //               currY + (tom_thumb.h - 1) + yPad, c000);
    drawText(display, &tom_thumb, c540, "LEVEL", levelHeaderTextStart, currY);

    //99
    currY += (tom_thumb.h + 1);
    snprintf(uiStr, sizeof(uiStr), "%d", (currentLevel + 1)); // Levels are displayed with 1 as the base level.
    getNumCentering(&tom_thumb, uiStr, xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE) + 1, display->w - 1, &numFieldStart,
                    &numFieldEnd);
    //fillDisplayArea(display, numFieldStart - xPad, currY, numFieldEnd + xPad, currY + tom_thumb.h + yPad, c000);
    drawText(display, &tom_thumb, c540, uiStr, numFieldStart, currY);

    //DROP
    int16_t leftControlAreaX0 = 30;
    int16_t leftControlAreaY0 = display->h - (tom_thumb.h + 3);
    int16_t leftControlAreaX1 = leftControlAreaX0 + textWidth(&tom_thumb, "DROP") + xPad;
    int16_t leftControlAreaY1 = display->h - 1;
    //fillDisplayArea(display, leftControlAreaX0, leftControlAreaY0, leftControlAreaX1, leftControlAreaY1, c000);
    int16_t leftControlTextX = leftControlAreaX0 + 1;
    int16_t leftControlTextY = display->h - (tom_thumb.h + 1);
    drawText(display, &tom_thumb, c540, "DROP", leftControlTextX, leftControlTextY);

    //ROTATE
    int16_t rightControlAreaX0 = display->w - 30 - textWidth(&tom_thumb, "ROTATE") - xPad;
    int16_t rightControlAreaY0 = display->h - (tom_thumb.h + 3);
    int16_t rightControlAreaX1 = display->w - 30;
    int16_t rightControlAreaY1 = display->h - 1;
    //fillDisplayArea(display, rightControlAreaX0, rightControlAreaY0, rightControlAreaX1, rightControlAreaY1, c000);
    int16_t rightControlTextX = rightControlAreaX0 + 1;
    int16_t rightControlTextY = display->h - (tom_thumb.h + 1);
    drawText(display, &tom_thumb, c540, "ROTATE", rightControlTextX, rightControlTextY);
}

void ttScoresDisplay(void)
{
    // Clear the display.
    display->clearPx();

    //c112, light grey blue
    //c211, light grey red
    //c021, forest green
    //c221, grey yellow
    //c102

    //c001 = dark blue
    //c112 = light blue border
    //c000 = black
    //c540 = bright yellow

    // fill sides and play area.
    fillDisplayArea(display, 0, 0, display->w, display->h, c001);
    fillDisplayArea(display, GRID_X, 0, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, display->h, c000);

    // Draw demo-scene title FX.
    plotPerspectiveEffect(display, GRID_X, 0, xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), display->w - 1, 0, display->h, 3, 3,
                          2.0,
                          stateTime, c112);

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(display, GRID_X, GRID_Y, GRID_UNIT_SIZE, TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tutorialTetradsGrid, false, c224);

    //Fill in the floor of the grid on-screen for visual consistency.
    plotLine(display, GRID_X, display->h - 1, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, display->h - 1, c224, 0); //TODO: why is the -1 needed, is my math off?

    
    // HIGH SCORES
    int16_t headerTextX = getCenteredTextX(&ibm_vga8, "HIGH SCORES", 0, display->w);
    int16_t headerTextY = SCORE_SCREEN_TITLE_Y;
    drawText(display, &ibm_vga8, c540, "HIGH SCORES", headerTextX, headerTextY);

    char uiStr[32] = {0};

    // XX. 99999
    for (int i = 0; i < NUM_TT_HIGH_SCORES; i++)
    {
        int16_t x0 = 0;
        int16_t x1 = display->w - 1;
        snprintf(uiStr, sizeof(uiStr), "%d. %d", i + 1, highScores[i]);
        int16_t scoreX = getCenteredTextX(&ibm_vga8, uiStr, x0, x1);
        drawText(display, &ibm_vga8, c540, uiStr, scoreX, SCORE_SCREEN_SCORE_Y + ((i + 1) * (ibm_vga8.h + 2)) + 4);
    }

    // YOUR LAST SCORE:
    //snprintf(uiStr, sizeof(uiStr), "YOUR LAST SCORE: %d", ttGetLastScore());
    //drawText(lastScoreX, (9 * tom_thumb.h) + 1, uiStr, TOM_THUMB, c555);

    // CLEAR
    int16_t clearScoresTextX = 29;
    int16_t clearScoresTextY = display->h - (tom_thumb.h + 1);
    drawText(display, &tom_thumb, c540, "CLEAR SCORES", clearScoresTextX, clearScoresTextY);

    // fill the clear scores area depending on how long the button's held down.
    if (clearScoreTimer != 0)
    {
        double holdProgress = ((double)clearScoreTimer / (double)CLEAR_SCORES_HOLD_TIME);
        int16_t holdAreaX0 = clearScoresTextX - 1;
        int16_t holdAreaY0 = (display->h - (tom_thumb.h + 1)) - 1;
        double holdAreaWidth = textWidth(&tom_thumb, "CLEAR SCORES") + 3; //TODO: magic number here, check math. 
        int16_t holdAreaX1 = holdAreaX0 + (int16_t)(holdProgress * holdAreaWidth);
        int16_t holdAreaY1 = display->h - 1;
        fillDisplayArea(display, holdAreaX0, holdAreaY0, holdAreaX1, holdAreaY1, c555); //TODO: this was INVERSE, repro this?
    }

    // TITLE
    int16_t titleTextX = display->w - 30 - textWidth(&tom_thumb, "TITLE");
    int16_t titleTextY = display->h - (tom_thumb.h + 1);
    drawText(display, &tom_thumb, c540, "TITLE", titleTextX, titleTextY);
}

void ttGameoverDisplay(void)
{
    // We don't clear the display because we want the playfield to appear in the background.

    // We should only need to draw the gameover ui on the first frame.
    bool drawUI = stateFrames == 0;

    if (drawUI)
    {
        // Draw the active tetrad that was the killing tetrad once so that the flash effect works.
        plotTetrad(display, xFromGridCol(GRID_X, activeTetrad.topLeft.c, GRID_UNIT_SIZE),
                   yFromGridRow(GRID_Y, activeTetrad.topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                   activeTetrad.shape, activeTetrad.type, activeTetrad.rotation, borderColors[activeTetrad.type-1], fillColors[activeTetrad.type-1]);

        // Draw a centered bordered window.
        uint8_t leftWindowXMargin = 82;
        uint8_t rightWindowXMargin = 80;
        uint8_t windowYMarginTop = 80;
        uint8_t windowYMarginBot = 80;

        uint8_t titleTextYOffset = 3;
        uint8_t highScoreTextYOffset = titleTextYOffset + ibm_vga8.h + 3;
        uint8_t scoreTextYOffset = highScoreTextYOffset + tom_thumb.h + 4;
        //uint8_t galleryUnlockTextYOffset = scoreTextYOffset + ibm_vga8.h + 4;
        uint8_t controlTextYOffset = display->h - windowYMarginBot - tom_thumb.h - 2;
        uint8_t controlTextXPadding = 2;

        // Draw a centered bordered window.
        fillDisplayArea(display, leftWindowXMargin, windowYMarginTop, display->w - rightWindowXMargin, display->h - windowYMarginBot, c000);
        plotRect(display, leftWindowXMargin, windowYMarginTop, display->w - rightWindowXMargin, display->h - windowYMarginBot, c540);

        // GAME OVER
        int16_t headerTextX = getCenteredTextX(&ibm_vga8, "GAME OVER", 0, display->w);
        drawText(display, &ibm_vga8, c540, "GAME OVER", headerTextX, windowYMarginTop + titleTextYOffset);

        // HIGH SCORE! or YOUR SCORE:
        if (newHighScore)
        {
            headerTextX = getCenteredTextX(&tom_thumb, "HIGH SCORE!", 0, display->w);
            drawText(display, &tom_thumb, c540, "HIGH SCORE!", headerTextX, windowYMarginTop + highScoreTextYOffset);
        }
        else
        {
            headerTextX = getCenteredTextX(&tom_thumb, "YOUR SCORE:", 0, display->w);
            drawText(display, &tom_thumb, c540, "YOUR SCORE:", headerTextX, windowYMarginTop + highScoreTextYOffset);
        }

        // 1230495
        char scoreStr[32] = {0};
        snprintf(scoreStr, sizeof(scoreStr), "%d", score);
        drawText(display, &ibm_vga8, c540, scoreStr, gameoverScoreX, windowYMarginTop + scoreTextYOffset);

        // TITLE    RESTART
        drawText(display, &tom_thumb, c540, "TITLE", leftWindowXMargin + controlTextXPadding, controlTextYOffset);
        drawText(display, &tom_thumb, c540, "RESTART", display->w - rightWindowXMargin - 27 - controlTextXPadding, controlTextYOffset);
    }

    gameoverLEDAnimCycle = ((double)stateTime * US_TO_MS_FACTOR) / DISPLAY_REFRESH_MS;
    //paletteColor_t killTetradColor = gameoverLEDAnimCycle % 2 == 0 ? c555 : c000;

    // Flash the active tetrad that was the killing tetrad.
    if (gameoverLEDAnimCycle % 2 == 0) 
    {
        plotTetrad(display, xFromGridCol(GRID_X, activeTetrad.topLeft.c, GRID_UNIT_SIZE),
               yFromGridRow(GRID_Y, activeTetrad.topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
               activeTetrad.shape, activeTetrad.type, activeTetrad.rotation, borderColors[activeTetrad.type-1], fillColors[activeTetrad.type-1]);
    }
    else 
    {
        plotTetrad(display, xFromGridCol(GRID_X, activeTetrad.topLeft.c, GRID_UNIT_SIZE),
               yFromGridRow(GRID_Y, activeTetrad.topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
               activeTetrad.shape, activeTetrad.type, activeTetrad.rotation, fillColors[activeTetrad.type-1], borderColors[activeTetrad.type-1]);
    }
}

// helper functions.

void ttChangeState(tiltradsState_t newState)
{
    tiltradsState_t prevState = currState;
    currState = newState;
    stateStartTime = esp_timer_get_time();
    stateTime = 0;
    stateFrames = 0;

    // Used for cache of ui anchors.
    int16_t x0 = 0;
    int16_t x1 = 0;
    char uiStr[32] = {0};

    switch( currState )
    {
        case TT_TITLE:
            clearLandedTetrads();

            // Get a random tutorial tetrad.
            initTetradRandomizer(BAG);
            nextTetradType = (tetradType_t)getNextTetradType(BAG, 0);
            clearGrid(GRID_COLS, GRID_ROWS, tutorialTetradsGrid);
            spawnNextTetrad(&tutorialTetrad, BAG, 0, TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tutorialTetradsGrid);

            // Reset the drop info to whatever is appropriate for the current level.
            dropTime = getDropTime(TITLE_LEVEL);
            dropTimer = 0;

            clearLEDs(NUM_LEDS);
            dancingLEDs(NUM_LEDS, titleColor, stateTime);

            // If we've come to the title from the game, stop all sound and restart title theme.
            if (prevState != TT_SCORES)
            {
                buzzer_stop();
                buzzer_play_bgm(&titleMusic);
            }

            break;
        case TT_GAME:
            // All game restart functions happen here.
            clearGrid(GRID_COLS, GRID_ROWS, tetradsGrid);
            clearGrid(NEXT_GRID_COLS, NEXT_GRID_ROWS, nextTetradGrid);
            tetradCounter = 0;
            clearLandedTetrads();
            linesClearedTotal = 0;
            linesClearedLastDrop = 0;
            comboCount = 0;
            currentLevel = 0;
            score = 0;
            loadHighScores();
            srand((uint32_t)(ttAccel.x + ttAccel.y * 3 + ttAccel.z * 5)); // Seed the random number generator.
            initTetradRandomizer(randomizer);
            nextTetradType = (tetradType_t)getNextTetradType(randomizer, tetradCounter);
            spawnNextTetrad(&activeTetrad, randomizer, tetradCounter, GRID_COLS, GRID_ROWS, tetradsGrid);
            // Reset the drop info to whatever is appropriate for the current level.
            dropTime = getDropTime(currentLevel);
            dropTimer = 0;
            dropFXTime = 0;

            // Reset animation info.
            stopClearAnimation();

            clearLEDs(NUM_LEDS);

            buzzer_stop();
            buzzer_play_sfx(&gameStartSting);

            break;
        case TT_SCORES:
            loadHighScores();

            /*x0 = 0;
            x1 = display->w - 1;
            snprintf(uiStr, sizeof(uiStr), "1. %d", highScores[0]);
            score0X = getCenteredTextX(&ibm_vga8, uiStr, x0, x1);
            snprintf(uiStr, sizeof(uiStr), "2. %d", highScores[1]);
            score1X = getCenteredTextX(&ibm_vga8, uiStr, x0, x1);
            snprintf(uiStr, sizeof(uiStr), "3. %d", highScores[2]);
            score2X = getCenteredTextX(&ibm_vga8, uiStr, x0, x1);*/
            //snprintf(uiStr, sizeof(uiStr), "YOUR LAST SCORE: %d", ttGetLastScore());
            //lastScoreX = getCenteredTextX(x0, x1, uiStr, tom_thumb);

            clearScoreTimer = 0;
            holdingClearScore = false;

            clearLEDs(NUM_LEDS);

            alternatingPulseLEDS(NUM_LEDS, highScoreColor, modeTime);

            break;
        case TT_GAMEOVER:
            // Update high score if needed.
            newHighScore = updateHighScores(score);
            if (newHighScore)
            {
                saveHighScores();
            }

            // Save out the last score.
            //ttSetLastScore(score);

            // Check if we unlocked the gallery image (for the first time)
            //galleryUnlock = score >= GALLERY_UNLOCK_SCORE && unlockGallery(GALLERY_UNLOCK_IMAGE_INDEX);

            // Get the correct offset for the high score.
            x0 = 18;
            x1 = display->w - x0;
            snprintf(uiStr, sizeof(uiStr), "%d", score);
            gameoverScoreX = getCenteredTextX(&ibm_vga8, uiStr, x0, x1);
            gameoverLEDAnimCycle = ((double)stateTime * US_TO_MS_FACTOR) / DISPLAY_REFRESH_MS;

            clearLEDs(NUM_LEDS);

            blinkLEDs(NUM_LEDS, gameoverColor, stateTime);

            buzzer_stop();
            buzzer_play_sfx(&gameOverSting);

            break;
        default:
            break;
    };
}

bool ttIsButtonPressed(uint8_t button)
{
    return (ttButtonState & button) && !(ttLastButtonState & button);
}

bool ttIsButtonReleased(uint8_t button)
{
    return !(ttButtonState & button) && (ttLastButtonState & button);
}

bool ttIsButtonDown(uint8_t button)
{
    return ttButtonState & button;
}

bool ttIsButtonUp(uint8_t button)
{
    return !(ttButtonState & button);
}

void copyGrid(coord_t srcOffset, uint8_t srcCols, uint8_t srcRows, const uint32_t src[][srcCols],
                                uint8_t dstCols, uint8_t dstRows, uint32_t dst[][dstCols])
{
    for (int32_t r = 0; r < srcRows; r++)
    {
        for (int32_t c = 0; c < srcCols; c++)
        {
            int32_t dstC = c + srcOffset.c;
            int32_t dstR = r + srcOffset.r;
            if (dstC < dstCols && dstR < dstRows)
            {
                dst[dstR][dstC] = src[r][c];
            }
        }
    }
}

void transferGrid(coord_t srcOffset, uint8_t srcCols, uint8_t srcRows,
                                    const uint32_t src[][srcCols], uint8_t dstCols, uint8_t dstRows, uint32_t dst[][dstCols], uint32_t transferVal)
{
    for (int32_t r = 0; r < srcRows; r++)
    {
        for (int32_t c = 0; c < srcCols; c++)
        {
            int32_t dstC = c + srcOffset.c;
            int32_t dstR = r + srcOffset.r;
            if (dstC < dstCols && dstR < dstRows)
            {
                if (src[r][c] != EMPTY)
                {
                    dst[dstR][dstC] = transferVal;
                }
            }
        }
    }
}

void clearGrid(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols])
{
    for (int32_t y = 0; y < gridRows; y++)
    {
        for (int32_t x = 0; x < gridCols; x++)
        {
            gridData[y][x] = EMPTY;
        }
    }
}

// NOTE: the grid value of every tetrad is reassigned on refresh to fix a bug that occurs where every 3 tetrads seems to ignore collision, cause unknown.
void refreshTetradsGrid(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols],
        list_t* fieldTetrads, tetrad_t* movingTetrad, bool includeMovingTetrad)
{
    clearGrid(gridCols, gridRows, gridData);

    node_t* current = fieldTetrads->first;
    for (int32_t t = 0; t < fieldTetrads->length; t++)
    {
        tetrad_t* currentTetrad = (tetrad_t*)current->val;
        transferGrid(currentTetrad->topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, currentTetrad->shape, gridCols, gridRows,
                     gridData, currentTetrad->gridValue);
        current = current->next;
    }

    if (includeMovingTetrad)
    {
        transferGrid(movingTetrad->topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, movingTetrad->shape, gridCols, gridRows,
                     gridData, movingTetrad->gridValue);
    }
}

int16_t xFromGridCol(int16_t x0, int16_t gridCol, uint8_t unitSize)
{
    return (x0 + 1) + (gridCol * unitSize);
}

int16_t yFromGridRow(int16_t y0, int16_t gridRow, uint8_t unitSize)
{
    return (y0 + 1) + (gridRow * unitSize);
}

// This assumes only complete tetrads can be rotated.
bool rotateTetrad(tetrad_t* tetrad, int32_t newRotation, uint8_t gridCols, uint8_t gridRows,
                                    uint32_t gridData[][gridCols])
{
    newRotation %= NUM_ROTATIONS;
    bool rotationClear = false;

    switch (tetrad->type)
    {
        case I_TETRAD:
            for (int32_t i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + iTetradRotationTests[tetrad->rotation][i].r;
                    testPoint.c = tetrad->topLeft.c + iTetradRotationTests[tetrad->rotation][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, iTetradRotations[newRotation], gridCols,
                                                    gridRows, gridData, tetrad->gridValue);
                    if (rotationClear)
                    {
                        tetrad->topLeft = testPoint;
                    }
                }
            }
            break;
        // The behavior here is such that an O tetrad can always be rotated, but that rotation does not effect anything, possibly only a semantic disctinction.
        case O_TETRAD:
            rotationClear = true;
            break;
        case T_TETRAD:
            for (int32_t i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + otjlszTetradRotationTests[tetrad->rotation][i].r;
                    testPoint.c = tetrad->topLeft.c + otjlszTetradRotationTests[tetrad->rotation][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tTetradRotations[newRotation], gridCols,
                                                    gridRows, gridData, tetrad->gridValue);
                    if (rotationClear)
                    {
                        tetrad->topLeft = testPoint;
                    }
                }
            }
            break;
        case J_TETRAD:
            for (int32_t i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + otjlszTetradRotationTests[tetrad->rotation][i].r;
                    testPoint.c = tetrad->topLeft.c + otjlszTetradRotationTests[tetrad->rotation][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, jTetradRotations[newRotation], gridCols,
                                                    gridRows, gridData, tetrad->gridValue);
                    if (rotationClear)
                    {
                        tetrad->topLeft = testPoint;
                    }
                }
            }
            break;
        case L_TETRAD:
            for (int32_t i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + otjlszTetradRotationTests[tetrad->rotation][i].r;
                    testPoint.c = tetrad->topLeft.c + otjlszTetradRotationTests[tetrad->rotation][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, lTetradRotations[newRotation], gridCols,
                                                    gridRows, gridData, tetrad->gridValue);
                    if (rotationClear)
                    {
                        tetrad->topLeft = testPoint;
                    }
                }
            }
            break;
        case S_TETRAD:
            for (int32_t i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + otjlszTetradRotationTests[tetrad->rotation][i].r;
                    testPoint.c = tetrad->topLeft.c + otjlszTetradRotationTests[tetrad->rotation][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, sTetradRotations[newRotation], gridCols,
                                                    gridRows, gridData, tetrad->gridValue);
                    if (rotationClear)
                    {
                        tetrad->topLeft = testPoint;
                    }
                }
            }
            break;
        case Z_TETRAD:
            for (int32_t i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + otjlszTetradRotationTests[tetrad->rotation][i].r;
                    testPoint.c = tetrad->topLeft.c + otjlszTetradRotationTests[tetrad->rotation][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, zTetradRotations[newRotation], gridCols,
                                                    gridRows, gridData, tetrad->gridValue);
                    if (rotationClear)
                    {
                        tetrad->topLeft = testPoint;
                    }
                }
            }
            break;
        default:
            break;
    }

    if (rotationClear)
    {
        // Actually rotate the tetrad.
        tetrad->rotation = newRotation;
        coord_t origin;
        origin.c = 0;
        origin.r = 0;
        switch (tetrad->type)
        {
            case I_TETRAD:
                copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, iTetradRotations[tetrad->rotation], TETRAD_GRID_SIZE,
                         TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case O_TETRAD:
                // Do nothing.
                break;
            case T_TETRAD:
                copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tTetradRotations[tetrad->rotation], TETRAD_GRID_SIZE,
                         TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case J_TETRAD:
                copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, jTetradRotations[tetrad->rotation], TETRAD_GRID_SIZE,
                         TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case L_TETRAD:
                copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, lTetradRotations[tetrad->rotation], TETRAD_GRID_SIZE,
                         TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case S_TETRAD:
                copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, sTetradRotations[tetrad->rotation], TETRAD_GRID_SIZE,
                         TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case Z_TETRAD:
                copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, zTetradRotations[tetrad->rotation], TETRAD_GRID_SIZE,
                         TETRAD_GRID_SIZE, tetrad->shape);
                break;
            default:
                break;
        }
    }
    return rotationClear;
}

void softDropTetrad()
{
    dropTimer += deltaTime * SOFT_DROP_FACTOR;
    dropFXTime += deltaTime * SOFT_DROP_FX_FACTOR;
}

bool moveTetrad(tetrad_t* tetrad, uint8_t gridCols, uint8_t gridRows,
                                  uint32_t gridData[][gridCols])
{
    // 0 = min top left
    // 9 = max top left
    // 3 is the center of top left in normal tetris

    bool moved = false;

    int32_t yMod = ttAccel.y / ACCEL_SEG_SIZE;

    coord_t targetPos;
    targetPos.r = tetrad->topLeft.r;
    targetPos.c = yMod + TETRAD_SPAWN_X;

    // Save the last accel, and if didn't change by a certain threshold, then don't recaculate the value.
    // Attempt to prevent jittering for gradual movements.
    if ((targetPos.c == tetrad->topLeft.c + 1 ||
            targetPos.c == tetrad->topLeft.c - 1) &&
            abs(ttAccel.y - ttLastTestAccel.y) <= ACCEL_JITTER_GUARD)
    {
        targetPos = tetrad->topLeft;
    }
    else
    {
        ttLastTestAccel = ttAccel;
    }

    // Emulator only (control with d pad)
    #ifdef EMU
    targetPos.c = tetrad->topLeft.c;
    //ESP_LOGW("EMU", "%d modeFrames", modeFrames);
    if (ttIsButtonPressed(LEFT)) 
    {
        targetPos.c -= 1;
    }
    else if (ttIsButtonPressed(RIGHT)) 
    {
        targetPos.c += 1;
    }
    #endif

    bool moveClear = true;
    while (targetPos.c != tetrad->topLeft.c && moveClear)
    {
        coord_t movePos = tetrad->topLeft;

        movePos.c = targetPos.c > movePos.c ? movePos.c + 1 : movePos.c - 1;

        if (checkCollision(movePos, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape, gridCols, gridRows, gridData,
                           tetrad->gridValue))
        {
            moveClear = false;
        }
        else
        {
            tetrad->topLeft = movePos;
            moved = true;
        }
    }
    return moved;
}

bool dropTetrad(tetrad_t* tetrad, uint8_t gridCols, uint8_t gridRows,
                                  uint32_t gridData[][gridCols])
{
    coord_t dropPos = tetrad->topLeft;
    dropPos.r++;
    bool dropSuccess = !checkCollision(dropPos, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape, gridCols, gridRows,
                                       gridData, tetrad->gridValue);

    // Move the tetrad down if it's clear to do so.
    if (dropSuccess)
    {
        tetrad->topLeft = dropPos;
    }
    return dropSuccess;
}

tetrad_t spawnTetrad(tetradType_t type, uint32_t gridValue, coord_t gridCoord, int32_t rotation)
{
    tetrad_t tetrad;
    tetrad.type = type;
    tetrad.gridValue = gridValue;
    tetrad.rotation = rotation;
    tetrad.topLeft = gridCoord;
    coord_t origin;
    origin.c = 0;
    origin.r = 0;
    switch (tetrad.type)
    {
        case I_TETRAD:
            copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, iTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                     tetrad.shape);
            break;
        case O_TETRAD:
            copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, oTetradRotations[0], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                     tetrad.shape);
            break;
        case T_TETRAD:
            copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                     tetrad.shape);
            break;
        case J_TETRAD:
            copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, jTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                     tetrad.shape);
            break;
        case L_TETRAD:
            copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, lTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                     tetrad.shape);
            break;
        case S_TETRAD:
            copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, sTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                     tetrad.shape);
            break;
        case Z_TETRAD:
            copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, zTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                     tetrad.shape);
            break;
        default:
            break;
    }
    return tetrad;
}

void spawnNextTetrad(tetrad_t* newTetrad, tetradRandomizer_t randomType, uint32_t currentTetradCount,
                                       uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols])
{
    coord_t spawnPos;
    spawnPos.c = TETRAD_SPAWN_X;
    spawnPos.r = TETRAD_SPAWN_Y;
    *newTetrad = spawnTetrad(nextTetradType, currentTetradCount + 1, spawnPos, TETRAD_SPAWN_ROT);
    nextTetradType = (tetradType_t)getNextTetradType(randomType, currentTetradCount);

    // Check if this is blocked, if it is, the game is over.
    if (checkCollision(newTetrad->topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, newTetrad->shape, gridCols, gridRows,
                       gridData, newTetrad->gridValue))
    {
        ttChangeState(TT_GAMEOVER);
    }
    // If the game isn't over, move the initial tetrad to where it should be based on the accelerometer.
    else
    {
        moveTetrad(newTetrad, gridCols, gridRows, gridData);
    }
}

// Lowest row on screen. (greatest value of r)
int32_t getLowestActiveRow(tetrad_t* tetrad)
{
    int32_t lowestRow = tetrad->topLeft.r;

    for (int32_t r = 0; r < TETRAD_GRID_SIZE; r++)
    {
        for (int32_t c = 0; c < TETRAD_GRID_SIZE; c++)
        {
            if (tetrad->shape[r][c] != EMPTY)
            {
                lowestRow = tetrad->topLeft.r + r;
            }
        }
    }

    return lowestRow;
}

// Highest row on screen. (greatest value of r)
int32_t getHighestActiveRow(tetrad_t* tetrad)
{
    int32_t highestRow = tetrad->topLeft.r;

    for (int32_t r = TETRAD_GRID_SIZE - 1; r <= 0; r--)
    {
        for (int32_t c = 0; c < TETRAD_GRID_SIZE; c++)
        {
            if (tetrad->shape[r][c] != EMPTY)
            {
                highestRow = tetrad->topLeft.r + r;
            }
        }
    }

    return highestRow;
}

int32_t getFallDistance(tetrad_t* tetrad, uint8_t gridCols, uint8_t gridRows,
        const uint32_t gridData[][gridCols])
{
    int32_t fallDistance = gridRows;
    int32_t currFallDistance;
    int32_t searchCol;
    int32_t lowestActiveRowInCol;

    // Search through every column the tetrad can occupy space in.
    for (int32_t c = 0; c < TETRAD_GRID_SIZE; c++)
    {
        // Find the lowest (closest to the floor of the playfield) occupied row in this column for the tetrad.
        lowestActiveRowInCol = -1;
        for (int32_t r = 0; r < TETRAD_GRID_SIZE; r++)
        {
            if (tetrad->shape[r][c] != EMPTY)
            {
                lowestActiveRowInCol = tetrad->topLeft.r + r;
            }
        }

        // If any space in that tetrad was occupied.
        if (lowestActiveRowInCol != -1)
        {
            searchCol = tetrad->topLeft.c + c;

            currFallDistance = gridRows - lowestActiveRowInCol - 1;

            // If no occupied spaces still reassign fall distance if closer than full fall height
            if (currFallDistance < fallDistance)
            {
                fallDistance = currFallDistance;
            }

            // Check grid spaces on rows below tetrad.
            for (int32_t gr = lowestActiveRowInCol + 1; gr < gridRows; gr++)
            {
                currFallDistance = (gr - lowestActiveRowInCol - 1);

                // If occupied by other tetrad, this is where
                if (gridData[gr][searchCol] != EMPTY &&
                        gridData[gr][searchCol] != tetrad->gridValue &&
                        currFallDistance < fallDistance)
                {
                    fallDistance = currFallDistance;
                }
            }
        }
    }
    return fallDistance;
}


void plotSquare(display_t* disp, int16_t x0, int16_t y0, uint8_t size, paletteColor_t col)
{
    plotRect(disp, x0, y0, x0 + (size - 1), y0 + (size - 1), col);
}

void plotGrid(display_t* disp, int16_t x0, int16_t y0, uint8_t unitSize, uint8_t gridCols, uint8_t gridRows,
                                uint32_t gridData[][gridCols], bool clearLineAnimation, paletteColor_t col)
{
    // Draw the border
    // TODO: the +2 moves the border down so that distinct tetrads don't clip ground.
    plotRect(disp, x0, y0, x0 + (unitSize * gridCols) + 2, y0 + (unitSize * gridRows) + 2, col);

    // Draw points for grid (maybe disable when not debugging)
    for (int32_t y = 0; y < gridRows; y++)
    {
        // Draw lines that are cleared.
        if (clearLineAnimation && isLineCleared(y, gridCols, gridRows, gridData))
        {
            fillDisplayArea(display, x0 + 1, y0 + (unitSize * y) + 1, x0 + (unitSize * gridCols), y0 + (unitSize * (y + 1)) + 1, c555);
        }

        for (int32_t x = 0; x < gridCols; x++)
        {
            //plotSquare(x0 + (x * unitSize) + 1, y0 + (y * unitSize) + 1, unitSize, c555);
            // Draw a centered pixel on empty grid units.
            //if (gridData[y][x] == EMPTY) disp->setPx(x0 + x * unitSize + (unitSize / 2), y0 + y * unitSize + (unitSize / 2), c555);
        }
    }
}

void plotTetrad(display_t* disp, int16_t x0, int16_t y0, uint8_t unitSize, uint8_t tetradCols, uint8_t tetradRows,
                                  uint32_t shape[][tetradCols], uint8_t tetradFill, int32_t fillRotation, paletteColor_t borderColor, paletteColor_t fillColor)
{
    bool patternRotated = fillRotation % 2 != 0;
    for (int32_t y = 0; y < tetradRows; y++)
    {
        for (int32_t x = 0; x < tetradCols; x++)
        {
            if (shape[y][x] != EMPTY)
            {
                // The top left of this unit.
                int16_t px = x0 + x * unitSize;
                int16_t py = y0 + y * unitSize;
                fillDisplayArea(disp, px, py, px + unitSize, py + unitSize, fillColor);
                switch (tetradFill)
                {
                    case I_TETRAD:
                        // thatch
                        /*disp->setPx(px + 1, py + 1, col);
                        disp->setPx(px + (unitSize - 2), py + 1, col);
                        disp->setPx(px + 1, py + (unitSize - 2), col);
                        disp->setPx(px + (unitSize - 2), py + (unitSize - 2), col);*/
                        // diagonals both
                        plotLine(disp, px, py, px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                        plotLine(disp, px, py + (unitSize - 1), px + (unitSize - 1), py, borderColor, 0);
                        break;
                    case O_TETRAD:
                        // full walls and center dots.
                        disp->setPx(px + (unitSize / 2), py + (unitSize / 2), borderColor);
                        plotSquare(disp, px, py, unitSize, borderColor);
                        break;
                    case T_TETRAD:
                        // internal border
                        //top
                        if (y == 0 || shape[y - 1][x] == EMPTY)
                        {
                            plotLine(disp, px, py + 1, px + (unitSize - 1), py + 1, borderColor, 0);
                        }
                        else
                        {
                            plotSquare(disp, px, py, 2, borderColor);
                            plotSquare(disp, px + (unitSize - 1) - 1, py, 2, borderColor);
                        }
                        //bot
                        if (y == tetradRows - 1 || shape[y + 1][x] == EMPTY)
                        {
                            plotLine(disp, px, py + (unitSize - 1) - 1, px + (unitSize - 1), py + (unitSize - 1) - 1, borderColor, 0);
                        }
                        else
                        {
                            plotSquare(disp, px, py + (unitSize - 1) - 1, 2, borderColor);
                            plotSquare(disp, px + (unitSize - 1) - 1, py + (unitSize - 1) - 1, 2, borderColor);
                        }

                        //left
                        if (x == 0 || shape[y][x - 1] == EMPTY)
                        {
                            plotLine(disp, px + 1, py, px + 1, py + (unitSize - 1), borderColor, 0);
                        }
                        else
                        {
                            plotSquare(disp, px, py, 2, borderColor);
                            plotSquare(disp, px, py + (unitSize - 1) - 1, 2, borderColor);
                        }

                        //right
                        if (x == tetradCols - 1 || shape[y][x + 1] == EMPTY)
                        {
                            plotLine(disp, px + (unitSize - 1) - 1, py, px + (unitSize - 1) - 1, py + (unitSize - 1), borderColor, 0);
                        }
                        else
                        {
                            plotSquare(disp, px + (unitSize - 1) - 1, py, 2, borderColor);
                            plotSquare(disp, px + (unitSize - 1) - 1, py + (unitSize - 1) - 1, 2, borderColor);
                        }

                        break;
                    case J_TETRAD:
                        // diagonals up
                        if (patternRotated)
                        {
                            plotLine(disp, px, py, px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                        }
                        else
                        {
                            plotLine(disp, px, py + (unitSize - 1), px + (unitSize - 1), py, borderColor, 0);
                        }
                        break;
                    case L_TETRAD:
                        // diagonals down
                        if (patternRotated)
                        {
                            plotLine(disp, px, py + (unitSize - 1), px + (unitSize - 1), py, borderColor, 0);
                        }
                        else
                        {
                            plotLine(disp, px, py, px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                        }
                        break;
                    case S_TETRAD:
                        // diagonals up
                        if (patternRotated)
                        {
                            //plotLine(disp, px + (unitSize / 2), py, px + (unitSize), py + (unitSize / 2), c000, 0);
                            plotLine(disp, px, py, px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                            //plotLine(disp, px, py, px + (unitSize / 2), py + (unitSize), c555, 0);
                        }
                        else
                        {
                            //plotLine(disp, px, py + (unitSize / 2) - 1, px + (unitSize / 2) - 1, py, c000, 0);
                            plotLine(disp, px, py + (unitSize - 1), px + (unitSize - 1), py, borderColor, 0);
                            //plotLine(disp, px + (unitSize / 2), py + (unitSize - 1), px + (unitSize - 1), py + (unitSize / 2) - 1, c555, 0);
                        }
                        break;
                    case Z_TETRAD:
                        // diagonals down
                        if (patternRotated)
                        {
                            plotLine(disp, px, py + (unitSize - 1), px + (unitSize - 1), py, borderColor, 0);
                        }
                        else
                        {
                            plotLine(disp, px, py, px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                        }
                        break;
                    // If empty or unrecognized fill, do nothing.
                    case EMPTY:
                    default:
                        break;
                }

                //top
                if (y == 0 || shape[y - 1][x] == EMPTY)
                {
                    plotLine(disp, px, py, px + (unitSize - 1), py, borderColor, 0);
                }
                else
                {
                    disp->setPx(px, py, borderColor);
                    disp->setPx(px + (unitSize - 1), py, borderColor);
                }

                //bot
                if (y == tetradRows - 1 || shape[y + 1][x] == EMPTY)
                {
                    plotLine(disp, px, py + (unitSize - 1), px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                }
                else
                {
                    disp->setPx(px, py + (unitSize - 1), borderColor);
                    disp->setPx(px + (unitSize - 1), py + (unitSize - 1), borderColor);
                }

                //left
                if (x == 0 || shape[y][x - 1] == EMPTY)
                {
                    plotLine(disp, px, py, px, py + (unitSize - 1), borderColor, 0);
                }
                else
                {
                    disp->setPx(px, py, borderColor);
                    disp->setPx(px, py + (unitSize - 1), borderColor);
                }

                //right
                if (x == tetradCols - 1 || shape[y][x + 1] == EMPTY)
                {
                    plotLine(disp, px + (unitSize - 1), py, px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                }
                else
                {
                    disp->setPx(px + (unitSize - 1), py, borderColor);
                    disp->setPx(px + (unitSize - 1), py, borderColor);
                }
            }
        }
    }
}

void plotPerspectiveEffect(display_t* disp, int16_t leftSrc, int16_t leftDst, int16_t rightSrc, int16_t rightDst,
        int16_t y0, int16_t y1, int32_t numVerticalLines, int32_t numHorizontalLines, double lineTweenTimeS,
        uint32_t currentTimeUS,
        paletteColor_t col)
{
    // Drawing some fake 3D demo-scene like lines for effect.

    // Vertical moving lines.
    for (int32_t i = 0; i < numVerticalLines; i++)
    {
        // TODO: Use Bill's equation.
        int32_t lineOffset = ((lineTweenTimeS * i) / numVerticalLines) * S_TO_MS_FACTOR * MS_TO_US_FACTOR;
        int32_t lineProgressUS = (currentTimeUS + lineOffset) % (int)(lineTweenTimeS * S_TO_MS_FACTOR * MS_TO_US_FACTOR);
        double lineProgress = (double)lineProgressUS / (double)(lineTweenTimeS * S_TO_MS_FACTOR * MS_TO_US_FACTOR);
        lineProgress *= lineProgress;

        uint8_t leftLineXProgress = lineProgress * (leftSrc - leftDst);
        uint8_t rightLineXProgress = lineProgress * (rightDst - rightSrc);
        plotLine(disp, leftSrc - leftLineXProgress, y0, leftSrc - leftLineXProgress, y1, col, 0);
        plotLine(disp, rightSrc + rightLineXProgress, y0, rightSrc + rightLineXProgress, y1, col, 0);
    }

    // Horizontal static lines.
    // TODO: this placement code doesn't handle the dst ys correctly for values of numHorizontalLines that aren't 3
    uint8_t lineSpace = (y1 - y0) / (numHorizontalLines + 1);
    bool oddLines = numHorizontalLines % 2 != 0;
    for (int32_t i = 0; i < numHorizontalLines; i++)
    {
        uint8_t lineSrcY = y0 + (lineSpace * (i + 1));
        uint8_t lineDstY = lineSrcY >= ((y1 - y0) / 2) ? lineSrcY + lineSpace : lineSrcY - lineSpace;
        if (oddLines && i == numHorizontalLines / 2)
        {
            lineDstY = lineSrcY;
        }

        plotLine(disp, leftSrc, lineSrcY, leftDst, lineDstY, col, 0);
        plotLine(disp, rightSrc, lineSrcY, rightDst, lineDstY, col, 0);
    }
}

uint16_t getCenteredTextX(font_t* font, const char* text, int16_t x0, int16_t x1)
{
    uint16_t txtWidth = textWidth(font, text);

    // Calculate the correct x to draw from.
    uint16_t fullWidth = x1 - x0 + 1;
    // NOTE: This may result in strange behavior when the width of the drawn text is greater than the distance between x0 and x1.
    uint16_t widthDiff = fullWidth - txtWidth;
    uint16_t centeredX = x0 + (widthDiff / 2);
    return centeredX;
}

void getNumCentering(font_t* font, const char* text, int16_t achorX0, int16_t anchorX1, int16_t* textX0, int16_t* textX1)
{
    uint8_t txtWidth = textWidth(font, text);

    // Calculate the correct x to draw from.
    uint8_t fullWidth = anchorX1 - achorX0 + 1;
    // NOTE: This may result in strange / undefined behavior when the width of the drawn text is greater than the distance between achorX0 and anchorX1.
    uint8_t widthDiff = fullWidth - txtWidth;
    *textX0 = achorX0 + (widthDiff / 2);
    *textX1 = *textX0 + (txtWidth - 1);
}

void initTypeOrder()
{
    typeOrder = malloc(sizeof(list_t));
    typeOrder->first = NULL;
    typeOrder->last = NULL;
    typeOrder->length = 0;
}

void clearTypeOrder()
{
    // Free all ints in the list.
    node_t* current = typeOrder->first;
    while (current != NULL)
    {
        free(current->val);
        current->val = NULL;
        current = current->next;
    }
    // Free the node containers for the list.
    clear(typeOrder);
}

void deInitTypeOrder()
{
    clearTypeOrder();

    // Finally free the list itself.
    free(typeOrder);
    typeOrder = NULL;
}

void initTetradRandomizer(tetradRandomizer_t randomType)
{
    switch (randomType)
    {
        case RANDOM:
            break;
        case BAG:
            bagIndex = 0;
            shuffle(NUM_TETRAD_TYPES, typeBag);
            break;
        case POOL:
        {
            // Initialize the tetrad type pool, 5 of each type.
            for (int32_t i = 0; i < 5; i++)
            {
                for (int32_t j = 0; j < NUM_TETRAD_TYPES; j++)
                {
                    typePool[i * NUM_TETRAD_TYPES + j] = j + 1;
                }
            }

            // Clear the history.
            for (int32_t i = 0; i < 4; i++)
            {
                typeHistory[i] = 0;
            }

            // Populate the history with initial values.
            typeHistory[0] = S_TETRAD;
            typeHistory[1] = Z_TETRAD;
            typeHistory[2] = S_TETRAD;

            // Clear the order list.
            clearTypeOrder();
        }
        break;
        default:
            break;
    }
}

int32_t getNextTetradType(tetradRandomizer_t randomType, int32_t index)
{
    int32_t nextType = EMPTY;
    switch (randomType)
    {
        case RANDOM:
            nextType = (rand() % NUM_TETRAD_TYPES) + 1;
            break;
        case BAG:
            nextType = typeBag[bagIndex];
            bagIndex++;
            if (bagIndex >= NUM_TETRAD_TYPES)
            {
                initTetradRandomizer(randomType);
            }
            break;
        case POOL:
        {
            // First piece special conditions.
            if (index == 0)
            {
                nextType = firstType[rand() % 4];
                typeHistory[3] = nextType;
            }
            else
            {
                // The pool index of the next piece.
                int32_t i;

                // Roll for piece.
                for (int32_t r = 0; r < 6; r++)
                {
                    i = rand() % 35;
                    nextType = typePool[i];

                    bool inHistory = false;
                    for (int32_t h = 0; h < 4; h++)
                    {
                        if (typeHistory[h] == nextType)
                        {
                            inHistory = true;
                        }
                    }

                    if (!inHistory || r == 5)
                    {
                        break;
                    }

                    if (typeOrder->length > 0)
                    {
                        typePool[i] = *((int*)typeOrder->first->val);
                    }
                }

                // Update piece order.
                node_t* current = typeOrder->last;
                for (int32_t j = typeOrder->length - 1; j >= 0; j--)
                {
                    // Get the current value.
                    int* currentType = (int*)current->val;

                    // Update current in case we remove this node.
                    current = current->prev;

                    // Remove this node and free its value if it matches.
                    if (*currentType == nextType)
                    {
                        free(removeIdx(typeOrder, j));
                    }
                }
                int* newOrderType = malloc(sizeof(int));
                *newOrderType = nextType;
                push(typeOrder, newOrderType);

                typePool[i] = *((int*)typeOrder->first->val);

                // Update history.
                for (int32_t h = 0; h < 4; h++)
                {
                    if (h == 3)
                    {
                        typeHistory[h] = nextType;
                    }
                    else
                    {
                        typeHistory[h] = typeHistory[h + 1];
                    }
                }
            }
        }
        break;
        default:
            break;
    }
    return nextType;
}

// Fisher–Yates Shuffle
void shuffle(int32_t length, int32_t array[length])
{
    for (int32_t i = length - 1; i > 0; i--)
    {
        // Pick a random index from 0 to i
        int32_t j = rand() % (i + 1);

        // Swap array[i] with the element at random index
        int32_t temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

void loadHighScores(void)
{
    ttGetHighScores();
}

void saveHighScores(void)
{
    ttSetHighScores();
}

bool updateHighScores(uint32_t newScore)
{
    bool highScore = false;
    uint32_t placeScore = newScore;
    for (int32_t i = 0; i < NUM_TT_HIGH_SCORES; i++)
    {
        // Get the current score at this index.
        uint32_t currentScore = highScores[i];

        if (placeScore >= currentScore)
        {
            highScores[i] = placeScore;
            placeScore = currentScore;
            highScore = true;
        }
    }
    return highScore;
}

void ttGetHighScores(void)
{
    char keyStr[32] = {0};
    for (int32_t i = 0; i < NUM_TT_HIGH_SCORES; i++)
    {
        snprintf(keyStr, sizeof(keyStr), "tt_high_score_%d", i);
        if (!readNvs32(keyStr, &highScores[i]))
        {
            highScores[i] = 0;
        }
    }
}

void ttSetHighScores(void)
{
    char keyStr[32] = {0};
    for (int32_t i = 0; i < NUM_TT_HIGH_SCORES; i++) 
    {
        snprintf(keyStr, sizeof(keyStr), "tt_high_score_%d", i);
        writeNvs32(keyStr, highScores[i]);
    }
}

/*uint32_t ttGetLastScore(void)
{
    return settings.ttLastScore;
}

void ttSetLastScore(uint32_t newLastScore)
{
    settings.ttLastScore = newLastScore;
    SaveSettings();
}*/

void initLandedTetrads()
{
    landedTetrads = malloc(sizeof(list_t));
    landedTetrads->first = NULL;
    landedTetrads->last = NULL;
    landedTetrads->length = 0;
}

void clearLandedTetrads()
{
    // Free all tetrads in the list.
    node_t* current = landedTetrads->first;
    while (current != NULL)
    {
        free(current->val);
        current->val = NULL;
        current = current->next;
    }
    // Free the node containers for the list.
    clear(landedTetrads);
}

void deInitLandedTetrads()
{
    clearLandedTetrads();

    // Finally free the list itself.
    free(landedTetrads);
    landedTetrads = NULL;
}

void startClearAnimation(int32_t numLineClears __attribute__((unused)))
{
    inClearAnimation = true;
    clearTimer = 0;
    clearTime = CLEAR_LINES_ANIM_TIME;

    singlePulseLEDs(NUM_LEDS, clearColor, 0.0);
}

void stopClearAnimation()
{
    inClearAnimation = false;
    clearTimer = 0;
    clearTime = 0;

    singlePulseLEDs(NUM_LEDS, clearColor, 1.0);
}

uint32_t getDropTime(uint32_t level)
{
    uint32_t dropTimeFrames = 0;

    switch (level)
    {
        case 0:
            dropTimeFrames = 48;
            break;
        case 1:
            dropTimeFrames = 43;
            break;
        case 2:
            dropTimeFrames = 38;
            break;
        case 3:
            dropTimeFrames = 33;
            break;
        case 4:
            dropTimeFrames = 28;
            break;
        case 5:
            dropTimeFrames = 23;
            break;
        case 6:
            dropTimeFrames = 18;
            break;
        case 7:
            dropTimeFrames = 13;
            break;
        case 8:
            dropTimeFrames = 8;
            break;
        case 9:
            dropTimeFrames = 6;
            break;
        case 10:
        case 11:
        case 12:
            dropTimeFrames = 5;
            break;
        case 13:
        case 14:
        case 15:
            dropTimeFrames = 4;
            break;
        case 16:
        case 17:
        case 18:
            dropTimeFrames = 3;
            break;
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
            dropTimeFrames = 2;
            break;
        case 29:
            dropTimeFrames = 1;
            break;
        default:
            break;
    }

    // We need the time in microseconds.
    return dropTimeFrames * UPDATE_TIME_MS * MS_TO_US_FACTOR;
}

double getDropFXTimeFactor(uint32_t level)
{
    double dropFXTimeFactor = 0;

    switch (level)
    {
        case 0:
            dropFXTimeFactor = 0;
            break;
        case 1:
            dropFXTimeFactor = 0.25;
            break;
        case 2:
            dropFXTimeFactor = 0.5;
            break;
        case 3:
            dropFXTimeFactor = 0.75;
            break;
        case 4:
            dropFXTimeFactor = 1;
            break;
        case 5:
            dropFXTimeFactor = 1.25;
            break;
        case 6:
            dropFXTimeFactor = 1.5;
            break;
        case 7:
            dropFXTimeFactor = 1.75;
            break;
        case 8:
            dropFXTimeFactor = 2;
            break;
        case 9:
            dropFXTimeFactor = 2.25;
            break;
        case 10:
        case 11:
        case 12:
            dropFXTimeFactor = 2.75;
            break;
        case 13:
        case 14:
        case 15:
            dropFXTimeFactor = 3.25;
            break;
        case 16:
        case 17:
        case 18:
            dropFXTimeFactor = 3.75;
            break;
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
            dropFXTimeFactor = 4.75;
            break;
        case 29:
            dropFXTimeFactor = 5;
            break;
        default:
            break;
    }

    return dropFXTimeFactor;
}

bool isLineCleared(int32_t line, uint8_t gridCols, uint8_t gridRows __attribute__((unused)),
                                     uint32_t gridData[][gridCols])
{
    bool clear = true;
    for (int32_t c = 0; c < gridCols; c++)
    {
        if (gridData[line][c] == EMPTY)
        {
            clear = false;
        }
    }
    return clear;
}

int32_t checkLineClears(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols],
        list_t* fieldTetrads)
{
    //Refresh the tetrads grid before checking for any clears.
    refreshTetradsGrid(gridCols, gridRows, gridData, fieldTetrads, NULL, false);

    int32_t lineClears = 0;

    int32_t currRow = gridRows - 1;

    // Go through every row bottom-to-top.
    while (currRow >= 0)
    {
        if (isLineCleared(currRow, gridCols, gridRows, gridData))
        {
            lineClears++;
        }
        currRow--;
    }

    return lineClears;
}

int32_t clearLines(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols],
                                     list_t* fieldTetrads)
{
    //Refresh the tetrads grid before checking for any clears.
    refreshTetradsGrid(gridCols, gridRows, gridData, fieldTetrads, NULL, false);

    int32_t lineClears = 0;

    int32_t currRow = gridRows - 1;

    // Go through every row bottom-to-top.
    while (currRow >= 0)
    {
        if (isLineCleared(currRow, gridCols, gridRows, gridData))
        {
            lineClears++;

            node_t* current = fieldTetrads->last;
            // Update the positions of compositions of any effected tetrads.
            for (int32_t t = fieldTetrads->length - 1; t >= 0; t--)
            {
                tetrad_t* currentTetrad = (tetrad_t*)current->val;
                bool aboveClear = true;

                // Go from bottom-to-top on each position of the tetrad.
                for (int32_t tr = TETRAD_GRID_SIZE - 1; tr >= 0; tr--)
                {
                    for (int32_t tc = 0; tc < TETRAD_GRID_SIZE; tc++)
                    {
                        // Check where we are on the grid.
                        coord_t gridPos;
                        gridPos.r = currentTetrad->topLeft.r + tr;
                        gridPos.c = currentTetrad->topLeft.c + tc;

                        // If any part of the tetrad (even empty) exists at the clear line, don't adjust its position downward.
                        if (gridPos.r >= currRow)
                        {
                            aboveClear = false;
                        }

                        // If something exists at that position...
                        if (!aboveClear && currentTetrad->shape[tr][tc] != EMPTY)
                        {
                            // Completely remove tetrad pieces on the cleared row.
                            if (gridPos.r == currRow)
                            {
                                currentTetrad->shape[tr][tc] = EMPTY;
                            }
                            // Move all the pieces of tetrads that are above the cleared row down by one.
                            else if (gridPos.r < currRow)
                            {
                                //NOTE: What if it cannot be moved down anymore in its local grid? Can this happen? I don't think that's possible.
                                if (tr < TETRAD_GRID_SIZE - 1)
                                {
                                    // Copy the current space into the space below it.
                                    currentTetrad->shape[tr + 1][tc] = currentTetrad->shape[tr][tc];

                                    // Empty the current space.
                                    currentTetrad->shape[tr][tc] = EMPTY;
                                }
                            }
                        }
                    }
                }

                // Move tetrads entirely above the cleared line down by one.
                if (aboveClear && currentTetrad->topLeft.r < currRow)
                {
                    currentTetrad->topLeft.r++;
                }

                // Adjust the current counter.
                current = current->prev;
            }

            // Before we check against the gridData of all tetrads again, we need to rebuilt an accurate version.
            refreshTetradsGrid(gridCols, gridRows, gridData, fieldTetrads, NULL, false);
        }
        else
        {
            currRow--;
        }
    }

    return lineClears;
}

// what is the best way to handle collisions above the grid space?
bool checkCollision(coord_t newPos, uint8_t tetradCols, uint8_t tetradRows __attribute__((unused)),
                                      const uint32_t shape[][tetradCols], uint8_t gridCols, uint8_t gridRows, const uint32_t gridData[][gridCols],
                                      uint32_t selfGridValue)
{
    for (int32_t r = 0; r < TETRAD_GRID_SIZE; r++)
    {
        for (int32_t c = 0; c < TETRAD_GRID_SIZE; c++)
        {
            if (shape[r][c] != EMPTY)
            {
                if (newPos.r + r >= gridRows ||
                        newPos.c + c >= gridCols ||
                        newPos.c + c < 0 ||
                        (gridData[newPos.r + r][newPos.c + c] != EMPTY &&
                         gridData[newPos.r + r][newPos.c + c] != selfGridValue)) // Don't check collision with yourself.
                {
                    return true;
                }
            }
        }
    }
    return false;
}

// a color is puled all leds according to the type of clear.
void singlePulseLEDs(uint8_t numLEDs, led_t fxColor, double progress)
{
    double lightness = 1.0 - (progress * progress);

    for (int32_t i = 0; i < numLEDs; i++)
    {
        leds[i].r = (uint8_t)((double)fxColor.r * lightness);
        leds[i].g = (uint8_t)((double)fxColor.g * lightness);
        leds[i].b = (uint8_t)((double)fxColor.b * lightness);
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(leds, sizeof(leds));
}

// blink red in sync with OLED gameover FX.
void blinkLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time)
{
    //TODO: there are instances where the red flashes on the opposite of the fill draw, how to ensure this does not happen?
    uint32_t animCycle = ((double)time * US_TO_MS_FACTOR) / DISPLAY_REFRESH_MS;
    bool lightActive = animCycle % 2 == 0;

    for (int32_t i = 0; i < numLEDs; i++)
    {
        leds[i].r = lightActive ? fxColor.r : 0x00;
        leds[i].g = lightActive ? fxColor.g : 0x00;
        leds[i].b = lightActive ? fxColor.b : 0x00;
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(leds, sizeof(leds));
}

// alternate lit up like a bulb sign
void alternatingPulseLEDS(uint8_t numLEDs, led_t fxColor, uint32_t time)
{
    double timeS = (double)time * US_TO_MS_FACTOR * MS_TO_S_FACTOR;
    double risingProgress = (sin(timeS * 4.0) + 1.0) / 2.0;
    double fallingProgress = 1.0 - risingProgress;

    double risingR = risingProgress * (double)fxColor.r;
    double risingG = risingProgress * (double)fxColor.g;
    double risingB = risingProgress * (double)fxColor.b;

    double fallingR = fallingProgress * (double)fxColor.r;
    double fallingG = fallingProgress * (double)fxColor.g;
    double fallingB = fallingProgress * (double)fxColor.b;

    bool risingLED;

    for (int32_t i = 0; i < numLEDs; i++)
    {
        risingLED = i % 2 == 0;
        leds[i].r = risingLED ? (uint8_t)risingR : (uint8_t)fallingR;
        leds[i].g = risingLED ? (uint8_t)risingG : (uint8_t)fallingG;
        leds[i].b = risingLED ? (uint8_t)risingB : (uint8_t)fallingB;
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(leds, sizeof(leds));
}

// radial wanderers.
void dancingLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time)
{
    uint32_t animCycle = ((double)time * US_TO_MS_FACTOR * 2.0) / DISPLAY_REFRESH_MS;
    int32_t firstIndex = animCycle % numLEDs;
    int32_t secondIndex = (firstIndex + (numLEDs / 2)) % numLEDs;

    //uint8_t timeMS = ((double)time * US_TO_MS_FACTOR)/400;

    for (int32_t i = 0; i < numLEDs; i++)
    {
        leds[i].r = i == firstIndex || i == secondIndex ? fxColor.r : 0x00;
        leds[i].g = i == firstIndex || i == secondIndex ? fxColor.g : 0x00;
        leds[i].b = i == firstIndex || i == secondIndex ? fxColor.b : 0x00;
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(leds, sizeof(leds));
}

void countdownLEDs(uint8_t numLEDs, led_t fxColor, double progress)
{
    // Reverse the direction of progress.
    progress = 1.0 - progress;

    // How many LEDs will be fully lit.
    uint8_t numLitLEDs = progress * numLEDs;

    // Get the length of each segment of progress.
    double segment = 1.0 / numLEDs;
    double segmentProgress = numLitLEDs * segment;
    // Find the amount that the leading LED should be partially lit.
    double modProgress = (progress - segmentProgress) / segment;

    for (int32_t i = 0; i < numLEDs; i++)
    {
        if (i < numLitLEDs)
        {
            leds[i].r = fxColor.r;
            leds[i].g = fxColor.g;
            leds[i].b = fxColor.b;
        }
        else if (i == numLitLEDs)
        {
            leds[i].r = (uint8_t)((double)fxColor.r * modProgress);
            leds[i].g = (uint8_t)((double)fxColor.g * modProgress);
            leds[i].b = (uint8_t)((double)fxColor.b * modProgress);
        }
        else
        {
            leds[i].r = 0x00;
            leds[i].g = 0x00;
            leds[i].b = 0x00;
        }
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(leds, sizeof(leds));
}

void clearLEDs(uint8_t numLEDs)
{
    for (int32_t i = 0; i < numLEDs; i++)
    {
        leds[i].r = 0x00;
        leds[i].g = 0x00;
        leds[i].b = 0x00;
    }

    setLeds(leds, sizeof(leds));
}

void applyLEDBrightness(uint8_t numLEDs, double brightness)
{
    // Best way would be to convert to HSV and then set, is this factor method ok?

    for (int32_t i = 0; i < numLEDs; i++)
    {
        leds[i].r = (uint8_t)((double)leds[i].r * brightness);
        leds[i].g = (uint8_t)((double)leds[i].g * brightness);
        leds[i].b = (uint8_t)((double)leds[i].b * brightness);
    }
}
