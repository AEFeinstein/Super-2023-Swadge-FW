/*
*   mode_tiltrads.c
*
*   Created on: Aug 2, 2019
*   Ported to 2023 Swadge: August 27, 2022
*       Author: Jonathan Moriarty
*/

#include <stdlib.h> // calloc
#include <stdio.h> // snprintf
#include <math.h> // sin
#include <string.h>// memset

#include "swadgeMode.h"  // Swadge mode
#include "mode_main_menu.h" // Return to main menu
#include "mode_tiltrads.h"
#include "esp_timer.h" // Timer functions
#include "esp_log.h" // Debug logging functions
#include "display.h" // Display functions and draw text
#include "bresenham.h"  // Draw shapes
#include "linked_list.h" // Custom linked list
#include "nvs_manager.h" // Saving and loading high scores and last scores
#include "musical_buzzer.h" // Music and SFX
#include "led_util.h" // LEDs

// NOTES:
// Decided not to handle cascade clears that result from falling tetrads after clears. Closer to target behavior.


// Any defines go here.

//#define NO_STRESS_TRIS // Debug mode that when enabled, stops tetrads from dropping automatically, they will only drop when the drop button is pressed. Useful for testing line clears.

// Controls (title)
#define BTN_TITLE_START_SCORES SELECT
#define BTN_TITLE_START_GAME START
#define BTN_TITLE_START_GAME_ALT BTN_A
//#define BTN_TITLE_EXIT_MODE BTN_B

// Controls (game)
#define BTN_GAME_SOFT_DROP DOWN
#define BTN_GAME_HARD_DROP UP
#define BTN_GAME_ROTATE_CW BTN_A
#define BTN_GAME_ROTATE_ACW BTN_B
#define BTN_GAME_PAUSE START

// Controls (scores)
#define BTN_SCORES_CLEAR_SCORES SELECT
#define BTN_SCORES_START_TITLE START

// Controls (gameover)
#define BTN_GAMEOVER_START_TITLE SELECT
#define BTN_GAMEOVER_START_GAME START
#define BTN_GAMEOVER_START_GAME_ALT BTN_A

// Update task info
#define UPDATE_TIME_MS 16
#define DISPLAY_REFRESH_MS 400 // This is a best guess for syncing LED FX with OLED FX.

// Time info
#define MS_TO_US_FACTOR 1000
#define S_TO_MS_FACTOR 1000
#define US_TO_MS_FACTOR 0.001
#define MS_TO_S_FACTOR 0.001

// Useful display
#define DISPLAY_HALF_HEIGHT 120

// Title screen
#define TUTORIAL_GRID_COLS 10
#define TUTORIAL_GRID_ROWS 26

#define TITLE_LEVEL 5 // The level used for calculating drop speed on the title screen.

//#define EXIT_MODE_HOLD_TIME (2 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

// Score screen
#define SCORE_SCREEN_TITLE_Y 30
#define SCORE_SCREEN_SCORE_Y 40
#define CLEAR_SCORES_HOLD_TIME (5 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define NUM_TT_HIGH_SCORES 10 // Track this many highest scores.

// Game screen
#define EMPTY 0
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

// Playfield
// 240 x 280
#define GRID_X 80
#define GRID_Y -14
#define GRID_UNIT_SIZE 12
#define GRID_COLS 10
#define GRID_ROWS 21

#define NEXT_GRID_X 208
#define NEXT_GRID_Y 40
#define NEXT_GRID_COLS 5
#define NEXT_GRID_ROWS 5

// Scoring, all of these are (* level)
#define SCORE_SINGLE 100
#define SCORE_DOUBLE 300
#define SCORE_TRIPLE 500
#define SCORE_QUAD 800
// This is per cell
#define SCORE_SOFT_DROP 1
#define SCORE_HARD_DROP 2
// This is (* count * level)
#define SCORE_COMBO 50

// Difficulty scaling
#define LINE_CLEARS_PER_LEVEL 5

// LED FX
#define MODE_LED_BRIGHTNESS 0.125 // Factor that decreases overall brightness of LEDs since they are a little distracting at full brightness.

// Music and SFX
#define NUM_LAND_FX 16

// Any typedefs go here.

// Mode state
typedef enum
{
    TT_TITLE,   // Title screen
    TT_GAME,    // Play the actual game
    TT_SCORES,  // High scores
    TT_GAMEOVER // Game over
} tiltradsState_t;

// Type of randomization behavior
typedef enum
{
    RANDOM, // Pure random
    BAG,    // 7 Bag
    POOL    // 35 Pool with 6 rolls
} tetradRandomizer_t;

// Type of tetrad
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

// Tetrad colors
// Tetrad color pairs:
// border, fill

// magenta   c505, c303
// orange    c530, c410

// ice-blue  c335, c113
// yellow    c540, c321

// red       c500, c300
// green     c452, c032

// white     c555, c333
// blue      c112, c001
const paletteColor_t borderColors[NUM_TETRAD_TYPES] =
{
    c555, // white
    c500, // red
    c505, // magenta
    c335, // ice-blue
    c540, // yellow
    c530, // orange
    c452  // green
};

const paletteColor_t fillColors[NUM_TETRAD_TYPES] =
{
    c333, // white
    c300, // red
    c303, // magenta
    c113, // ice-blue
    c321, // yellow
    c420, // orange
    c032  // green
};

// Coordinates on the playfield grid, not the screen.
typedef struct
{
    int16_t c;
    int16_t r;
} coord_t;

// Tetrads
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

const song_t titleMusic =
{
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

const song_t gameMusic = {
    .notes = {
        {.note = C_4, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = C_4, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = E_4, .timeMs = 188},
        {.note = G_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = E_4, .timeMs = 188},
        {.note = G_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = C_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = D_SHARP_4, .timeMs = 188},
        {.note = G_SHARP_4, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = D_SHARP_4, .timeMs = 188},
        {.note = G_SHARP_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = D_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = D_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = D_5, .timeMs = 188},
        {.note = F_6, .timeMs = 188},
        {.note = D_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = C_4, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = C_4, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = E_4, .timeMs = 188},
        {.note = G_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = E_4, .timeMs = 188},
        {.note = G_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = C_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = D_SHARP_4, .timeMs = 188},
        {.note = G_SHARP_4, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = D_SHARP_4, .timeMs = 188},
        {.note = G_SHARP_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = D_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = D_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = D_5, .timeMs = 188},
        {.note = F_6, .timeMs = 188},
        {.note = D_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = C_4, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = C_4, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = C_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = C_6, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = E_4, .timeMs = 188},
        {.note = G_4, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = E_4, .timeMs = 188},
        {.note = G_4, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = C_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = C_6, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = D_SHARP_4, .timeMs = 188},
        {.note = G_SHARP_4, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = D_SHARP_4, .timeMs = 188},
        {.note = G_SHARP_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = D_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = C_4, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = C_4, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = C_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = C_6, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = E_4, .timeMs = 188},
        {.note = G_4, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = E_4, .timeMs = 188},
        {.note = G_4, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = C_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = C_6, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = D_SHARP_4, .timeMs = 188},
        {.note = G_SHARP_4, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = D_SHARP_4, .timeMs = 188},
        {.note = G_SHARP_4, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = F_4, .timeMs = 188},
        {.note = A_SHARP_4, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = D_SHARP_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = D_6, .timeMs = 188},
        {.note = A_SHARP_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = A_5, .timeMs = 188},
        {.note = F_5, .timeMs = 188},
        {.note = A_5, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
        {.note = SILENCE, .timeMs = 188},
    },
    .numNotes = 268,
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

const led_t tetradColors[NUM_TETRAD_TYPES] =
{
    // I_TETRAD
    {
        .r = 0xFF,
        .g = 0xFF,
        .b = 0xFF
    },
    // O_TETRAD
    {
        .r = 0xFF,
        .g = 0x00,
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
        .r = 0xFF,
        .g = 0x45,
        .b = 0x00
    },
    // Z_TETRAD
    {
        .r = 0x00,
        .g = 0xFF,
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

const int32_t firstType[4] = {I_TETRAD, J_TETRAD, L_TETRAD, T_TETRAD};

typedef struct
{
    // Randomizer vars
    tetradRandomizer_t randomizer;
    // BAG
    int32_t typeBag[NUM_TETRAD_TYPES];
    int32_t bagIndex;
    // POOL
    int32_t typePool[35];
    int32_t typeHistory[4];
    list_t* typeOrder;

    // Title screen vars
    tetrad_t tutorialTetrad;
    uint32_t tutorialTetradsGrid[TUTORIAL_GRID_ROWS][TUTORIAL_GRID_COLS];
    /*
    int64_t exitTimer;
    int64_t lastExitTimer;
    bool holdingExit;
    */

    // Score screen vars
    int64_t clearScoreTimer;
    int64_t lastClearScoreTimer;
    bool holdingClearScore;
    
    // Game state vars
    uint32_t tetradsGrid[GRID_ROWS][GRID_COLS];
    uint32_t nextTetradGrid[NEXT_GRID_ROWS][NEXT_GRID_COLS];
    tetrad_t activeTetrad;
    tetradType_t nextTetradType;
    list_t* landedTetrads;
    bool landTetradFX; // If a tiltrad landed flash it to indicate it has locked in-place.
    bool activeTetradChange; // Tracks if the active tetrad changed in some way between frames, useful for redraw logic.
    uint32_t tetradCounter; // Used for distinguishing tetrads on the full grid, and for counting how many total tetrads have landed.
    int64_t dropTimer;  // The timer for dropping the current tetrad one level.
    int64_t dropTime; // The amount of time it takes for a tetrad to drop. Changes based on the level.
    int64_t dropFXTime; // This is specifically used for handling the perspective effect correctly with regards to increasing dropSpeed.
    bool isPaused;
    
    // Score related vars
    uint32_t linesClearedTotal; // The number of lines cleared total.
    uint32_t linesClearedLastDrop; // The number of lines cleared the last time a tetrad landed. (Used for combos)
    uint32_t comboCount; // The combo counter for successive line clears.
    int64_t currentLevel; // The current difficulty level, increments every 10 line clears.
    int32_t score; // The current score this game.
    int32_t highScores[NUM_TT_HIGH_SCORES];
    bool newHighScore;

    // Clear animation vars
    bool inClearAnimation;
    int64_t clearTimer;
    int64_t clearTime;

    // Gameover vars
    int16_t gameoverScoreX;
    int32_t gameoverLEDAnimCycle;

    // Input vars
    accel_t ttAccel;
    accel_t ttLastAccel;
    accel_t ttLastTestAccel;

    uint16_t ttButtonState;
    uint16_t ttButtonsPressedSinceLast;
    uint16_t ttLastButtonState;

    int64_t modeStartTime; // Time mode started in microseconds.
    int64_t stateStartTime; // Time the most recent state started in microseconds.
    int64_t deltaTime; // Time elapsed since last update in microseconds.
    int64_t modeTime; // Total time the mode has been running in microseconds.
    int64_t stateTime; // Total time the state has been running in microseconds.
    uint32_t modeFrames; // Total number of frames elapsed in this mode.
    uint32_t stateFrames; // Total number of frames elapsed in this state.

    // Game state
    tiltradsState_t currState;

    // LED FX vars
    led_t leds[NUM_LEDS];

    // Display pointer
    display_t* disp;

    // Fonts
    font_t ibm_vga8;
    font_t radiostars;
} tiltrads_t;

// Struct pointer
tiltrads_t* tiltrads;

// Function prototypes go here.

// Mode hook functions
void ttInit(display_t* disp);
void ttDeInit(void);
void ttButtonCallback(buttonEvt_t* evt);
void ttAccelerometerCallback(accel_t* accel);

// Game loop functions
static void ttUpdate(int64_t elapsedUs);

// Handle inputs.
void ttTitleInput(void);
void ttGameInput(void);
void ttScoresInput(void);
void ttGameoverInput(void);

// Update any input-unrelated logic.
void ttTitleUpdate(void);
void ttGameUpdate(void);
void ttScoresUpdate(void);
void ttGameoverUpdate(void);

// Draw the frame.
void ttTitleDisplay(void);
void ttGameDisplay(void);
void ttScoresDisplay(void);
void ttGameoverDisplay(void);

// Helper functions

// Mode state management
void ttChangeState(tiltradsState_t newState);

// Input checking
bool ttIsButtonPressed(uint8_t button);
// bool ttIsButtonReleased(uint8_t button);
bool ttIsButtonDown(uint8_t button);
bool ttIsButtonUp(uint8_t button);

// Grid management
void copyGrid(coord_t srcOffset, uint8_t srcCols, uint8_t srcRows, const uint32_t src[][srcCols],
              uint8_t dstCols, uint8_t dstRows, uint32_t dst[][dstCols]);
void transferGrid(coord_t srcOffset, uint8_t srcCols, uint8_t srcRows,
                  const uint32_t src[][srcCols], uint8_t dstCols, uint8_t dstRows, uint32_t dst[][dstCols], uint32_t transferVal);
void clearGrid(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols]);
void refreshTetradsGrid(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols],
                        list_t* fieldTetrads, tetrad_t* movingTetrad, bool includeMovingTetrad);
int16_t xFromGridCol(int16_t x0, int16_t gridCol, uint8_t unitSize);
int16_t yFromGridRow(int16_t y0, int16_t gridRow, uint8_t unitSize);
// void debugPrintGrid(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols]);


// Tetrad operations
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
// int32_t getHighestActiveRow(tetrad_t* tetrad);
int32_t getFallDistance(tetrad_t* tetrad, uint8_t gridCols, uint8_t gridRows,
                        const uint32_t gridData[][gridCols]);

// Drawing functions
void plotSquare(display_t* disp, int16_t x0, int16_t y0, int16_t size, paletteColor_t col);
void plotGrid(display_t* disp, int16_t x0, int16_t y0, int16_t unitSize, uint8_t gridCols, uint8_t gridRows,
                                uint32_t gridData[][gridCols], bool clearLineAnimation, paletteColor_t col);
void plotTetrad(display_t* disp, int16_t x0, int16_t y0, int16_t unitSize, uint8_t tetradCols, uint8_t tetradRows,
                                  uint32_t shape[][tetradCols], uint8_t tetradFill, int32_t fillRotation, paletteColor_t borderColor, paletteColor_t fillColor);
void plotPerspectiveEffect(display_t* disp, int16_t leftSrc, int16_t leftDst, int16_t rightSrc, int16_t rightDst,
                           int16_t y0, int16_t y1, int32_t numVerticalLines, int32_t numHorizontalLines, double lineTweenTimeS,
                           uint32_t currentTimeUS,
                           paletteColor_t col);
uint16_t getCenteredTextX(font_t* font, const char* text, int16_t x0, int16_t x1);
void getNumCentering(font_t* font, const char* text, int16_t achorX0, int16_t anchorX1, int16_t* textX0,
                     int16_t* textX1);

// Randomizer operations
void initTypeOrder(void);
void clearTypeOrder(void);
void deInitTypeOrder(void);
void initTetradRandomizer(tetradRandomizer_t randomType);
int32_t getNextTetradType(tetradRandomizer_t randomType, int32_t index);
void shuffle(int32_t length, int32_t array[length]);

// Score operations
void loadHighScores(void);
void saveHighScores(void);
bool updateHighScores(int32_t newScore);

// Tiltrads score functions
void ttGetHighScores(void);
void ttSetHighScores(void);

// Game logic operations
void initLandedTetrads(void);
void clearLandedTetrads(void);
void deInitLandedTetrads(void);

void startClearAnimation(int32_t numLineClears);
void stopClearAnimation(void);

int64_t getDropTime(int64_t level);
double getDropFXTimeFactor(int64_t level);

bool isLineCleared(int32_t line, uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols]);
int32_t checkLineClears(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols],
                        list_t* fieldTetrads);
int32_t clearLines(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols],
                   list_t* fieldTetrads);

bool checkCollision(coord_t newPos, uint8_t tetradCols, uint8_t tetradRows,
                    const uint32_t shape[][tetradCols], uint8_t gridCols, uint8_t gridRows, const uint32_t gridData[][gridCols],
                    uint32_t selfGridValue);

// LED FX functions
void singlePulseLEDs(uint8_t numLEDs, led_t fxColor, double progress);
void blinkLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time);
void alternatingPulseLEDS(uint8_t numLEDs, led_t fxColor, uint32_t time);
void dancingLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time);
void countdownLEDs(uint8_t numLEDs, led_t fxColor, double progress);
void clearLEDs(uint8_t numLEDs);
void applyLEDBrightness(uint8_t numLEDs, double brightness);

// Mode struct hook
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
    .fnTemperatureCallback = NULL,
    .overrideUsb = false
};

void ttInit(display_t* disp)
{
    // Allocate and zero memory.
    tiltrads = calloc(1, sizeof(tiltrads_t));

    // Save the display pointer.
    tiltrads->disp = disp;

    // Load some fonts.
    loadFont("ibm_vga8.font", &(tiltrads->ibm_vga8));
    loadFont("radiostars.font", &(tiltrads->radiostars));

    // Initialize a lot of variables.
    tiltrads->randomizer = POOL;
    tiltrads->typeBag[0] = I_TETRAD;
    tiltrads->typeBag[1] = J_TETRAD;
    tiltrads->typeBag[2] = L_TETRAD;
    tiltrads->typeBag[3] = O_TETRAD;
    tiltrads->typeBag[4] = S_TETRAD;
    tiltrads->typeBag[5] = T_TETRAD;
    tiltrads->typeBag[6] = Z_TETRAD;

    // Input vars
    tiltrads->ttAccel.x = 0;
    tiltrads->ttAccel.y = 0;
    tiltrads->ttAccel.z = 0;

    tiltrads->ttLastAccel.x = 0;
    tiltrads->ttLastAccel.y = 0;
    tiltrads->ttLastAccel.z = 0;

    tiltrads->ttLastTestAccel.x = 0;
    tiltrads->ttLastTestAccel.y = 0;
    tiltrads->ttLastTestAccel.z = 0;

    tiltrads->ttButtonState = 0;
    tiltrads->ttButtonsPressedSinceLast = 0;
    tiltrads->ttLastButtonState = 0;

    // Reset mode time tracking.
    tiltrads->modeStartTime = esp_timer_get_time();
    tiltrads->stateStartTime = 0;
    tiltrads->deltaTime = 0;
    tiltrads->modeTime = 0;
    tiltrads->stateTime = 0;
    tiltrads->modeFrames = 0;
    tiltrads->stateFrames = 0;

    // Game state
    tiltrads->currState = TT_TITLE;

    tiltrads->landTetradFX = false;

    // Grab any memory we need.
    initLandedTetrads();
    initTypeOrder();

    // Reset state stuff.
    ttChangeState(TT_TITLE);
}

void ttDeInit(void)
{
    freeFont(&(tiltrads->ibm_vga8));
    freeFont(&(tiltrads->radiostars));

    buzzer_stop();

    deInitLandedTetrads();
    deInitTypeOrder();

    free(tiltrads);
}

void ttButtonCallback(buttonEvt_t* evt)
{
    tiltrads->ttButtonState = evt->state;  // Set the state of all buttons.
    // If this was a press
    if(evt->down)
    {
        // Save this press so a quick press/release isn't ignored
        tiltrads->ttButtonsPressedSinceLast |= evt->button;
    }
}

void ttAccelerometerCallback(accel_t* accel)
{
    // Set the accelerometer values.
    tiltrads->ttAccel.x = accel->x;
    tiltrads->ttAccel.y = accel->y;
    tiltrads->ttAccel.z = accel->z;
}

static void ttUpdate(int64_t elapsedUs __attribute__((unused)))
{
    // Update time tracking.
    // NOTE: delta time is in microseconds
    // UPDATE time is in milliseconds

    int64_t newModeTime = esp_timer_get_time() - tiltrads->modeStartTime;
    int64_t newStateTime = esp_timer_get_time() - tiltrads->stateStartTime;
    tiltrads->deltaTime = newModeTime - tiltrads->modeTime;
    tiltrads->modeTime = newModeTime;
    tiltrads->stateTime = newStateTime;
    tiltrads->modeFrames++;
    tiltrads->stateFrames++;

    // Handle input. (based on the state)
    switch( tiltrads->currState )
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
    tiltrads->ttLastButtonState = tiltrads->ttButtonState;
    // Clear the buttons pressed since last check
    tiltrads->ttButtonsPressedSinceLast = 0;
    tiltrads->ttLastAccel = tiltrads->ttAccel;

    // Handle game logic. (based on the state)
    switch( tiltrads->currState )
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

    // Handle drawing frame. (based on the state)
    switch( tiltrads->currState )
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

    // Guidelines
    //plotLine(tiltrads->disp, 0, 0, 0, 240, c200, 5);
    //plotLine(tiltrads->disp, 140, 0, 140, 240, c200, 5);
    //plotLine(tiltrads->disp, 280, 0, 280, 240, c200, 5);
    //plotLine(tiltrads->disp, 0, 120, 280, 120, c200, 5);

    // Draw debug FPS counter.
    /*double seconds = ((double)stateTime * (double)US_TO_MS_FACTOR * (double)MS_TO_S_FACTOR);
    int32_t fps = (int)((double)stateFrames / seconds);
    snprintf(uiStr, sizeof(uiStr), "FPS: %d", fps);
    drawText(tiltrads->disp->w - getTextWidth(uiStr, TOM_THUMB) - 1, tiltrads->disp->h - (1 * (ibm_vga8.h + 1)), uiStr, TOM_THUMB, c555);*/
}

void ttTitleInput(void)
{
    // Accel = tilt something on screen like you would a tetrad.
    moveTetrad(&(tiltrads->tutorialTetrad), TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tiltrads->tutorialTetradsGrid);

    // Start game.
    if(ttIsButtonPressed(BTN_TITLE_START_GAME) || ttIsButtonPressed(BTN_TITLE_START_GAME_ALT))
    {
        ttChangeState(TT_GAME);
    }
    // Go to score screen.
    else if(ttIsButtonPressed(BTN_TITLE_START_SCORES))
    {
        ttChangeState(TT_SCORES);
    }

    /*
    // Exit mode.
    tiltrads->lastExitTimer = tiltrads->exitTimer;
    if(tiltrads->holdingExit && ttIsButtonDown(BTN_TITLE_EXIT_MODE))
    {
        tiltrads->exitTimer += tiltrads->deltaTime;
        if (tiltrads->exitTimer >= EXIT_MODE_HOLD_TIME)
        {
            tiltrads->exitTimer = 0;
            switchToSwadgeMode(&modeMainMenu);
        }
    }
    else if(ttIsButtonUp(BTN_TITLE_EXIT_MODE))
    {
        tiltrads->exitTimer = 0;
    }
    // This is added to prevent people holding the button from the previous screen and accidentally quitting the game.
    else if(ttIsButtonPressed(BTN_TITLE_EXIT_MODE))
    {
        tiltrads->holdingExit = true;
    }
    */
}

void ttGameInput(void)
{
    if (ttIsButtonPressed(BTN_GAME_PAUSE))
    {
        tiltrads->isPaused = !tiltrads->isPaused;

        if(tiltrads->isPaused)
        {
            buzzer_stop();
        }
        else
        {
            //buzzer_play_bgm(&gameMusic);
        }
    }

    // Reset the check for if the active tetrad moved, dropped, or landed.
    tiltrads->activeTetradChange = false;

    // Refresh the tetrads grid.
    refreshTetradsGrid(GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid, tiltrads->landedTetrads, &(tiltrads->activeTetrad), false);

    // Only respond to input when the clear animation isn't running.
    if (!tiltrads->inClearAnimation && !tiltrads->isPaused)
    {
        // Rotate piece.
        if(ttIsButtonPressed(BTN_GAME_ROTATE_CW))
        {
            tiltrads->activeTetradChange = rotateTetrad(&(tiltrads->activeTetrad), tiltrads->activeTetrad.rotation + 1, GRID_COLS, GRID_ROWS,
                                              tiltrads->tetradsGrid);
        }
        else if(ttIsButtonPressed(BTN_GAME_ROTATE_ACW))
        {
            tiltrads->activeTetradChange = rotateTetrad(&(tiltrads->activeTetrad), tiltrads->activeTetrad.rotation - 1, GRID_COLS, GRID_ROWS,
                                              tiltrads->tetradsGrid);
        }

#ifdef NO_STRESS_TRIS
        if(ttIsButtonPressed(BTN_GAME_SOFT_DROP))
        {
            tiltrads->dropTimer = tiltrads->dropTime;
        }
        else if (ttIsButtonPressed(BTN_GAME_HARD_DROP)) 
        {
            // Drop piece as far as it will go before landing.
            int32_t dropDistance = 0;
            while (dropTetrad(&(tiltrads->activeTetrad), GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid))
            {
                dropDistance++;
            }

            tiltrads->score += dropDistance * SCORE_HARD_DROP;
            // Set the drop timer so it will land on update.
            tiltrads->dropTimer = tiltrads->dropTime;
            debugPrintGrid(GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid);
        }
#else
        // Button down = soft drop piece.
        if(ttIsButtonDown(BTN_GAME_SOFT_DROP))
        {
            softDropTetrad();
        }
        // Button up = hard drop piece.
        else if (ttIsButtonPressed(BTN_GAME_HARD_DROP)) 
        {
            // Drop piece as far as it will go before landing.
            int32_t dropDistance = 0;
            while (dropTetrad(&(tiltrads->activeTetrad), GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid))
            {
                dropDistance++;
            }
            tiltrads->score += dropDistance * SCORE_HARD_DROP;
            // Set the drop timer so it will land on update.
            tiltrads->dropTimer = tiltrads->dropTime;
        }
#endif

        // Only move tetrads left and right when the fast drop button isn't being held down.
        if(ttIsButtonUp(BTN_GAME_SOFT_DROP) && ttIsButtonUp(BTN_GAME_HARD_DROP))
        {
            tiltrads->activeTetradChange = tiltrads->activeTetradChange || moveTetrad(&(tiltrads->activeTetrad), GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid);
        }
    }
}

void ttScoresInput(void)
{
    tiltrads->lastClearScoreTimer = tiltrads->clearScoreTimer;
    // Hold to clear scores.
    if(tiltrads->holdingClearScore && ttIsButtonDown(BTN_SCORES_CLEAR_SCORES))
    {
        tiltrads->clearScoreTimer += tiltrads->deltaTime;
        if (tiltrads->clearScoreTimer >= CLEAR_SCORES_HOLD_TIME)
        {
            tiltrads->clearScoreTimer = 0;
            memset(tiltrads->highScores, 0, NUM_TT_HIGH_SCORES * sizeof(uint32_t));
            saveHighScores();
            loadHighScores();
        }
    }
    else if(ttIsButtonUp(BTN_SCORES_CLEAR_SCORES))
    {
        tiltrads->clearScoreTimer = 0;
    }
    // This is added to prevent people holding the button from the previous screen from accidentally clearing their scores.
    else if(ttIsButtonPressed(BTN_SCORES_CLEAR_SCORES))
    {
        tiltrads->holdingClearScore = true;
    }

    // Go to title screen.
    if(ttIsButtonPressed(BTN_SCORES_START_TITLE))
    {
        ttChangeState(TT_TITLE);
    }
}

void ttGameoverInput(void)
{
    // Start game.
    if(ttIsButtonPressed(BTN_GAMEOVER_START_GAME) || ttIsButtonPressed(BTN_GAMEOVER_START_GAME_ALT))
    {
        ttChangeState(TT_GAME);
    }
    // Go to title screen.
    else if(ttIsButtonPressed(BTN_GAMEOVER_START_TITLE))
    {
        ttChangeState(TT_TITLE);
    }
}

void ttTitleUpdate(void)
{
    // Refresh the tetrads grid.
    refreshTetradsGrid(TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tiltrads->tutorialTetradsGrid, tiltrads->landedTetrads, &(tiltrads->tutorialTetrad),
                       false);

    tiltrads->dropTimer += tiltrads->deltaTime;

    if (tiltrads->dropTimer >= tiltrads->dropTime)
    {
        tiltrads->dropTimer = 0;

        // If we couldn't drop, then we've landed.
        if (!dropTetrad(&(tiltrads->tutorialTetrad), TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tiltrads->tutorialTetradsGrid))
        {
            // Spawn the next tetrad.
            spawnNextTetrad(&(tiltrads->tutorialTetrad), BAG, 0, TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tiltrads->tutorialTetradsGrid);

            // Reset the drop info to whatever is appropriate for the current level.
            tiltrads->dropTime = getDropTime(TITLE_LEVEL);
            tiltrads->dropTimer = 0;
        }
    }

    dancingLEDs(NUM_LEDS, titleColor, tiltrads->stateTime);
}

void ttGameUpdate(void)
{
    // Refresh the tetrads grid.
    refreshTetradsGrid(GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid, tiltrads->landedTetrads, &(tiltrads->activeTetrad), false);

    if (!tiltrads->isPaused)
    {
        // Land tetrad.
        // Update score.
        // Start clear animation.
        // End clear animation.
        // Clear lines.
        // Spawn new active tetrad.

        if (tiltrads->inClearAnimation)
        {
            tiltrads->clearTimer += tiltrads->deltaTime;

            double clearProgress = (double)tiltrads->clearTimer / (double)tiltrads->clearTime;
            singlePulseLEDs(NUM_LEDS, clearColor, clearProgress);

            if (tiltrads->clearTimer >= tiltrads->clearTime)
            {
                stopClearAnimation();

                // Actually clear the lines.
                clearLines(GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid, tiltrads->landedTetrads);

                // Spawn the next tetrad.
                spawnNextTetrad(&(tiltrads->activeTetrad), tiltrads->randomizer, tiltrads->tetradCounter, GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid);

                // Reset the drop info to whatever is appropriate for the current level.
                tiltrads->dropTime = getDropTime(tiltrads->currentLevel);
                tiltrads->dropTimer = 0;

                // There's a new active tetrad, so that's a change.
                tiltrads->activeTetradChange = true;
            }
        }
        else
        {
    #ifndef NO_STRESS_TRIS
            tiltrads->dropTimer += tiltrads->deltaTime;
    #endif

            // Update the LED FX.
            // Progress is the drop time for this row. (Too fast)
            //double dropProgress = (double)tiltrads->dropTimer / (double)tiltrads->dropTime;

            // Progress is how close it is to landing on the floor. (Too nebulous or unhelpful?)
            double totalFallTime = (GRID_ROWS - 1) * tiltrads->dropTime;
            int32_t fallDistance = getFallDistance(&(tiltrads->activeTetrad), GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid);

            double totalFallProgress = totalFallTime - (((fallDistance + 1) * tiltrads->dropTime) - tiltrads->dropTimer);
            double countdownProgress = totalFallProgress / totalFallTime;

            // NOTE: this check is here because under unknown circumstances the math above can produce bad countdownProgress values, causing a slight flicker when a tetrad lands.
            // Ideally the math above should be fixed, but this is an acceptable fix for now.
            if (countdownProgress >= 0.0 && countdownProgress <= 1.0)
            {
                countdownLEDs(NUM_LEDS, tetradColors[tiltrads->activeTetrad.type - 1], countdownProgress);
            }

            if (tiltrads->dropTimer >= tiltrads->dropTime)
            {
                tiltrads->dropTimer = 0;

                // The active tetrad has either dropped or landed, redraw required either way.
                tiltrads->activeTetradChange = true;

                if (ttIsButtonDown(BTN_GAME_SOFT_DROP))
                {
                    tiltrads->score += SCORE_SOFT_DROP;
                }

                // If we couldn't drop, then we've landed.
                if (!dropTetrad(&(tiltrads->activeTetrad), GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid))
                {
                    tiltrads->landTetradFX = true;

                    // Land the current tetrad.
                    tetrad_t* landedTetrad = malloc(sizeof(tetrad_t));
                    landedTetrad->type = tiltrads->activeTetrad.type;
                    landedTetrad->gridValue = tiltrads->activeTetrad.gridValue;
                    landedTetrad->rotation = tiltrads->activeTetrad.rotation;
                    landedTetrad->topLeft = tiltrads->activeTetrad.topLeft;

                    coord_t origin;
                    origin.c = 0;
                    origin.r = 0;
                    copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tiltrads->activeTetrad.shape, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                            landedTetrad->shape);

                    push(tiltrads->landedTetrads, landedTetrad);

                    tiltrads->tetradCounter++;

                    // Check for any clears now that the new tetrad has landed.
                    uint32_t linesClearedThisDrop = checkLineClears(GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid, tiltrads->landedTetrads);

                    int32_t landingSFX;

                    switch( linesClearedThisDrop )
                    {
                        case 1:
                            tiltrads->score += SCORE_SINGLE * (tiltrads->currentLevel + 1);
                            buzzer_play_sfx(&singleLineClearSFX);
                            break;
                        case 2:
                            tiltrads->score += SCORE_DOUBLE * (tiltrads->currentLevel + 1);
                            buzzer_play_sfx(&doubleLineClearSFX);
                            break;
                        case 3:
                            tiltrads->score += SCORE_TRIPLE * (tiltrads->currentLevel + 1);
                            buzzer_play_sfx(&tripleLineClearSFX);
                            break;
                        case 4:
                            tiltrads->score += SCORE_QUAD * (tiltrads->currentLevel + 1);
                            buzzer_play_sfx(&quadLineClearSFX);
                            break;
                        case 0:
                            // Full grid height is GRID_ROWS, we have NUM_LAND_FX SFX, offset results by an amount so that sfx[15] correctly plays at the playfield floor and the first row above it.
                            landingSFX = getLowestActiveRow(landedTetrad) - ((GRID_ROWS - NUM_LAND_FX) - 1);
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
                    if (tiltrads->linesClearedLastDrop > 0 && linesClearedThisDrop > 0)
                    {
                        tiltrads->comboCount += linesClearedThisDrop;
                        tiltrads->score += SCORE_COMBO * tiltrads->comboCount * (tiltrads->currentLevel + 1);
                    }
                    else
                    {
                        tiltrads->comboCount = 0;
                    }

                    // Increase total number of lines cleared.
                    tiltrads->linesClearedTotal += linesClearedThisDrop;

                    // Update the level if necessary.
                    tiltrads->currentLevel = tiltrads->linesClearedTotal / LINE_CLEARS_PER_LEVEL;

                    // Keep track of the last number of line clears.
                    tiltrads->linesClearedLastDrop = linesClearedThisDrop;

                    if (linesClearedThisDrop > 0)
                    {
                        // Start the clear animation.
                        startClearAnimation(linesClearedThisDrop);
                    }
                    else
                    {
                        // Spawn the next tetrad.
                        spawnNextTetrad(&(tiltrads->activeTetrad), tiltrads->randomizer, tiltrads->tetradCounter, GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid);

                        // Reset the drop info to whatever is appropriate for the current level.
                        tiltrads->dropTime = getDropTime(tiltrads->currentLevel);
                        tiltrads->dropTimer = 0;
                    }
                }

                // Clear out empty tetrads.
                node_t* current = tiltrads->landedTetrads->last;
                for (int32_t t = tiltrads->landedTetrads->length - 1; t >= 0; t--)
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
                        tetrad_t* emptyTetrad = removeIdx(tiltrads->landedTetrads, t);
                        free(emptyTetrad);
                    }
                }

                refreshTetradsGrid(GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid, tiltrads->landedTetrads, &(tiltrads->activeTetrad), false);

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
                    // Check for any clears now that this new.
                    checkLineClears(GRID_COLS, GRID_ROWS, tetradsGrid, landedTetrads);
                }*/
            }
        }

        // Drop FX time advances by the normal amount.
        tiltrads->dropFXTime += tiltrads->deltaTime;
        // Drop FX time advances by deltaTime * SOFT_DROP_FX_FACTOR(2) when the soft drop button is being held down. (Happens in softDropTetrad)

        // Drop FX time advances a little bit more according to the currentLevel.
        tiltrads->dropFXTime += (tiltrads->deltaTime * getDropFXTimeFactor(tiltrads->currentLevel));
    }

    // Check if we have a new high score.
    tiltrads->newHighScore = tiltrads->score > tiltrads->highScores[0];
}

void ttScoresUpdate(void)
{
    // Update the LED FX.
    alternatingPulseLEDS(NUM_LEDS, highScoreColor, tiltrads->modeTime);
}

void ttGameoverUpdate(void)
{
    // Update the LED FX.
    blinkLEDs(NUM_LEDS, gameoverColor, tiltrads->stateTime);
}

void ttTitleDisplay(void)
{
    // Clear the display.
    tiltrads->disp->clearPx();

    // c000 = play area
    // c001 = background of perspective "walls"
    // c112 = perspective lines
    // c224 = grid lines
    // c540 = text
    // c321 = behind text fill
    // c555 = tiltrads clear line color

    // Fill sides and play area.
    fillDisplayArea(tiltrads->disp, 0, 0, tiltrads->disp->w, tiltrads->disp->h, c001);
    fillDisplayArea(tiltrads->disp, GRID_X, 0, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, tiltrads->disp->h, c000);

    // Draw demo-scene title FX.
    plotPerspectiveEffect(tiltrads->disp, GRID_X, 0, xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), tiltrads->disp->w - 1, 0, tiltrads->disp->h, 3, 3,
                          2.0,
                          tiltrads->stateTime, c112);

    // LEFT FOR
    int16_t scoresTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "SELECT FOR", 0, GRID_X);
    int16_t scoresTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * 7 + 1);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "SELECT FOR", scoresTextX, scoresTextY);

    // SCORES
    scoresTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "SCORES", 0, GRID_X);
    scoresTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * 6);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "SCORES", scoresTextX, scoresTextY);

    // START TO
    int16_t playTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "START TO", xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), tiltrads->disp->w);
    int16_t playTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * 7 + 1);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "START TO", playTextX, playTextY);

    // PLAY
    playTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "PLAY", xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), tiltrads->disp->w);
    playTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * 6);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "PLAY", playTextX, playTextY);

    // Clear the grid data. (may not want to do this every frame)
    refreshTetradsGrid(TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tiltrads->tutorialTetradsGrid, tiltrads->landedTetrads, &(tiltrads->tutorialTetrad),
                       true);

    // Draw the active tetrad.
    plotTetrad(tiltrads->disp, xFromGridCol(GRID_X, tiltrads->tutorialTetrad.topLeft.c, GRID_UNIT_SIZE),
               yFromGridRow(GRID_Y, tiltrads->tutorialTetrad.topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
               tiltrads->tutorialTetrad.shape, tiltrads->tutorialTetrad.type, tiltrads->tutorialTetrad.rotation, borderColors[tiltrads->tutorialTetrad.type-1], fillColors[tiltrads->tutorialTetrad.type-1]);

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(tiltrads->disp, GRID_X, GRID_Y, GRID_UNIT_SIZE, TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tiltrads->tutorialTetradsGrid, false, c224);

    // TILTRADS
    int16_t titleTextX = getCenteredTextX(&(tiltrads->radiostars), "TILTRADS", 0, tiltrads->disp->w);
    int16_t titleTextY = DISPLAY_HALF_HEIGHT - tiltrads->radiostars.h - 2;
    drawText(tiltrads->disp, &(tiltrads->radiostars), c540, "TILTRADS", titleTextX, titleTextY);

    // COLOR (rotating color fx)
    titleTextX = getCenteredTextX(&(tiltrads->radiostars), "COLOR", 0, tiltrads->disp->w);
    titleTextY += tiltrads->radiostars.h + 2;
    drawText(tiltrads->disp, &(tiltrads->radiostars), tiltrads->stateFrames % c555, "COLOR", titleTextX, titleTextY);

    /*
    // SELECT TO EXIT
    int16_t exitX = getCenteredTextX(&(tiltrads->ibm_vga8), "B TO EXIT", 0, tiltrads->disp->w);
    int16_t exitY = tiltrads->disp->h - tiltrads->ibm_vga8.h - 2;
    // Fill the SELECT TO EXIT area depending on how long the button's held down.
    if (tiltrads->exitTimer != 0)
    {
        double holdProgress = ((double)(tiltrads->exitTimer) / (double)EXIT_MODE_HOLD_TIME);
        int16_t holdAreaX0 = GRID_X + 1;
        int16_t holdAreaY0 = exitY - 1;
        double holdAreaWidth = TUTORIAL_GRID_COLS * GRID_UNIT_SIZE;
        int16_t holdAreaX1 = holdAreaX0 + (int16_t)(holdProgress * holdAreaWidth);
        int16_t holdAreaY1 = tiltrads->disp->h - 1;
        fillDisplayArea(tiltrads->disp, holdAreaX0, holdAreaY0, holdAreaX1, holdAreaY1, c321);
    }
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "B TO EXIT", exitX, exitY);
    */

    // Fill in the floor of the grid on-screen for visual consistency.
    plotLine(tiltrads->disp, GRID_X, tiltrads->disp->h - 1, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, tiltrads->disp->h - 1, c224, 0);
}

void ttGameDisplay(void)
{
    // Clear the display.
    tiltrads->disp->clearPx();

    // Fill the BG sides and play area.
    fillDisplayArea(tiltrads->disp, 0, 0, tiltrads->disp->w, tiltrads->disp->h, c001);
    fillDisplayArea(tiltrads->disp, GRID_X, 0, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, tiltrads->disp->h, c000);

    // Draw the BG FX.
    // Goal: noticeable speed-ups when level increases and when soft drop is being held or released.
    plotPerspectiveEffect(tiltrads->disp, GRID_X, 0, xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), tiltrads->disp->w - 1, 0, tiltrads->disp->h, 3, 3,
                        5.0,
                        tiltrads->dropFXTime, c112);

    // Draw the active tetrad.
    plotTetrad(tiltrads->disp, xFromGridCol(GRID_X, tiltrads->activeTetrad.topLeft.c, GRID_UNIT_SIZE),
               yFromGridRow(GRID_Y, tiltrads->activeTetrad.topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
               tiltrads->activeTetrad.shape, tiltrads->activeTetrad.type, tiltrads->activeTetrad.rotation, borderColors[tiltrads->activeTetrad.type-1], fillColors[tiltrads->activeTetrad.type-1]);

    // Draw all the landed tetrads.
    node_t* current = tiltrads->landedTetrads->first;
    for (int32_t t = 0; t < tiltrads->landedTetrads->length; t++)
    {
        tetrad_t* currentTetrad = (tetrad_t*)current->val;
        plotTetrad(tiltrads->disp, xFromGridCol(GRID_X, currentTetrad->topLeft.c, GRID_UNIT_SIZE),
                   yFromGridRow(GRID_Y, currentTetrad->topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                   currentTetrad->shape, currentTetrad->type, currentTetrad->rotation, borderColors[currentTetrad->type - 1],
                   fillColors[currentTetrad->type - 1]);
        current = current->next;
    }

    // Draw landed tetrad lock effect.
    if (tiltrads->landTetradFX)
    {
        tetrad_t* landedTetrad = (tetrad_t*)tiltrads->landedTetrads->last->val;
        plotTetrad(tiltrads->disp, xFromGridCol(GRID_X, landedTetrad->topLeft.c, GRID_UNIT_SIZE),
                   yFromGridRow(GRID_Y, landedTetrad->topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                   landedTetrad->shape, landedTetrad->type, landedTetrad->rotation, fillColors[landedTetrad->type-1], borderColors[landedTetrad->type-1]);
        tiltrads->landTetradFX = false;
    }

    // Clear the grid data (may not want to do this every frame).
    refreshTetradsGrid(GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid, tiltrads->landedTetrads, &(tiltrads->activeTetrad), true);

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(tiltrads->disp, GRID_X, GRID_Y, GRID_UNIT_SIZE, GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid, tiltrads->inClearAnimation, c224);

    // Draw the UI.
    int16_t currY = 0;

    // NEXT
    currY = NEXT_GRID_Y - 2 - tiltrads->ibm_vga8.h;
    int16_t nextHeaderTextStart = getCenteredTextX(&(tiltrads->ibm_vga8), "NEXT", NEXT_GRID_X, xFromGridCol(NEXT_GRID_X, NEXT_GRID_COLS, GRID_UNIT_SIZE));
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "NEXT", nextHeaderTextStart, currY);

    // Fill area of grid background.
    fillDisplayArea(tiltrads->disp, NEXT_GRID_X, NEXT_GRID_Y, xFromGridCol(NEXT_GRID_X, NEXT_GRID_COLS, GRID_UNIT_SIZE),
                    yFromGridRow(NEXT_GRID_Y, NEXT_GRID_ROWS, GRID_UNIT_SIZE), c000);

    // Draw the next tetrad.
    coord_t nextTetradPoint;
    nextTetradPoint.c = 1;
    nextTetradPoint.r = 1;
    tetrad_t nextTetrad = spawnTetrad(tiltrads->nextTetradType, tiltrads->tetradCounter + 1, nextTetradPoint, TETRAD_SPAWN_ROT);
    plotTetrad(tiltrads->disp, xFromGridCol(NEXT_GRID_X, nextTetradPoint.c, GRID_UNIT_SIZE),
               yFromGridRow(NEXT_GRID_Y, nextTetradPoint.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
               nextTetrad.shape, nextTetrad.type, nextTetrad.rotation, borderColors[nextTetrad.type - 1],
               fillColors[nextTetrad.type - 1]);

    // Draw the grid holding the next tetrad.
    clearGrid(NEXT_GRID_COLS, NEXT_GRID_ROWS, tiltrads->nextTetradGrid);
    copyGrid(nextTetrad.topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, nextTetrad.shape, NEXT_GRID_COLS, NEXT_GRID_ROWS,
             tiltrads->nextTetradGrid);
    plotGrid(tiltrads->disp, NEXT_GRID_X, NEXT_GRID_Y, GRID_UNIT_SIZE, NEXT_GRID_COLS, NEXT_GRID_ROWS, tiltrads->nextTetradGrid, false, c224);

    // Draw the left-side score UI.
    char uiStr[32] = {0};
    int16_t numFieldStart = 0;
    int16_t numFieldEnd = 0;

    // HIGH
    int16_t highScoreHeaderTextStart = getCenteredTextX(&(tiltrads->ibm_vga8), tiltrads->newHighScore ? "HIGH (NEW)" : "HIGH", 0, GRID_X);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, tiltrads->newHighScore ? "HIGH (NEW)" : "HIGH", highScoreHeaderTextStart, currY);

    // 99999
    currY += (tiltrads->ibm_vga8.h + 1);
    snprintf(uiStr, sizeof(uiStr), "%d", tiltrads->newHighScore ? tiltrads->score : tiltrads->highScores[0]);
    getNumCentering(&(tiltrads->ibm_vga8), uiStr, 0, GRID_X, &numFieldStart, &numFieldEnd);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, uiStr, numFieldStart, currY);

    // SCORE
    currY += tiltrads->ibm_vga8.h + (tiltrads->ibm_vga8.h + 1);
    int16_t scoreHeaderTextStart = getCenteredTextX(&(tiltrads->ibm_vga8), "SCORE", 0, GRID_X);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "SCORE", scoreHeaderTextStart, currY);

    // 99999
    currY += (tiltrads->ibm_vga8.h + 1);
    snprintf(uiStr, sizeof(uiStr), "%d", tiltrads->score);
    getNumCentering(&(tiltrads->ibm_vga8), uiStr, 0, GRID_X, &numFieldStart, &numFieldEnd);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, uiStr, numFieldStart, currY);

    // LINES
    currY += tiltrads->ibm_vga8.h + (tiltrads->ibm_vga8.h + 1) + 1;
    int16_t linesHeaderTextStart = getCenteredTextX(&(tiltrads->ibm_vga8), "LINES", 0, GRID_X);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "LINES", linesHeaderTextStart, currY);

    // 999
    currY += (tiltrads->ibm_vga8.h + 1);
    snprintf(uiStr, sizeof(uiStr), "%u", tiltrads->linesClearedTotal);
    getNumCentering(&(tiltrads->ibm_vga8), uiStr, 0, GRID_X, &numFieldStart, &numFieldEnd);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, uiStr, numFieldStart, currY);

    // LEVEL
    currY = yFromGridRow(NEXT_GRID_Y, NEXT_GRID_ROWS, GRID_UNIT_SIZE) + 3;
    int16_t levelHeaderTextStart = getCenteredTextX(&(tiltrads->ibm_vga8), "LEVEL", NEXT_GRID_X, xFromGridCol(NEXT_GRID_X, NEXT_GRID_COLS, GRID_UNIT_SIZE));
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "LEVEL", levelHeaderTextStart, currY);

    // 99
    currY += (tiltrads->ibm_vga8.h + 1);
    snprintf(uiStr, sizeof(uiStr), "%d", (int)(tiltrads->currentLevel + 1)); // Levels are displayed with 1 as the base level.
    getNumCentering(&(tiltrads->ibm_vga8), uiStr, NEXT_GRID_X, xFromGridCol(NEXT_GRID_X, NEXT_GRID_COLS, GRID_UNIT_SIZE), &numFieldStart,
                    &numFieldEnd);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, uiStr, numFieldStart, currY);

    int topIncrement = 8;
    int botIncrement = 7;

    // ROTATE
    int16_t scoresTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "ROTATE", 0, GRID_X);
    int16_t scoresTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * topIncrement + 1);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "ROTATE", scoresTextX, scoresTextY);

    // B/A
    scoresTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "B/A", 0, GRID_X);
    scoresTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * botIncrement);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "B/A", scoresTextX, scoresTextY);

    // DROP
    int16_t playTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "DROP", xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), tiltrads->disp->w);
    int16_t playTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * topIncrement + 1);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "DROP", playTextX, playTextY);

    // UP/DOWN
    playTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "UP/DOWN", xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), tiltrads->disp->w);
    playTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * botIncrement);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "UP/DOWN", playTextX, playTextY);

    if (tiltrads->isPaused)
    {
        // Draw a centered bordered window.
        int16_t leftWindowXMargin = 82;
        int16_t rightWindowXMargin = 80;
        int16_t windowYMarginTop = 99;
        int16_t windowYMarginBot = 99;

        int16_t titleTextYOffset = 3;
        int16_t controlTextYOffset = tiltrads->disp->h - windowYMarginBot - tiltrads->ibm_vga8.h - 2;
        int16_t controlTextXPadding = 2;

        // Draw a centered bordered window.
        fillDisplayArea(tiltrads->disp, leftWindowXMargin, windowYMarginTop, tiltrads->disp->w - rightWindowXMargin, tiltrads->disp->h - windowYMarginBot, c000);
        plotRect(tiltrads->disp, leftWindowXMargin, windowYMarginTop, tiltrads->disp->w - rightWindowXMargin, tiltrads->disp->h - windowYMarginBot, c540);

        // PAUSED
        int16_t headerTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "PAUSED", 0, tiltrads->disp->w);
        drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "PAUSED", headerTextX, windowYMarginTop + titleTextYOffset);

        // START TO RESUME (centered)
        //drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "START TO RESUME", (tiltrads->disp->w + leftWindowXMargin - rightWindowXMargin - textWidth(&(tiltrads->ibm_vga8), "START TO RESUME")) / 2 + controlTextXPadding, controlTextYOffset);

        // START TO RESUME (cheating)
        drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "START", leftWindowXMargin + controlTextXPadding, controlTextYOffset);
        drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "TO RESUME", tiltrads->disp->w - rightWindowXMargin - textWidth(&(tiltrads->ibm_vga8), "TO RESUME") - controlTextXPadding, controlTextYOffset);
    }
}

void ttScoresDisplay(void)
{
    // Clear the display.
    tiltrads->disp->clearPx();

    // Fill sides and play area.
    fillDisplayArea(tiltrads->disp, 0, 0, tiltrads->disp->w, tiltrads->disp->h, c001);
    fillDisplayArea(tiltrads->disp, GRID_X, 0, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, tiltrads->disp->h, c000);

    // Draw demo-scene title FX.
    plotPerspectiveEffect(tiltrads->disp, GRID_X, 0, xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), tiltrads->disp->w - 1, 0, tiltrads->disp->h, 3, 3,
                          2.0,
                          tiltrads->stateTime, c112);

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(tiltrads->disp, GRID_X, GRID_Y, GRID_UNIT_SIZE, TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tiltrads->tutorialTetradsGrid, false, c224);

    // Fill in the floor of the grid on-screen for visual consistency.
    plotLine(tiltrads->disp, GRID_X, tiltrads->disp->h - 1, xFromGridCol(GRID_X, TUTORIAL_GRID_COLS, GRID_UNIT_SIZE) - 1, tiltrads->disp->h - 1, c224, 0);


    // HIGH SCORES
    int16_t headerTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "HIGH SCORES", 0, tiltrads->disp->w);
    int16_t headerTextY = SCORE_SCREEN_TITLE_Y;
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "HIGH SCORES", headerTextX, headerTextY);

    char uiStr[32] = {0};

    // XX. 99999
    for (int i = 0; i < NUM_TT_HIGH_SCORES; i++)
    {
        int16_t x0 = 0;
        int16_t x1 = tiltrads->disp->w - 1;
        snprintf(uiStr, sizeof(uiStr), "%d. %d", i + 1, tiltrads->highScores[i]);
        int16_t scoreX = getCenteredTextX(&(tiltrads->ibm_vga8), uiStr, x0, x1);
        drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, uiStr, scoreX, SCORE_SCREEN_SCORE_Y + ((i + 1) * (tiltrads->ibm_vga8.h + 2)) + 4);
    }

    // Draw the clear score text and bar.
    // int16_t clearScoresTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h + 1);

    // fill the clear scores area depending on how long the button's held down.
    if (tiltrads->clearScoreTimer != 0)
    {
        int16_t clearScoresTextX = 29;
        double holdProgress = ((double)(tiltrads->clearScoreTimer) / (double)CLEAR_SCORES_HOLD_TIME);
        int16_t holdAreaX0 = clearScoresTextX - 1;
        int16_t holdAreaY0 = (tiltrads->disp->h - (tiltrads->ibm_vga8.h + 1)) - 1;
        double holdAreaWidth = textWidth(&(tiltrads->ibm_vga8), "CLEAR") + 3;
        int16_t holdAreaX1 = holdAreaX0 + (int16_t)(holdProgress * holdAreaWidth);
        int16_t holdAreaY1 = tiltrads->disp->h - 1;
        plotRect(tiltrads->disp, holdAreaX0, holdAreaY0, holdAreaX0 + holdAreaWidth, holdAreaY1, c321);
        fillDisplayArea(tiltrads->disp, holdAreaX0, holdAreaY0, holdAreaX1, holdAreaY1, c321);
    }

    // SELECT TO
    int16_t scoresTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "SELECT TO", 0, GRID_X);
    int16_t scoresTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * 7 + 1);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "SELECT TO", scoresTextX, scoresTextY);

    // CLEAR
    scoresTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "CLEAR", 0, GRID_X);
    scoresTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * 6);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "CLEAR", scoresTextX, scoresTextY);

    // START FOR
    int16_t playTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "START FOR", xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), tiltrads->disp->w);
    int16_t playTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * 7 + 1);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "START FOR", playTextX, playTextY);

    // TITLE
    playTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "TITLE", xFromGridCol(GRID_X, GRID_COLS, GRID_UNIT_SIZE), tiltrads->disp->w);
    playTextY = tiltrads->disp->h - (tiltrads->ibm_vga8.h * 6);
    drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "TITLE", playTextX, playTextY);
}

void ttGameoverDisplay(void)
{
    // We don't clear the display because we want the playfield to appear in the background.

    // We should only need to draw the gameover UI on the first frame.
    bool drawUI = tiltrads->stateFrames == 0;

    if (drawUI)
    {
        // Draw the gameplay frame first to catch any last minute landed tetrads.
        ttGameDisplay();
        // Draw the active tetrad that was the killing tetrad once so that the flash effect works.
        plotTetrad(tiltrads->disp, xFromGridCol(GRID_X, tiltrads->activeTetrad.topLeft.c, GRID_UNIT_SIZE),
                   yFromGridRow(GRID_Y, tiltrads->activeTetrad.topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
                   tiltrads->activeTetrad.shape, tiltrads->activeTetrad.type, tiltrads->activeTetrad.rotation, borderColors[tiltrads->activeTetrad.type-1], fillColors[tiltrads->activeTetrad.type-1]);

        // Draw a centered bordered window.
        int16_t leftWindowXMargin = 82;
        int16_t rightWindowXMargin = 80;
        int16_t windowYMarginTop = 80;
        int16_t windowYMarginBot = 80;

        int16_t titleTextYOffset = 3;
        int16_t highScoreTextYOffset = titleTextYOffset + tiltrads->ibm_vga8.h + 3;
        int16_t scoreTextYOffset = highScoreTextYOffset + tiltrads->ibm_vga8.h + 4;
        int16_t controlTextYOffset = tiltrads->disp->h - windowYMarginBot - tiltrads->ibm_vga8.h - 2;
        int16_t controlTextXPadding = 2;

        // Draw a centered bordered window.
        fillDisplayArea(tiltrads->disp, leftWindowXMargin, windowYMarginTop, tiltrads->disp->w - rightWindowXMargin, tiltrads->disp->h - windowYMarginBot, c000);
        plotRect(tiltrads->disp, leftWindowXMargin, windowYMarginTop, tiltrads->disp->w - rightWindowXMargin, tiltrads->disp->h - windowYMarginBot, c540);

        // GAME OVER
        int16_t headerTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "GAME OVER", 0, tiltrads->disp->w);
        drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "GAME OVER", headerTextX, windowYMarginTop + titleTextYOffset);

        // HIGH SCORE! or YOUR SCORE:
        if (tiltrads->newHighScore)
        {
            headerTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "HIGH SCORE!", 0, tiltrads->disp->w);
            drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "HIGH SCORE!", headerTextX, windowYMarginTop + highScoreTextYOffset);
        }
        else
        {
            headerTextX = getCenteredTextX(&(tiltrads->ibm_vga8), "YOUR SCORE:", 0, tiltrads->disp->w);
            drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "YOUR SCORE:", headerTextX, windowYMarginTop + highScoreTextYOffset);
        }

        // 1230495
        char scoreStr[32] = {0};
        snprintf(scoreStr, sizeof(scoreStr), "%d", tiltrads->score);
        drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, scoreStr, tiltrads->gameoverScoreX, windowYMarginTop + scoreTextYOffset);

        // SELECT    START
        // TITLE   RESTART
        drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "SELECT", leftWindowXMargin + controlTextXPadding, controlTextYOffset - tiltrads->ibm_vga8.h - 1);
        drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "TITLE", leftWindowXMargin + controlTextXPadding, controlTextYOffset);

        drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "START", tiltrads->disp->w - rightWindowXMargin - textWidth(&(tiltrads->ibm_vga8), "START") - controlTextXPadding, controlTextYOffset - tiltrads->ibm_vga8.h - 1);
        drawText(tiltrads->disp, &(tiltrads->ibm_vga8), c540, "RESTART", tiltrads->disp->w - rightWindowXMargin - textWidth(&(tiltrads->ibm_vga8), "RESTART") - controlTextXPadding, controlTextYOffset);
    }

    tiltrads->gameoverLEDAnimCycle = ((double)tiltrads->stateTime * US_TO_MS_FACTOR) / DISPLAY_REFRESH_MS;

    // Flash the active tetrad that was the killing tetrad.
    if (tiltrads->gameoverLEDAnimCycle % 2 == 0) 
    {
        plotTetrad(tiltrads->disp, xFromGridCol(GRID_X, tiltrads->activeTetrad.topLeft.c, GRID_UNIT_SIZE),
               yFromGridRow(GRID_Y, tiltrads->activeTetrad.topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
               tiltrads->activeTetrad.shape, tiltrads->activeTetrad.type, tiltrads->activeTetrad.rotation, borderColors[tiltrads->activeTetrad.type-1], fillColors[tiltrads->activeTetrad.type-1]);
    }
    else
    {
        plotTetrad(tiltrads->disp, xFromGridCol(GRID_X, tiltrads->activeTetrad.topLeft.c, GRID_UNIT_SIZE),
               yFromGridRow(GRID_Y, tiltrads->activeTetrad.topLeft.r, GRID_UNIT_SIZE), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE,
               tiltrads->activeTetrad.shape, tiltrads->activeTetrad.type, tiltrads->activeTetrad.rotation, fillColors[tiltrads->activeTetrad.type-1], borderColors[tiltrads->activeTetrad.type-1]);
    }
}

// Helper functions.

void ttChangeState(tiltradsState_t newState)
{
    tiltradsState_t prevState = tiltrads->currState;
    tiltrads->currState = newState;
    tiltrads->stateStartTime = esp_timer_get_time();
    tiltrads->stateTime = 0;
    tiltrads->stateFrames = 0;

    // Used for cache of UI anchors.
    int16_t x0 = 0;
    int16_t x1 = 0;
    char uiStr[32] = {0};

    switch( tiltrads->currState )
    {
        case TT_TITLE:
            /*
            tiltrads->exitTimer = 0;
            tiltrads->holdingExit = false;
            */

            clearLandedTetrads();
            tiltrads->landTetradFX = false;

            // Get a random tutorial tetrad.
            initTetradRandomizer(BAG);
            tiltrads->nextTetradType = (tetradType_t)getNextTetradType(BAG, 0);
            clearGrid(GRID_COLS, GRID_ROWS, tiltrads->tutorialTetradsGrid);
            spawnNextTetrad(&(tiltrads->tutorialTetrad), BAG, 0, TUTORIAL_GRID_COLS, TUTORIAL_GRID_ROWS, tiltrads->tutorialTetradsGrid);

            // Reset the drop info to whatever is appropriate for the current level.
            tiltrads->dropTime = getDropTime(TITLE_LEVEL);
            tiltrads->dropTimer = 0;

            clearLEDs(NUM_LEDS);
            dancingLEDs(NUM_LEDS, titleColor, tiltrads->stateTime);

            // If we've come to the title from the game, stop all sound and restart title theme.
            if (prevState != TT_SCORES)
            {
                buzzer_stop();
                buzzer_play_bgm(&titleMusic);
            }

            break;
        case TT_GAME:
            // All game restart functions happen here.
            clearGrid(GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid);
            clearGrid(NEXT_GRID_COLS, NEXT_GRID_ROWS, tiltrads->nextTetradGrid);
            tiltrads->tetradCounter = 0;
            clearLandedTetrads();
            tiltrads->landTetradFX = false;
            tiltrads->linesClearedTotal = 0;
            tiltrads->linesClearedLastDrop = 0;
            tiltrads->comboCount = 0;
            tiltrads->currentLevel = 0;
            tiltrads->score = 0;
            loadHighScores();
            srand((uint32_t)(tiltrads->ttAccel.x + tiltrads->ttAccel.y * 3 + tiltrads->ttAccel.z * 5)); // Seed the random number generator.
            initTetradRandomizer(tiltrads->randomizer);
            tiltrads->nextTetradType = (tetradType_t)getNextTetradType(tiltrads->randomizer, tiltrads->tetradCounter);
            spawnNextTetrad(&(tiltrads->activeTetrad), tiltrads->randomizer, tiltrads->tetradCounter, GRID_COLS, GRID_ROWS, tiltrads->tetradsGrid);
            // Reset the drop info to whatever is appropriate for the current level.
            tiltrads->dropTime = getDropTime(tiltrads->currentLevel);
            tiltrads->dropTimer = 0;
            tiltrads->dropFXTime = 0;

            // Reset animation info.
            stopClearAnimation();

            clearLEDs(NUM_LEDS);

            buzzer_stop();
            buzzer_play_sfx(&gameStartSting);
            //buzzer_play_bgm(&gameMusic);

            break;
        case TT_SCORES:
            loadHighScores();

            tiltrads->clearScoreTimer = 0;
            tiltrads->holdingClearScore = false;

            clearLEDs(NUM_LEDS);

            alternatingPulseLEDS(NUM_LEDS, highScoreColor, tiltrads->modeTime);

            break;
        case TT_GAMEOVER:
            // Update high score if needed.
            tiltrads->newHighScore = updateHighScores(tiltrads->score);
            if (tiltrads->newHighScore)
            {
                saveHighScores();
            }

            // Get the correct offset for the high score.
            x0 = 18;
            x1 = tiltrads->disp->w - x0;
            snprintf(uiStr, sizeof(uiStr), "%d", tiltrads->score);
            tiltrads->gameoverScoreX = getCenteredTextX(&(tiltrads->ibm_vga8), uiStr, x0, x1);
            tiltrads->gameoverLEDAnimCycle = ((double)tiltrads->stateTime * US_TO_MS_FACTOR) / DISPLAY_REFRESH_MS;

            clearLEDs(NUM_LEDS);

            blinkLEDs(NUM_LEDS, gameoverColor, tiltrads->stateTime);

            buzzer_stop();
            buzzer_play_sfx(&gameOverSting);

            break;
        default:
            break;
    };
}

bool ttIsButtonPressed(uint8_t button)
{
    return ((tiltrads->ttButtonState | tiltrads->ttButtonsPressedSinceLast) & button) && !(tiltrads->ttLastButtonState & button);
}

// bool ttIsButtonReleased(uint8_t button)
// {
//     return !((tiltrads->ttButtonState | tiltrads->ttButtonsPressedSinceLast) & button) && (tiltrads->ttLastButtonState & button);
// }

bool ttIsButtonDown(uint8_t button)
{
    return (tiltrads->ttButtonState | tiltrads->ttButtonsPressedSinceLast) & button;
}

bool ttIsButtonUp(uint8_t button)
{
    return !((tiltrads->ttButtonState | tiltrads->ttButtonsPressedSinceLast) & button);
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

// void debugPrintGrid(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols])
// {
//     ESP_LOGW("EMU", "Grid Dimensions: c%d x r%d", gridCols, gridRows);
//     for (int32_t y = 0; y < gridRows; y++)
//     {
//         for (int32_t x = 0; x < gridCols; x++)
//         {
//             printf(" %2d ", gridData[y][x]);
//         }
//         printf("\n");
//     }
// }

// This assumes only complete tetrads can be rotated.
bool rotateTetrad(tetrad_t* tetrad, int32_t newRotation, uint8_t gridCols, uint8_t gridRows,
                  uint32_t gridData[][gridCols])
{
    newRotation %= NUM_ROTATIONS;
    if (newRotation < 0) 
    {
        newRotation += NUM_ROTATIONS;
    }

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
    tiltrads->dropTimer += tiltrads->deltaTime * SOFT_DROP_FACTOR;
    tiltrads->dropFXTime += tiltrads->deltaTime * SOFT_DROP_FX_FACTOR;
}

bool moveTetrad(tetrad_t* tetrad, uint8_t gridCols, uint8_t gridRows,
                uint32_t gridData[][gridCols])
{
    // 0 = min top left
    // 9 = max top left
    // 3 is the center of top left in normal tetris

    bool moved = false;

    //ESP_LOGW("EMU", "ttAccel.y: %d", tiltrads->ttAccel.y);
    int32_t yMod = tiltrads->ttAccel.y / ACCEL_SEG_SIZE;
    //ESP_LOGW("EMU", "yMod: %d", yMod);
    //ESP_LOGW("EMU", "ttAccel.delta: %d", abs(tiltrads->ttAccel.y - tiltrads->ttLastTestAccel.y));

    coord_t targetPos;
    targetPos.r = tetrad->topLeft.r;
    targetPos.c = yMod + TETRAD_SPAWN_X;

    // Save the last accel, and if didn't change by a certain threshold, then don't recalculate the value.
    // Attempt to prevent jittering for gradual movements.
    if ((targetPos.c == tetrad->topLeft.c + 1 ||
            targetPos.c == tetrad->topLeft.c - 1) &&
            abs(tiltrads->ttAccel.y - tiltrads->ttLastTestAccel.y) <= ACCEL_JITTER_GUARD)
    {
        targetPos = tetrad->topLeft;
    }
    else
    {
        tiltrads->ttLastTestAccel = tiltrads->ttAccel;
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
    *newTetrad = spawnTetrad(tiltrads->nextTetradType, currentTetradCount + 1, spawnPos, TETRAD_SPAWN_ROT);
    tiltrads->nextTetradType = (tetradType_t)getNextTetradType(randomType, currentTetradCount);

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
// int32_t getHighestActiveRow(tetrad_t* tetrad)
// {
//     int32_t highestRow = tetrad->topLeft.r;

//     for (int32_t r = TETRAD_GRID_SIZE - 1; r <= 0; r--)
//     {
//         for (int32_t c = 0; c < TETRAD_GRID_SIZE; c++)
//         {
//             if (tetrad->shape[r][c] != EMPTY)
//             {
//                 highestRow = tetrad->topLeft.r + r;
//             }
//         }
//     }

//     return highestRow;
// }

int32_t getFallDistance(tetrad_t* tetrad, uint8_t gridCols, uint8_t gridRows,
                        const uint32_t gridData[][gridCols])
{
    int32_t fallDistance = gridRows;
    int32_t currFallDistance;
    int32_t searchCol;

    // Search through every column the tetrad can occupy space in.
    for (int32_t c = 0; c < TETRAD_GRID_SIZE; c++)
    {
        // Find the lowest (closest to the floor of the playfield) occupied row in this column for the tetrad.
        int32_t lowestActiveRowInCol = -1;
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

            // If no occupied spaces, still reassign fall distance if closer than full fall height.
            if (currFallDistance < fallDistance)
            {
                fallDistance = currFallDistance;
            }

            // Check grid spaces on rows below tetrad.
            for (int32_t gr = lowestActiveRowInCol + 1; gr < gridRows; gr++)
            {
                currFallDistance = (gr - lowestActiveRowInCol - 1);

                // If occupied by other tetrad, this is where.
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


void plotSquare(display_t* disp, int16_t x0, int16_t y0, int16_t size, paletteColor_t col)
{
    plotRect(disp, x0, y0, x0 + (size - 1), y0 + (size - 1), col);
}

void plotGrid(display_t* disp, int16_t x0, int16_t y0, int16_t unitSize, uint8_t gridCols, uint8_t gridRows,
                                uint32_t gridData[][gridCols], bool clearLineAnimation, paletteColor_t col)
{
    // Draw the border.
    // The +2 moves the border down so that distinct tetrads don't clip ground.
    plotRect(disp, x0, y0, x0 + (unitSize * gridCols) + 2, y0 + (unitSize * gridRows) + 2, col);

    // Draw points for grid. (maybe disable when not debugging)
    for (int32_t y = 0; y < gridRows; y++)
    {
        // Draw lines that are cleared.
        if (clearLineAnimation && isLineCleared(y, gridCols, gridRows, gridData))
        {
            fillDisplayArea(tiltrads->disp, x0 + 1, y0 + (unitSize * y) + 1, x0 + (unitSize * gridCols), y0 + (unitSize * (y + 1)) + 1, c555);
        }

        /*for (int32_t x = 0; x < gridCols; x++)
        {
            // Draw a centered pixel on empty grid units.
            plotSquare(x0 + (x * unitSize) + 1, y0 + (y * unitSize) + 1, unitSize, c555);
            if (gridData[y][x] == EMPTY) disp->setPx(x0 + x * unitSize + (unitSize / 2), y0 + y * unitSize + (unitSize / 2), c555);
        }*/
    }
}

void plotTetrad(display_t* disp, int16_t x0, int16_t y0, int16_t unitSize, uint8_t tetradCols, uint8_t tetradRows,
                                  uint32_t shape[][tetradCols], uint8_t tetradFill, int32_t fillRotation, paletteColor_t borderColor, paletteColor_t fillColor)
{
    bool patternRotated = fillRotation % 2 != 0;
    for (int32_t y = 0; y < tetradRows; y++)
    {
        for (int32_t x = 0; x < tetradCols; x++)
        {
            if (shape[y][x] != EMPTY)
            {
                // The top left of this unit
                int16_t px = x0 + x * unitSize;
                int16_t py = y0 + y * unitSize;
                fillDisplayArea(disp, px, py, px + unitSize, py + unitSize, fillColor);
                switch (tetradFill)
                {
                    case I_TETRAD:
                        // Thatch
                        /*SET_PIXEL_BOUNDS(disp, px + 1, py + 1, col);
                        SET_PIXEL_BOUNDS(disp, px + (unitSize - 2), py + 1, col);
                        SET_PIXEL_BOUNDS(disp, px + 1, py + (unitSize - 2), col);
                        SET_PIXEL_BOUNDS(disp, px + (unitSize - 2), py + (unitSize - 2), col);*/
                        // Diagonals both
                        plotLine(disp, px, py, px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                        plotLine(disp, px, py + (unitSize - 1), px + (unitSize - 1), py, borderColor, 0);
                        break;
                    case O_TETRAD:
                        // Full walls and center dots
                        SET_PIXEL_BOUNDS(disp, px + (unitSize / 2), py + (unitSize / 2), borderColor);
                        plotSquare(disp, px, py, unitSize, borderColor);
                        break;
                    case T_TETRAD:
                        // TODO: fix the T pattern.
                        // Internal border
                        // Top
                        if (y == 0 || shape[y - 1][x] == EMPTY)
                        {
                            plotLine(disp, px, py + 1, px + (unitSize - 1), py + 1, borderColor, 0);
                        }
                        else
                        {
                            plotSquare(disp, px, py, 2, borderColor);
                            plotSquare(disp, px + (unitSize - 1) - 1, py, 2, borderColor);
                        }
                        // Bottom
                        if (y == tetradRows - 1 || shape[y + 1][x] == EMPTY)
                        {
                            plotLine(disp, px, py + (unitSize - 1) - 1, px + (unitSize - 1), py + (unitSize - 1) - 1, borderColor, 0);
                        }
                        else
                        {
                            plotSquare(disp, px, py + (unitSize - 1) - 1, 2, borderColor);
                            plotSquare(disp, px + (unitSize - 1) - 1, py + (unitSize - 1) - 1, 2, borderColor);
                        }

                        // Left
                        if (x == 0 || shape[y][x - 1] == EMPTY)
                        {
                            plotLine(disp, px + 1, py, px + 1, py + (unitSize - 1), borderColor, 0);
                        }
                        else
                        {
                            plotSquare(disp, px, py, 2, borderColor);
                            plotSquare(disp, px, py + (unitSize - 1) - 1, 2, borderColor);
                        }

                        // Right
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
                        // Diagonals up
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
                        // Diagonals down
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
                        // Diagonals up
                        if (patternRotated)
                        {
                            plotLine(disp, px, py, px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                        }
                        else
                        {
                            plotLine(disp, px, py + (unitSize - 1), px + (unitSize - 1), py, borderColor, 0);
                        }
                        break;
                    case Z_TETRAD:
                        // Diagonals down
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

                // Top
                if (y == 0 || shape[y - 1][x] == EMPTY)
                {
                    plotLine(disp, px, py, px + (unitSize - 1), py, borderColor, 0);
                }
                else
                {
                    SET_PIXEL_BOUNDS(disp, px, py, borderColor);
                    SET_PIXEL_BOUNDS(disp, px + (unitSize - 1), py, borderColor);
                }

                // Bottom
                if (y == tetradRows - 1 || shape[y + 1][x] == EMPTY)
                {
                    plotLine(disp, px, py + (unitSize - 1), px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                }
                else
                {
                    SET_PIXEL_BOUNDS(disp, px, py + (unitSize - 1), borderColor);
                    SET_PIXEL_BOUNDS(disp, px + (unitSize - 1), py + (unitSize - 1), borderColor);
                }

                // Left
                if (x == 0 || shape[y][x - 1] == EMPTY)
                {
                    plotLine(disp, px, py, px, py + (unitSize - 1), borderColor, 0);
                }
                else
                {
                    SET_PIXEL_BOUNDS(disp, px, py, borderColor);
                    SET_PIXEL_BOUNDS(disp, px, py + (unitSize - 1), borderColor);
                }

                // Right
                if (x == tetradCols - 1 || shape[y][x + 1] == EMPTY)
                {
                    plotLine(disp, px + (unitSize - 1), py, px + (unitSize - 1), py + (unitSize - 1), borderColor, 0);
                }
                else
                {
                    SET_PIXEL_BOUNDS(disp, px + (unitSize - 1), py, borderColor);
                    SET_PIXEL_BOUNDS(disp, px + (unitSize - 1), py, borderColor);
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
    // TODO: this placement code doesn't handle the dst ys correctly for values of numHorizontalLines that aren't 3.
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

void getNumCentering(font_t* font, const char* text, int16_t achorX0, int16_t anchorX1, int16_t* textX0,
                     int16_t* textX1)
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
    tiltrads->typeOrder = malloc(sizeof(list_t));
    tiltrads->typeOrder->first = NULL;
    tiltrads->typeOrder->last = NULL;
    tiltrads->typeOrder->length = 0;
}

void clearTypeOrder()
{
    // Free all ints in the list.
    node_t* current = tiltrads->typeOrder->first;
    while (current != NULL)
    {
        free(current->val);
        current->val = NULL;
        current = current->next;
    }
    // Free the node containers for the list.
    clear(tiltrads->typeOrder);
}

void deInitTypeOrder()
{
    clearTypeOrder();

    // Finally free the list itself.
    free(tiltrads->typeOrder);
    tiltrads->typeOrder = NULL;
}

void initTetradRandomizer(tetradRandomizer_t randomType)
{
    switch (randomType)
    {
        case RANDOM:
            break;
        case BAG:
            tiltrads->bagIndex = 0;
            shuffle(NUM_TETRAD_TYPES, tiltrads->typeBag);
            break;
        case POOL:
        {
            // Initialize the tetrad type pool, 5 of each type.
            for (int32_t i = 0; i < 5; i++)
            {
                for (int32_t j = 0; j < NUM_TETRAD_TYPES; j++)
                {
                    tiltrads->typePool[i * NUM_TETRAD_TYPES + j] = j + 1;
                }
            }

            // Clear the history.
            for (int32_t i = 0; i < 4; i++)
            {
                tiltrads->typeHistory[i] = 0;
            }

            // Populate the history with initial values.
            tiltrads->typeHistory[0] = S_TETRAD;
            tiltrads->typeHistory[1] = Z_TETRAD;
            tiltrads->typeHistory[2] = S_TETRAD;

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
            nextType = tiltrads->typeBag[tiltrads->bagIndex];
            tiltrads->bagIndex++;
            if (tiltrads->bagIndex >= NUM_TETRAD_TYPES)
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
                tiltrads->typeHistory[3] = nextType;
            }
            else
            {
                // The pool index of the next piece.
                int32_t i;

                // Roll for piece.
                for (int32_t r = 0; r < 6; r++)
                {
                    i = rand() % 35;
                    nextType = tiltrads->typePool[i];

                    bool inHistory = false;
                    for (int32_t h = 0; h < 4; h++)
                    {
                        if (tiltrads->typeHistory[h] == nextType)
                        {
                            inHistory = true;
                        }
                    }

                    if (!inHistory || r == 5)
                    {
                        break;
                    }

                    if (tiltrads->typeOrder->length > 0)
                    {
                        tiltrads->typePool[i] = *((int*)(tiltrads->typeOrder)->first->val);
                    }
                }

                // Update piece order.
                node_t* current = tiltrads->typeOrder->last;
                for (int32_t j = tiltrads->typeOrder->length - 1; j >= 0; j--)
                {
                    // Get the current value.
                    int* currentType = (int*)current->val;

                    // Update current in case we remove this node.
                    current = current->prev;

                    // Remove this node and free its value if it matches.
                    if (*currentType == nextType)
                    {
                        free(removeIdx(tiltrads->typeOrder, j));
                    }
                }
                int* newOrderType = malloc(sizeof(int));
                *newOrderType = nextType;
                push(tiltrads->typeOrder, newOrderType);

                tiltrads->typePool[i] = *((int*)(tiltrads->typeOrder)->first->val);

                // Update history.
                for (int32_t h = 0; h < 4; h++)
                {
                    if (h == 3)
                    {
                        tiltrads->typeHistory[h] = nextType;
                    }
                    else
                    {
                        tiltrads->typeHistory[h] = tiltrads->typeHistory[h + 1];
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
        // Pick a random index from 0 to i.
        int32_t j = rand() % (i + 1);

        // Swap array[i] with the element at random index.
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

bool updateHighScores(int32_t newScore)
{
    bool highScore = false;
    int32_t placeScore = newScore;
    for (int32_t i = 0; i < NUM_TT_HIGH_SCORES; i++)
    {
        // Get the current score at this index.
        int32_t currentScore = tiltrads->highScores[i];

        if (placeScore >= currentScore)
        {
            tiltrads->highScores[i] = placeScore;
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
        if (!readNvs32(keyStr, &(tiltrads->highScores[i])))
        {
            tiltrads->highScores[i] = 0;
        }
    }
}

void ttSetHighScores(void)
{
    char keyStr[32] = {0};
    for (int32_t i = 0; i < NUM_TT_HIGH_SCORES; i++)
    {
        snprintf(keyStr, sizeof(keyStr), "tt_high_score_%d", i);
        writeNvs32(keyStr, tiltrads->highScores[i]);
    }
}

void initLandedTetrads()
{
    tiltrads->landedTetrads = malloc(sizeof(list_t));
    tiltrads->landedTetrads->first = NULL;
    tiltrads->landedTetrads->last = NULL;
    tiltrads->landedTetrads->length = 0;
}

void clearLandedTetrads()
{
    // Free all tetrads in the list.
    node_t* current = tiltrads->landedTetrads->first;
    while (current != NULL)
    {
        free(current->val);
        current->val = NULL;
        current = current->next;
    }
    // Free the node containers for the list.
    clear(tiltrads->landedTetrads);
}

void deInitLandedTetrads()
{
    clearLandedTetrads();
    tiltrads->landTetradFX = false;

    // Finally free the list itself.
    free(tiltrads->landedTetrads);
    tiltrads->landedTetrads = NULL;
}

void startClearAnimation(int32_t numLineClears __attribute__((unused)))
{
    tiltrads->inClearAnimation = true;
    tiltrads->clearTimer = 0;
    tiltrads->clearTime = CLEAR_LINES_ANIM_TIME;

    singlePulseLEDs(NUM_LEDS, clearColor, 0.0);
}

void stopClearAnimation()
{
    tiltrads->inClearAnimation = false;
    tiltrads->clearTimer = 0;
    tiltrads->clearTime = 0;

    singlePulseLEDs(NUM_LEDS, clearColor, 1.0);
}

int64_t getDropTime(int64_t level)
{
    int64_t dropTimeFrames = 0;

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

double getDropFXTimeFactor(int64_t level)
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
    bool lineClear = true;
    for (int32_t c = 0; c < gridCols; c++)
    {
        if (gridData[line][c] == EMPTY)
        {
            lineClear = false;
        }
    }
    return lineClear;
}

int32_t checkLineClears(uint8_t gridCols, uint8_t gridRows, uint32_t gridData[][gridCols],
                        list_t* fieldTetrads)
{
    // Refresh the tetrads grid before checking for any clears.
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
    // Refresh the tetrads grid before checking for any clears.
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
                                // NOTE: What if it cannot be moved down anymore in its local grid? Can this happen? I don't think that's possible.
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

// What is the best way to handle collisions above the grid space?
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

// A color is puled all LEDs according to the type of clear.
void singlePulseLEDs(uint8_t numLEDs, led_t fxColor, double progress)
{
    double lightness = 1.0 - (progress * progress);

    for (int32_t i = 0; i < numLEDs; i++)
    {
        tiltrads->leds[i].r = (uint8_t)((double)fxColor.r * lightness);
        tiltrads->leds[i].g = (uint8_t)((double)fxColor.g * lightness);
        tiltrads->leds[i].b = (uint8_t)((double)fxColor.b * lightness);
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(tiltrads->leds, NUM_LEDS);
}

// Blink red in sync with OLED gameover FX.
void blinkLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time)
{
    // TODO: there are instances where the red flashes on the opposite of the fill draw, how to ensure this does not happen?
    uint32_t animCycle = ((double)time * US_TO_MS_FACTOR) / DISPLAY_REFRESH_MS;
    bool lightActive = animCycle % 2 == 0;

    for (int32_t i = 0; i < numLEDs; i++)
    {
        tiltrads->leds[i].r = lightActive ? fxColor.r : 0x00;
        tiltrads->leds[i].g = lightActive ? fxColor.g : 0x00;
        tiltrads->leds[i].b = lightActive ? fxColor.b : 0x00;
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(tiltrads->leds, NUM_LEDS);
}

// Alternate lit up like a bulb sign.
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

    for (int32_t i = 0; i < numLEDs; i++)
    {
        bool risingLED = i % 2 == 0;
        tiltrads->leds[i].r = risingLED ? (uint8_t)risingR : (uint8_t)fallingR;
        tiltrads->leds[i].g = risingLED ? (uint8_t)risingG : (uint8_t)fallingG;
        tiltrads->leds[i].b = risingLED ? (uint8_t)risingB : (uint8_t)fallingB;
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(tiltrads->leds, NUM_LEDS);
}

// Radial wanderers.
void dancingLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time)
{
    uint32_t animCycle = ((double)time * US_TO_MS_FACTOR * 2.0) / DISPLAY_REFRESH_MS;
    int32_t firstIndex = animCycle % numLEDs;
    int32_t secondIndex = (firstIndex + (numLEDs / 2)) % numLEDs;

    for (int32_t i = 0; i < numLEDs; i++)
    {
        tiltrads->leds[i].r = i == firstIndex || i == secondIndex ? fxColor.r : 0x00;
        tiltrads->leds[i].g = i == firstIndex || i == secondIndex ? fxColor.g : 0x00;
        tiltrads->leds[i].b = i == firstIndex || i == secondIndex ? fxColor.b : 0x00;
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(tiltrads->leds, NUM_LEDS);
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
            tiltrads->leds[i].r = fxColor.r;
            tiltrads->leds[i].g = fxColor.g;
            tiltrads->leds[i].b = fxColor.b;
        }
        else if (i == numLitLEDs)
        {
            tiltrads->leds[i].r = (uint8_t)((double)fxColor.r * modProgress);
            tiltrads->leds[i].g = (uint8_t)((double)fxColor.g * modProgress);
            tiltrads->leds[i].b = (uint8_t)((double)fxColor.b * modProgress);
        }
        else
        {
            tiltrads->leds[i].r = 0x00;
            tiltrads->leds[i].g = 0x00;
            tiltrads->leds[i].b = 0x00;
        }
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(tiltrads->leds, NUM_LEDS);
}

void clearLEDs(uint8_t numLEDs)
{
    for (int32_t i = 0; i < numLEDs; i++)
    {
        tiltrads->leds[i].r = 0x00;
        tiltrads->leds[i].g = 0x00;
        tiltrads->leds[i].b = 0x00;
    }

    setLeds(tiltrads->leds, NUM_LEDS);
}

void applyLEDBrightness(uint8_t numLEDs, double brightness)
{
    // Best way would be to convert to HSV and then set, is this factor method ok?

    for (int32_t i = 0; i < numLEDs; i++)
    {
        tiltrads->leds[i].r = (uint8_t)((double)tiltrads->leds[i].r * brightness);
        tiltrads->leds[i].g = (uint8_t)((double)tiltrads->leds[i].g * brightness);
        tiltrads->leds[i].b = (uint8_t)((double)tiltrads->leds[i].b * brightness);
    }
}
