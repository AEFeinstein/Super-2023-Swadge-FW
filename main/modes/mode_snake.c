/*
 * mode_snake.c
 *
 *  Created on: Jul 28, 2018
 *      Author: adam
 */

/*==============================================================================
 * Includes
 *============================================================================*/

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "esp_random.h"
#include "esp_timer.h"

#include "musical_buzzer.h"
#include "ssd1306.h"
#include "bresenham.h"
#include "font.h"
#include "led_util.h"
#include "nvs_manager.h"

#include "mode_snake.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define SPRITE_DIM 4
#define SNAKE_FIELD_OFFSET_X 24
#define SNAKE_FIELD_OFFSET_Y 14
#define SNAKE_FIELD_WIDTH  SPRITE_DIM * 20
#define SNAKE_FIELD_HEIGHT SPRITE_DIM * 11
#define SNAKE_TEXT_OFFSET_Y 5
#define SNAKE_INITIAL_LEN 7

/*==============================================================================
 * Enums
 *============================================================================*/

typedef enum
{
    S_UP    = 0,
    S_RIGHT = 1,
    S_DOWN  = 2,
    S_LEFT  = 3,
    S_NUM_DIRECTIONS
} dir_t;

typedef enum
{
    MODE_MENU,
    MODE_GAME,
    MODE_GAME_OVER_BLINK
} snakeGameMode_t;

typedef enum
{
    EASY = 0,
    MEDIUM,
    HARD,
    NUM_DIFFICULTIES
} snakeDifficulty_t;

typedef enum
{
    HEAD_UP              = 0b0000011001101010,
    HEAD_RIGHT           = 0b1000011011100000,
    HEAD_DOWN            = 0b1010011001100000,
    HEAD_LEFT            = 0b0001011001110000,
    EATH_UP              = 0b0000100101101010,
    EATH_RIGHT           = 0b1010010011000010,
    EATH_DOWN            = 0b1010011010010000,
    EATH_LEFT            = 0b0101001000110100,
    BODY_UP              = 0b0110001001000110,
    BODY_RIGHT           = 0b0000110110110000,
    BODY_DOWN            = 0b0110010000100110,
    BODY_LEFT            = 0b0000101111010000,
    TAIL_UP              = 0b0110011000100010,
    TAIL_RIGHT           = 0b0000001111110000,
    TAIL_DOWN            = 0b0010001001100110,
    TAIL_LEFT            = 0b0000110011110000,
    BODY_UP_FAT          = 0b0110101111010110,
    BODY_RIGHT_FAT       = 0b0110110110110110,
    BODY_DOWN_FAT        = 0b0110110110110110,
    BODY_LEFT_FAT        = 0b0110101111010110,
    CORNER_UPRIGHT       = 0b0000001101010110,
    CORNER_UPLEFT        = 0b0000110010100110,
    CORNER_DOWNRIGHT     = 0b0110010100110000,
    CORNER_DOWNLEFT      = 0b0110101011000000,
    CORNER_RIGHTUP       = 0b0110101011000000,
    CORNER_RIGHTDOWN     = 0b0000110010100110,
    CORNER_LEFTUP        = 0b0110010100110000,
    CORNER_LEFTDOWN      = 0b0000001101010110,
    CORNER_UPRIGHT_FAT   = 0b0000001101010111,
    CORNER_UPLEFT_FAT    = 0b0000110010101110,
    CORNER_DOWNRIGHT_FAT = 0b0111010100110000,
    CORNER_DOWNLEFT_FAT  = 0b1110101011000000,
    CORNER_RIGHTUP_FAT   = 0b1110101011000000,
    CORNER_RIGHTDOWN_FAT = 0b0000110010101110,
    CORNER_LEFTUP_FAT    = 0b0111010100110000,
    CORNER_LEFTDOWN_FAT  = 0b0000001101010111,
    FOOD                 = 0b0100101001000000,
} snakeSprite;

typedef enum
{
    bug1 = 0b00111111101110101100111111010101,
    bug2 = 0b11001100001100000100111011111010,
    bug4 = 0b01011011111100100100111011110100,
    bug3 = 0b00001001101101111100101011101111,
    bug5 = 0b00001000111101010000000011110101,
} critterSprite;

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    uint8_t x;
    uint8_t y;
} pos_t;

typedef struct _snakeNode_t
{
    snakeSprite sprite;
    pos_t pos;
    dir_t dir;
    uint8_t ttl;
    uint8_t isFat;
    struct _snakeNode_t* prevSegment;
    struct _snakeNode_t* nextSegment;
} snakeNode_t;

/*==============================================================================
 * Constant Data
 *============================================================================*/

const song_t MetalGear =
{
    .notes = {
        {.note = E_5, .timeMs = 480},
        {.note = D_5, .timeMs = 480},
        {.note = C_5, .timeMs = 960},
        {.note = SILENCE, .timeMs = 240},
        {.note = D_5, .timeMs = 240},
        {.note = E_5, .timeMs = 240},
        {.note = A_4, .timeMs = 240},
        {.note = E_5, .timeMs = 480},
        {.note = D_5, .timeMs = 960},
        {.note = C_5, .timeMs = 240},
        {.note = D_5, .timeMs = 240},
        {.note = E_5, .timeMs = 960},
        {.note = SILENCE, .timeMs = 240},
        {.note = A_5, .timeMs = 240},
        {.note = G_5, .timeMs = 240},
        {.note = E_5, .timeMs = 240},
        {.note = C_5, .timeMs = 480},
        {.note = D_5, .timeMs = 960},
        {.note = E_5, .timeMs = 240},
        {.note = A_5, .timeMs = 240},
        {.note = C_6, .timeMs = 960},
        {.note = SILENCE, .timeMs = 240},
        {.note = B_5, .timeMs = 240},
        {.note = C_6, .timeMs = 240},
        {.note = D_6, .timeMs = 240},
        {.note = C_6, .timeMs = 480},
        {.note = A_5, .timeMs = 960},
        {.note = G_5, .timeMs = 240},
        {.note = A_5, .timeMs = 240},
        {.note = B_5, .timeMs = 480},
        {.note = SILENCE, .timeMs = 240},
        {.note = C_6, .timeMs = 240},
        {.note = B_5, .timeMs = 480},
        {.note = A_5, .timeMs = 240},
        {.note = G_5, .timeMs = 240},
        {.note = A_5, .timeMs = 1920},
        {.note = SILENCE, .timeMs = 480},
        {.note = SILENCE, .timeMs = 480},
        {.note = B_5, .timeMs = 480},
        {.note = A_5, .timeMs = 480},
        {.note = G_5, .timeMs = 960},
        {.note = SILENCE, .timeMs = 240},
        {.note = A_5, .timeMs = 240},
        {.note = B_5, .timeMs = 240},
        {.note = E_5, .timeMs = 240},
        {.note = B_5, .timeMs = 480},
        {.note = A_5, .timeMs = 960},
        {.note = G_5, .timeMs = 240},
        {.note = A_5, .timeMs = 240},
        {.note = B_5, .timeMs = 960},
        {.note = SILENCE, .timeMs = 240},
        {.note = E_6, .timeMs = 240},
        {.note = D_6, .timeMs = 240},
        {.note = B_5, .timeMs = 240},
        {.note = G_5, .timeMs = 480},
        {.note = A_5, .timeMs = 960},
        {.note = B_5, .timeMs = 240},
        {.note = E_6, .timeMs = 240},
        {.note = G_6, .timeMs = 960},
        {.note = SILENCE, .timeMs = 240},
        {.note = F_SHARP_6, .timeMs = 240},
        {.note = G_6, .timeMs = 240},
        {.note = A_6, .timeMs = 240},
        {.note = G_6, .timeMs = 480},
        {.note = E_6, .timeMs = 960},
        {.note = D_6, .timeMs = 240},
        {.note = E_6, .timeMs = 240},
        {.note = F_SHARP_6, .timeMs = 480},
        {.note = SILENCE, .timeMs = 240},
        {.note = G_6, .timeMs = 240},
        {.note = F_SHARP_6, .timeMs = 480},
        {.note = E_6, .timeMs = 240},
        {.note = D_6, .timeMs = 240},
        {.note = E_6, .timeMs = 1920},
        {.note = SILENCE, .timeMs = 480},
        {.note = SILENCE, .timeMs = 480},
    },
    .numNotes = 76,
    .shouldLoop = false
};

const song_t foodSfx =
{
    .notes = {
        {.note = G_5, .timeMs = 150},
        {.note = C_6, .timeMs = 150},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t critterSfx =
{
    .notes = {
        {.note = G_5, .timeMs = 100},
        {.note = A_5, .timeMs = 100},
        {.note = B_5, .timeMs = 100},
        {.note = C_6, .timeMs = 100},
    },
    .numNotes = 4,
    .shouldLoop = false
};

const song_t snakeDeathSfx =
{
    .notes = {
        {.note = A_SHARP_4, .timeMs = 666},
        {.note = A_4, .timeMs = 666},
        {.note = G_SHARP_4, .timeMs = 666},
        {.note = G_4, .timeMs = 2002},
    },
    .numNotes = 4,
    .shouldLoop = false
};

const uint32_t snakeBackground[] =
{
    0x80fe0000, 0x00000000, 0x00000000, 0x00007f01,
    0x80fe0000, 0x03ffffff, 0xffffffc0, 0x00007f01,
    0x80fe07ff, 0xffffffff, 0xffffffff, 0xffe07f01,
    0x80fe0fff, 0xffffffff, 0xffffffff, 0xfff07f01,
    0x00fe0fff, 0xffffffff, 0xffffffff, 0xfff07f00,
    0x00fe0fff, 0xffffffff, 0xffffffff, 0xfff07f00,
    0x00fe0fff, 0xffffffff, 0xffffffff, 0xfff07f00,
    0x00fe0fff, 0xffffffff, 0xffffffff, 0xfff07f00,
    0x00fe0fff, 0xffffffff, 0xffffffff, 0xfff07f00,
    0x00fe0fff, 0xffffffff, 0xffffffff, 0xfff07f00,
    0x00fe0fff, 0xffffffff, 0xffffffff, 0xfff07f00,
    0x00fe0e00, 0x00000000, 0x00000000, 0x00707f00,
    0x00fe0fff, 0xffffffff, 0xffffffff, 0xfff07f00,
    0x00fe0e00, 0x00000000, 0x00000000, 0x00707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00fe0eff, 0xffffffff, 0xffffffff, 0xff707f00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x00ff0eff, 0xffffffff, 0xffffffff, 0xff70ff00,
    0x80ff0eff, 0xffffffff, 0xffffffff, 0xff70ff01,
    0x80ff8eff, 0xffffffff, 0xffffffff, 0xff71ff01,
    0x80ff8eff, 0xffffffff, 0xffffffff, 0xff71ff01,
    0x807f8eff, 0xffffffff, 0xffffffff, 0xff71fe01,
    0x807f8eff, 0xffffffff, 0xffffffff, 0xff71fe01,
    0x807f8eff, 0xffffffff, 0xffffffff, 0xff71fe01,
    0x807f86ff, 0xffffffff, 0xffffffff, 0xff61fe01,
    0x807f86ff, 0xffffffff, 0xffffffff, 0xff61fe01,
    0x807f86ff, 0xffffffff, 0xffffffff, 0xff61fe01,
    0x807f86ff, 0xffffffff, 0xffffffff, 0xff61fe01,
    0x807f86ff, 0xffffffff, 0xffffffff, 0xff61fe01,
    0x807f86ff, 0xffffffff, 0xffffffff, 0xff61fe01,
    0x807f86ff, 0xffffffff, 0xffffffff, 0xff61fe01,
    0x807f86ff, 0xffffffff, 0xffffffff, 0xff61fe01,
    0x803f86ff, 0xffffffff, 0xffffffff, 0xff61fc01,
    0xc03fc6ff, 0xffffffff, 0xffffffff, 0xff63fc03,
    0xc03fc2ff, 0xffffffff, 0xffffffff, 0xff43fc03,
    0xc03fc2ff, 0xffffffff, 0xffffffff, 0xff43fc03,
    0xc03fc2ff, 0xffffffff, 0xffffffff, 0xff43fc03,
    0xc03fc2ff, 0xffffffff, 0xffffffff, 0xff43fc03,
    0xc01fe2ff, 0xffffffff, 0xffffffff, 0xff47f803,
    0xc01fe2ff, 0xffffffff, 0xffffffff, 0xff47f803,
    0xc01fe200, 0x00000000, 0x00000000, 0x0047f803,
    0xc01ff3ff, 0xffffffff, 0xffffffff, 0xffcff803,
    0xc01ff1ff, 0xffffffff, 0xffffffff, 0xff8ff803,
    0xc00ff0ff, 0xffffffff, 0xffffffff, 0xff0ff003,
    0xc00ff800, 0x00000000, 0x00000000, 0x001ff003,
    0xc00ffc00, 0x00000000, 0x00000000, 0x003ff003,
};

const snakeSprite spriteTransitionTable[2][4][4] =
{
    // Snake is skinny
    {
        // Head is UP
        {
            // Body is UP
            BODY_UP,
            // Body is RIGHT
            CORNER_RIGHTUP,
            // Body is DOWN
            BODY_UP,
            // Body is LEFT
            CORNER_LEFTUP,
        },
        // Head is RIGHT
        {
            // Body is UP
            CORNER_UPRIGHT,
            // Body is RIGHT
            BODY_RIGHT,
            // Body is DOWN
            CORNER_DOWNRIGHT,
            // Body is LEFT
            BODY_RIGHT,
        },
        // Head is DOWN
        {
            // Body is UP
            BODY_DOWN,
            // Body is RIGHT
            CORNER_RIGHTDOWN,
            // Body is DOWN
            BODY_DOWN,
            // Body is LEFT
            CORNER_LEFTDOWN,
        },
        // Head is LEFT
        {
            // Body is UP
            CORNER_UPLEFT,
            // Body is RIGHT
            BODY_LEFT,
            // Body is DOWN
            CORNER_DOWNLEFT,
            // Body is LEFT
            BODY_LEFT,
        }
    },
    // Snake is fat
    {
        // Head is UP
        {
            // Body is UP
            BODY_UP_FAT,
            // Body is RIGHT
            CORNER_RIGHTUP_FAT,
            // Body is DOWN
            BODY_UP_FAT,
            // Body is LEFT
            CORNER_LEFTUP_FAT,
        },
        // Head is RIGHT
        {
            // Body is UP
            CORNER_UPRIGHT_FAT,
            // Body is RIGHT
            BODY_RIGHT_FAT,
            // Body is DOWN
            CORNER_DOWNRIGHT_FAT,
            // Body is LEFT
            BODY_RIGHT_FAT,
        },
        // Head is DOWN
        {
            // Body is UP
            BODY_DOWN_FAT,
            // Body is RIGHT
            CORNER_RIGHTDOWN_FAT,
            // Body is DOWN
            BODY_DOWN_FAT,
            // Body is LEFT
            CORNER_LEFTDOWN_FAT,
        },
        // Head is LEFT
        {
            // Body is UP
            CORNER_UPLEFT_FAT,
            // Body is RIGHT
            BODY_LEFT_FAT,
            // Body is DOWN
            CORNER_DOWNLEFT_FAT,
            // Body is LEFT
            BODY_LEFT_FAT,
        }
    }
};

const snakeSprite headTransitionTable[2][4] =
{
    {
        // Head is UP
        HEAD_UP,
        // Head is RIGHT
        HEAD_RIGHT,
        // Head is DOWN
        HEAD_DOWN,
        // Head is LEFT
        HEAD_LEFT,
    },
    {
        // Head is UP
        EATH_UP,
        // Head is RIGHT
        EATH_RIGHT,
        // Head is DOWN
        EATH_DOWN,
        // Head is LEFT
        EATH_LEFT,
    }
};

const snakeSprite tailTransitionTable[4] =
{
    // Tail is UP
    TAIL_UP,
    // Tail is RIGHT
    TAIL_RIGHT,
    // Tail is DOWN
    TAIL_DOWN,
    // Tail is LEFT
    TAIL_LEFT,
};

const critterSprite critterSprites[5] =
{
    bug1,
    bug2,
    bug3,
    bug4,
    bug5
};

const char snakeDifficultyNames[NUM_DIFFICULTIES][8] =
{
    "Easy",
    "Med",
    "Hard"
};

const uint32_t snakeDifficulties[3][2] =
{
    // us per frame, score multiplier
    {180000, 5}, // Easy
    {90000, 9},  // Medium
    {60000, 13},  // Hard
};

const char snakeTitle[] = "Snake!!";
const char snakeGameOver[] = "Game Over %d";

/*==============================================================================
 * Function prototypes
 *============================================================================*/

// Swadge mode callbacks
void snakeInit(void);
void snakeMainLoop(int64_t elapsedUs);
void snakeDeinit(void);
void snakeButtonCallback(uint8_t state, int button, int down);

// Functions for managing game state
void snakeResetGame(void);
void snakeMoveSnake(void);
void snakePlaceCritter(void);
void snakePlaceFood(void);
void snakeProcessGame(void* arg);

// Utility functions for the game
void snakeAddNode(uint8_t ttl);
bool isOccupiedBySnake(uint8_t x, uint8_t y, snakeNode_t* node, bool checkFood);
bool isFoodAheadOfHead(void);
void snakeMoveSnakePos(pos_t* pos, dir_t dir);
inline uint8_t wrapIdx(uint8_t idx, int8_t delta, uint8_t max);

// Functions for drawing the game or menu
void snakeClearDisplay(void);
void snakeDrawBackground(void);
void snakeDrawSprite(uint8_t x, uint8_t y, snakeSprite sprite, bool isOffset);
void snakeDrawSnake(void);
void snakeDrawCritter(void);
void snakeDrawFood(void);
void snakeDrawMenu(void);
void snakeSetLeds(void);

void snakeBlinkField(void* arg __attribute__((unused)));

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode snakeMode =
{
    .modeName = "Snake",
    .fnEnterMode = snakeInit,
    .fnExitMode = snakeDeinit,
    .fnMainLoop = snakeMainLoop,
    .fnButtonCallback = snakeButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    // .menuImageData = mnu_snake_0,
    // .menuImageLen = sizeof(mnu_snake_0)
};

struct
{
    // Generic State
    snakeGameMode_t mode;

    // Menu State
    snakeDifficulty_t cursorPos;
    char title[16];

    // Game State
    snakeNode_t* snakeList;
    uint16_t length;
    dir_t dir;
    pos_t posFood;
    uint16_t foodEaten;
    pos_t posCritter;
    critterSprite cSprite;
    uint16_t lastCritterAt;
    uint8_t critterTimerCount;
    uint32_t score;
    uint16_t scoreMultiplier;
    uint8_t ledBlinkCountdown;

    // Blinking State
    esp_timer_handle_t timerHandeleSnakeBlink;
    uint8_t numBlinks;
    bool printUnlock;
} snake;

/*==============================================================================
 * Swadge Callback Functions
 *============================================================================*/

/**
 * Initialize by zeroing out everything, drawing the frame, and drawing the menu
 */
void snakeInit(void)
{
    // Clear everything
    memset(&snake, 0, sizeof(snake));

    // Reset game variables
    snakeResetGame();

    // Set up timers
    esp_timer_create_args_t blinkArgs =
    {
        .callback = snakeBlinkField,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sn_blink",
        .skip_unhandled_events = false,
    };
    esp_timer_create(&blinkArgs, &snake.timerHandeleSnakeBlink);

    // Init high scores if they can't be read
    int32_t hs;
    if(false == readNvs32("snakehs0", &hs))
    {
        writeNvs32("snakehs0", 0);
        writeNvs32("snakehs1", 0);
        writeNvs32("snakehs2", 0);
    }

    // Draw the border
    snakeDrawBackground();

    // Set up and draw the menu
    memcpy(snake.title, snakeTitle, sizeof(snakeTitle));
    snake.cursorPos = 0;
    snake.mode = MODE_MENU;
    snake.numBlinks = 0;
    snake.printUnlock = false;
    snakeDrawMenu();
}

/**
 * Deinitialize this by disarming all timers and freeing memory
 */
void snakeDeinit(void)
{
    esp_timer_stop(snake.timerHandeleSnakeBlink);

    snakeNode_t* snakePtr = snake.snakeList;
    while(NULL != snakePtr)
    {
        snakeNode_t* nextPtr = snakePtr->nextSegment;
        free(snakePtr);
        snakePtr = nextPtr;
    }
}

/**
 * @brief TODO
 * 
 */
void snakeMainLoop(int64_t elapsedUs)
{
    switch(snake.mode)
    {
        case MODE_MENU:
        {
            break;
        }
        case MODE_GAME:
        {
            // Start a software timer to run at some interval, based on the difficult
            // esp_timer_stop(snake.timerHandleSnakeLogic);
            // esp_timer_start_periodic(snake.timerHandleSnakeLogic, snakeDifficulties[snake.cursorPos][0]);
            static int64_t totalElapsedUs = 0;
            totalElapsedUs += elapsedUs;
            while (totalElapsedUs >= snakeDifficulties[snake.cursorPos][0])
            {
                totalElapsedUs -= snakeDifficulties[snake.cursorPos][0];
                snakeProcessGame(NULL);
            }
            break;
        }
        case MODE_GAME_OVER_BLINK:
        {
            break;
        }
    }
}

/**
 * Called whenever there is a button press. Moves the menu cursor or changes the
 * direction of the snake, depending on the current mode
 *
 * @param state A bitmask of all current button states
 * @param button The button ID that triggered this callback
 * @param down The state of the button that triggered this callback
 */
void snakeButtonCallback(uint8_t state __attribute__((unused)),
                         int button, int down)
{
    switch(snake.mode)
    {
        case MODE_MENU:
        {
            if(down)
            {
                switch(button)
                {
                    // Left button
                    case 1:
                    {
                        // Move the cursor, then draw the menu
                        snake.cursorPos = (snake.cursorPos + 1) % NUM_DIFFICULTIES;
                        snakeDrawMenu();
                        break;
                    }
                    // Right button
                    case 2:
                    {
                        // Stop the buzzer, just in case
                        // buzzer_stop();

                        // Request responsive buttons
                        // enableDebounce(false);

                        // Redraw the background to clear button funcs
                        snakeDrawBackground();

                        // Start the game
                        snake.mode = MODE_GAME;
                        snakeResetGame();

                        // Set the score multiplier based on difficulty
                        snake.scoreMultiplier = snakeDifficulties[snake.cursorPos][1];

                        // randomly place food
                        snakePlaceFood();

                        // Draw the frame
                        snakeProcessGame(NULL);

                        break;
                    }
                    default:
                    {
                        // No other buttons to handle
                        break;
                    }
                }
            }
            break;
        }
        case MODE_GAME:
        {
            if(down)
            {
                switch(button)
                {
                    // Left Button
                    case 1:
                    {
                        if(0 == snake.dir)
                        {
                            snake.dir += S_NUM_DIRECTIONS;
                        }
                        snake.dir--;
                        break;
                    }
                    // Right Button
                    case 2:
                    {
                        snake.dir = (snake.dir + 1) % S_NUM_DIRECTIONS;
                        break;
                    }
                    default:
                    {
                        // No other buttons to handle
                        break;
                    }
                }
            }
            break;
        }
        case MODE_GAME_OVER_BLINK:
        default:
        {
            // Nothing to do here
            break;
        }
    }
}

/*==============================================================================
 * Game State Functions
 *============================================================================*/

/**
 * Reset the game by freeing all memory, setting all default values, and
 * allocating a new snake
 */
void snakeResetGame(void)
{
    // Free the snake
    snakeNode_t* snakePtr = snake.snakeList;
    while(NULL != snakePtr)
    {
        snakeNode_t* nextPtr = snakePtr->nextSegment;
        free(snakePtr);
        snakePtr = nextPtr;
    }
    snake.snakeList = NULL;

    // Set all the game variables
    snake.dir = S_RIGHT;
    snake.posFood.x = -1;
    snake.posFood.y = -1;
    snake.posCritter.x = -1;
    snake.posCritter.y = -1;
    snake.cSprite = 0;
    snake.score = 0;
    snake.foodEaten = 0;
    snake.lastCritterAt = 0;
    snake.critterTimerCount = 0;
    esp_timer_stop(snake.timerHandeleSnakeBlink);

    // Build the snake
    uint8_t i;
    for(i = 0; i < SNAKE_INITIAL_LEN; i++)
    {
        snakeAddNode(SNAKE_INITIAL_LEN - i);
    }
}

/**
 * Move the snake's position, check game logic, then draw a frame
 * Called on a timer whose speed is dependent on the difficulty
 *
 * @param arg unused
 */
void snakeProcessGame(void* arg __attribute__((unused)))
{
    // Move the snake and do all the game logic
    snakeMoveSnake();

    // Run the critter timer if it's active
    if(snake.critterTimerCount > 0)
    {
        snake.critterTimerCount--;
        // Critter timed out, clear it
        if(0 == snake.critterTimerCount)
        {
            snake.posCritter.x = -1;
            snake.posCritter.y = -1;
        }
    }
    // Check if we should create a critter
    else if(0 == snake.critterTimerCount &&
            snake.lastCritterAt != snake.foodEaten &&
            snake.foodEaten % 5 == 0)
    {
        // Pick random snake.cSprite
        snake.cSprite = critterSprites[esp_random() % (sizeof(critterSprites) / sizeof(critterSprites[0]))];
        snake.critterTimerCount = 20;
        snakePlaceCritter();
        snake.lastCritterAt = snake.foodEaten;
    }

    // Clear the display, then draw everything
    snakeClearDisplay();
    snakeDrawSnake();
    snakeDrawFood();
    // Draw the critter and critter timer, if applicable
    if(snake.critterTimerCount > 0)
    {
        snakeDrawCritter();
        char critterStr[16] = {0};
        snprintf(critterStr, sizeof(critterStr), "%02d", snake.critterTimerCount);
        plotText(SNAKE_FIELD_OFFSET_X + 72, SNAKE_TEXT_OFFSET_Y, critterStr, TOM_THUMB, WHITE);
    }

    // Draw the score
    char scoreStr[16] = {0};
    snprintf(scoreStr, sizeof(scoreStr), "%04d", snake.score);
    plotText(SNAKE_FIELD_OFFSET_X, SNAKE_TEXT_OFFSET_Y, scoreStr, TOM_THUMB, WHITE);

    // Plot button funcs
    fillDisplayArea(
        0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 2,
        16, OLED_HEIGHT,
        BLACK);
    plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "LEFT", TOM_THUMB, WHITE);
    fillDisplayArea(
        OLED_WIDTH - 21, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 2,
        OLED_WIDTH, OLED_HEIGHT,
        BLACK);
    plotText(OLED_WIDTH - 19, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "RIGHT", TOM_THUMB, WHITE);
}

/**
 * Move the snake, update sprites, and check all game logic like
 * self-intersection and food-eating
 */
void snakeMoveSnake(void)
{
    // Save the old head
    snakeNode_t* oldHead = snake.snakeList;
    bool ateSomething = false;

    // create a new head and link it to the list
    snakeNode_t* newHead = (snakeNode_t*)malloc(sizeof(snakeNode_t));
    newHead->prevSegment = NULL;
    newHead->nextSegment = oldHead;
    oldHead->prevSegment = newHead;
    snake.snakeList = newHead;

    // Figure out where the new head is, and how long it has to live
    newHead->dir = snake.dir;
    newHead->pos.x = newHead->nextSegment->pos.x;
    newHead->pos.y = newHead->nextSegment->pos.y;
    snakeMoveSnakePos(&newHead->pos, newHead->dir);
    newHead->ttl = oldHead->ttl + 1;

    // Figure out the sprite based on the food location and direction
    newHead->sprite = headTransitionTable[isFoodAheadOfHead()][newHead->dir];

    // Swap sprite right behind the head and adjust it's direction to match the head
    snakeNode_t* neck = newHead->nextSegment;
    newHead->nextSegment->sprite = spriteTransitionTable[newHead->nextSegment->isFat][newHead->dir][neck->nextSegment->dir];
    neck->dir = newHead->dir;

    // See if we ate anything
    newHead->isFat = 0;
    if(newHead->pos.x == snake.posFood.x && newHead->pos.y == snake.posFood.y)
    {
        // Mmm, tasty
        newHead->isFat = 1;
        snake.foodEaten++;
        ateSomething = true;

        // Food is points
        snake.score += snake.scoreMultiplier;

        // Start the LED blinker countdown
        snake.ledBlinkCountdown = 4;

        // Play a jingle
        // buzzer_play(&foodSfx);

        // Draw a new food somewhere else
        snakePlaceFood();
    }
    else if((newHead->pos.y == snake.posCritter.y) && (newHead->pos.x == snake.posCritter.x
            || newHead->pos.x == snake.posCritter.x + SPRITE_DIM) )
    {
        // Mmm a critter
        newHead->isFat = 1;
        ateSomething = true;

        // Critters are more points
        snake.score += (snake.scoreMultiplier * snake.critterTimerCount);

        // Start the LED blinker countdown
        snake.ledBlinkCountdown = 4;
        // Play a jingle
        // buzzer_play(&critterSfx);
        // Clear the criter
        snake.critterTimerCount = 0;
        snake.posCritter.x = -1;
        snake.posCritter.y = -1;
    }

    // If anything was eaten, increment all the ttls, making the snake longer
    if(ateSomething)
    {
        snakeNode_t* snakePtr = snake.snakeList;
        while(NULL != snakePtr)
        {
            snakePtr->ttl++;
            snakePtr = snakePtr->nextSegment;
        }
    }

    // Iterate through the list, decrementing the ttls
    snakeNode_t* snakePtr = snake.snakeList;
    while(NULL != snakePtr)
    {
        snakePtr->ttl--;
        if(1 == snakePtr->ttl)
        {
            // Draw a new tail here
            snakePtr->sprite = tailTransitionTable[snakePtr->dir];
        }
        else if(0 == snakePtr->ttl)
        {
            // If this segment is done, remove it from the list and free it
            snakePtr->prevSegment->nextSegment = NULL;
            free(snakePtr);
            snakePtr = NULL;
        }

        // iterate
        if (NULL != snakePtr)
        {
            snakePtr = snakePtr->nextSegment;
        }
    }

    // Now that the snake is fully moved, check for self-collisions
    if(isOccupiedBySnake(newHead->pos.x, newHead->pos.y, snake.snakeList->nextSegment, false))
    {
        // Collided with self, game over
        // Stop the song. Losers don't get music
        // buzzer_stop();

        // If they're on hard mode and scored a lot
        if(snake.cursorPos == 2 && snake.score >= 500)
        {
            // Give 'em a reward
            // buzzer_play(&MetalGear);
        }
        else
        {
            // buzzer_play(&snakeDeathSfx);
        }

        // Save the high score
        char key[] = "snakehs0";
        key[7] += snake.cursorPos;

        int32_t oldHs;
        if(readNvs32(key, &oldHs))
        {
            if (snake.score > oldHs)
            {
                writeNvs32(key, snake.score);
            }
        }
        else
        {
            writeNvs32(key, snake.score);
        }

        // // If all three high scores are at least 500, unlock the gallery img
        // uint32_t* snakeHS = getSnakeHighScores();
        // if( snakeHS[0] >= 500 &&
        //         snakeHS[1] >= 500 &&
        //         snakeHS[2] >= 500)
        // {
        //     // 1 means Funkus Chillin
        //     if(true == unlockGallery(1))
        //     {
        //         // Print gallery unlock
        //         snake.printUnlock = true;
        //     }
        // }

        // Blink the field to indicate game over
        snake.mode = MODE_GAME_OVER_BLINK;
        esp_timer_start_periodic(snake.timerHandeleSnakeBlink, 500000);
        // os_timer_arm(&snake.timerHandeleSnakeBlink, 500, true);

        // Request debounced buttons
        // enableDebounce(true);
    }

    // finally, update the LEDs display
    // we do this last so LED state updates this tick will be applied this tick
    snakeSetLeds();
}

/**
 * Randomly place the food somewhere on the field, but not somewhere the snake
 * or critter already are
 */
void snakePlaceFood(void)
{
    // Clear the food first
    snake.posFood.x = -1;
    snake.posFood.y = -1;

    // Look for all the places the food would fit
    uint16_t possibleSlots = 0;
    for(uint8_t y = 0; y < SNAKE_FIELD_HEIGHT; y += SPRITE_DIM)
    {
        for(uint8_t x = 0; x < SNAKE_FIELD_WIDTH; x += SPRITE_DIM)
        {
            if(!isOccupiedBySnake(x, y, snake.snakeList, true))
            {
                possibleSlots++;
            }
        }
    }

    // Pick a random location
    uint16_t randPos = esp_random() % possibleSlots;
    uint16_t linearAddr = 0;

    // Place the food at that location
    for(uint8_t y = 0; y < SNAKE_FIELD_HEIGHT; y += SPRITE_DIM)
    {
        for(uint8_t x = 0; x < SNAKE_FIELD_WIDTH; x += SPRITE_DIM)
        {
            if(!isOccupiedBySnake(x, y, snake.snakeList, true))
            {
                if(randPos == linearAddr)
                {
                    snake.posFood.x = x;
                    snake.posFood.y = y;
                    return;
                }
                else
                {
                    linearAddr++;
                }
            }
        }
    }
    // TODO ultimate winner?
}

/**
 * Randomly place the critter somewhere on the field, but not somewhere the
 * snake or food already are
 */
void snakePlaceCritter(void)
{
    // Clear the critter first
    snake.posCritter.x = -1;
    snake.posCritter.y = -1;

    // Look for all the places a critter would fit
    uint16_t possibleSlots = 0;
    for(uint8_t y = 0; y < SNAKE_FIELD_HEIGHT; y += SPRITE_DIM)
    {
        for(uint8_t x = 0; x < SNAKE_FIELD_WIDTH - SPRITE_DIM; x += SPRITE_DIM)
        {
            if(!isOccupiedBySnake(x, y, snake.snakeList, true) && !isOccupiedBySnake(x + SPRITE_DIM, y, snake.snakeList, true))
            {
                possibleSlots++;
            }
        }
    }

    // Pick a random location
    uint16_t randPos = esp_random() % possibleSlots;
    uint16_t linearAddr = 0;

    // Place the critter at that location
    for(uint8_t y = 0; y < SNAKE_FIELD_HEIGHT; y += SPRITE_DIM)
    {
        for(uint8_t x = 0; x < SNAKE_FIELD_WIDTH - SPRITE_DIM; x += SPRITE_DIM)
        {
            if(!isOccupiedBySnake(x, y, snake.snakeList, true) && !isOccupiedBySnake(x + SPRITE_DIM, y, snake.snakeList, true))
            {
                if(randPos == linearAddr)
                {
                    snake.posCritter.x = x;
                    snake.posCritter.y = y;
                    return;
                }
                else
                {
                    linearAddr++;
                }
            }
        }
    }
    // TODO ultimate winner?
}

/*==============================================================================
 * Game Utility Functions
 *============================================================================*/

/**
 * @brief Add a segment to the end of the snake
 *
 * @param ttl This segment's time to live
 */
void snakeAddNode(uint8_t ttl)
{
    // If snakeList is NULL, start the snake
    if(NULL == snake.snakeList)
    {
        snake.snakeList = (snakeNode_t*)malloc(sizeof(snakeNode_t));
        snake.snakeList->sprite = HEAD_RIGHT;
        snake.snakeList->ttl = ttl;
        // Start in the middle of the display, facing right
        snake.snakeList->pos.x = SNAKE_FIELD_WIDTH / 2;
        snake.snakeList->pos.y = SNAKE_FIELD_HEIGHT / 2 + 2;
        snake.snakeList->dir = S_RIGHT;
        snake.snakeList->isFat = 0;
        snake.snakeList->prevSegment = NULL;
        snake.snakeList->nextSegment = NULL;
    }
    else
    {
        // Iterate through the list, and tack on a new segment
        snakeNode_t* snakePtr = snake.snakeList;
        while(NULL != snakePtr->nextSegment)
        {
            snakePtr = snakePtr->nextSegment;
        }
        snakePtr->nextSegment = (snakeNode_t*)malloc(sizeof(snakeNode_t));
        snakePtr->nextSegment->ttl = ttl;
        // If this has 1 frame to live, it's the tail, otherwise body
        if(1 == ttl)
        {
            snakePtr->nextSegment->sprite = TAIL_RIGHT;
        }
        else
        {
            snakePtr->nextSegment->sprite = BODY_RIGHT;
        }
        snakePtr->nextSegment->pos.x = snakePtr->pos.x - SPRITE_DIM;
        snakePtr->nextSegment->pos.y = snakePtr->pos.y;
        snakePtr->nextSegment->dir = S_RIGHT;
        snakePtr->nextSegment->isFat = 0;
        snakePtr->nextSegment->prevSegment = snakePtr;
        snakePtr->nextSegment->nextSegment = NULL;
    }
}

/**
 * @brief Check if a space is occupied by a snake or food
 *
 * @param x         The x position to check
 * @param y         The y position to check
 * @param node      A pointer to a snake list to check against
 * @param checkFood true to check if the space is occupied by food, false to ignore food
 * @return true if the space is occupied, false if it is free
 */
bool isOccupiedBySnake(uint8_t x, uint8_t y, snakeNode_t* node, bool checkFood)
{
    // Iterate over the snake, checking for occupation
    while(NULL != node)
    {
        if((node->pos.x == x) && (node->pos.y == y))
        {
            return true;
        }
        node = node->nextSegment;
    }

    // If we should check food
    if(checkFood)
    {
        // Check if the food is in this spot
        if(snake.posFood.x == x && snake.posFood.y == y)
        {
            return true;
        }
        // Or if the critter is here
        else if (snake.posCritter.y == y && (snake.posCritter.x == x || snake.posCritter.x + SPRITE_DIM == x))
        {
            return true;
        }
    }
    // Not occupied
    return false;
}

/**
 * @return true if the snake is facing food or a critter, false otherwise
 */
bool isFoodAheadOfHead(void)
{
    // Move a virtual head one space in its direction
    pos_t headPos;
    headPos.x = snake.snakeList->pos.x;
    headPos.y = snake.snakeList->pos.y;
    snakeMoveSnakePos(&headPos, snake.snakeList->dir);

    // Check if the virtual head collides with food or critter
    return isOccupiedBySnake(headPos.x, headPos.y, NULL, true);
}

/**
 * Wrap an index around a given range, kind of like the % operator, but works
 * with negative numbers
 *
 * @param idx   The number to wrap
 * @param delta The amount to increment or decrement the number
 * @param max   The max range of the number (min is 0)
 * @return The new number
 */
inline uint8_t wrapIdx(uint8_t idx, int8_t delta, uint8_t max)
{
    if(idx + delta < 0)
    {
        idx += max;
    }
    idx += delta;
    if(idx >= max)
    {
        idx -= max;
    }
    return idx;
}

/**
 * Move a pos_t in a given dir_t, taking into account wrapping of the field
 *
 * @param pos The position to move
 * @param dir The direction to move in
 */
void snakeMoveSnakePos(pos_t* pos, dir_t dir)
{
    switch(dir)
    {
        case S_UP:
        {
            pos->y = wrapIdx(pos->y, -SPRITE_DIM, SNAKE_FIELD_HEIGHT);
            break;
        }
        case S_DOWN:
        {
            pos->y = wrapIdx(pos->y, SPRITE_DIM, SNAKE_FIELD_HEIGHT);
            break;
        }
        case S_LEFT:
        {
            pos->x = wrapIdx(pos->x, -SPRITE_DIM, SNAKE_FIELD_WIDTH);
            break;
        }
        case S_RIGHT:
        {
            pos->x = wrapIdx(pos->x, SPRITE_DIM, SNAKE_FIELD_WIDTH);
            break;
        }
        case S_NUM_DIRECTIONS:
        default:
        {
            break;
        }
    }
}

/*==============================================================================
 * Functions for blinking
 *============================================================================*/

/**
 * Timer function which is called to blink the entire display after the game
 * has ended
 *
 * @param arg unused
 */
void snakeBlinkField(void* arg __attribute__((unused)))
{
    // Keep track of how many times this has been called
    snake.numBlinks++;

    // Always clear everything
    snakeClearDisplay();

    // Done blinking, back to the menu
    if(snake.numBlinks == 9)
    {
        snake.printUnlock = false;
        snake.numBlinks = 0;
        esp_timer_stop(snake.timerHandeleSnakeBlink);
        snake.mode = MODE_MENU;
        snakeDrawMenu();
    }
    else
    {
        led_t leds[NUM_LEDS] = {{0}};
        uint8_t i;
        // Draw everything every other blink
        if(snake.numBlinks % 2 != 0)
        {
            snakeDrawSnake();
            snakeDrawFood();
            if(snake.critterTimerCount > 0)
            {
                snakeDrawCritter();
            }

            // Blink Red for losing
            for(i = 0; i < NUM_LEDS; i++)
            {
                leds[i].r = 6;
                leds[i].g = 0;
                leds[i].b = 0;
            }
        }
        setLeds(leds, sizeof(leds));

        // Draw the game over text & score
        snprintf(snake.title, sizeof(snake.title), snakeGameOver, snake.score);
        plotText(SNAKE_FIELD_OFFSET_X, SNAKE_TEXT_OFFSET_Y, snake.title, TOM_THUMB, WHITE);
        if(snake.printUnlock)
        {
            fillDisplayArea(
                SNAKE_FIELD_OFFSET_X + 5,
                SNAKE_FIELD_OFFSET_Y + (SNAKE_FIELD_HEIGHT / 2) - FONT_HEIGHT_IBMVGA8 - 2 - 2,
                SNAKE_FIELD_OFFSET_X + SNAKE_FIELD_WIDTH - 1 - 5,
                SNAKE_FIELD_OFFSET_Y + (SNAKE_FIELD_HEIGHT / 2) + FONT_HEIGHT_IBMVGA8 + 1 + 2,
                BLACK);
            plotRect(
                SNAKE_FIELD_OFFSET_X + 5,
                SNAKE_FIELD_OFFSET_Y + (SNAKE_FIELD_HEIGHT / 2) - FONT_HEIGHT_IBMVGA8 - 2 - 2,
                SNAKE_FIELD_OFFSET_X + SNAKE_FIELD_WIDTH - 1 - 5,
                SNAKE_FIELD_OFFSET_Y + (SNAKE_FIELD_HEIGHT / 2) + FONT_HEIGHT_IBMVGA8 + 1 + 2,
                WHITE);
            plotText(
                SNAKE_FIELD_OFFSET_X + 8 + 4,
                SNAKE_FIELD_OFFSET_Y + (SNAKE_FIELD_HEIGHT / 2) - (FONT_HEIGHT_IBMVGA8) - 1,
                "Gallery",
                IBM_VGA_8,
                WHITE);
            plotText(
                SNAKE_FIELD_OFFSET_X + 8,
                SNAKE_FIELD_OFFSET_Y + (SNAKE_FIELD_HEIGHT / 2) + 1,
                "Unlocked",
                IBM_VGA_8,
                WHITE);
        }
    }
}

/*==============================================================================
 * Functions for drawing
 *============================================================================*/

/**
 * @brief Draw the background. Not the most efficient, but it works
 */
void snakeDrawBackground(void)
{
    uint32_t bgVal = 0;
    uint16_t bgIdx = 0;
    for(int y = 0; y < OLED_HEIGHT; y++)
    {
        for(int x = 0; x < OLED_WIDTH; x++)
        {
            if(x % 32 == 0)
            {
                bgVal = snakeBackground[bgIdx++];
            }
            if(bgVal & (0x80000000 >> (x % 32)))
            {
                drawPixel(x, y, BLACK);
            }
            else
            {
                drawPixel(x, y, WHITE);
            }
        }
    }
}

/**
 * Clear the display areas for the game and text, but don't touch the border
 */
void snakeClearDisplay(void)
{
    fillDisplayArea(SNAKE_FIELD_OFFSET_X, SNAKE_TEXT_OFFSET_Y,
                    SNAKE_FIELD_OFFSET_X + SNAKE_FIELD_WIDTH - 1,
                    SNAKE_TEXT_OFFSET_Y + FONT_HEIGHT_TOMTHUMB,
                    BLACK);

    fillDisplayArea(SNAKE_FIELD_OFFSET_X, SNAKE_FIELD_OFFSET_Y,
                    SNAKE_FIELD_OFFSET_X + SNAKE_FIELD_WIDTH - 1,
                    SNAKE_FIELD_OFFSET_Y + SNAKE_FIELD_HEIGHT - 1,
                    BLACK);
}

/**
 * Iterate through the snake list and draw each sprite
 */
void snakeDrawSnake(void)
{
    // Draw the snake
    snakeNode_t* snakePtr = snake.snakeList;
    while(NULL != snakePtr)
    {
        snakeDrawSprite(snakePtr->pos.x, snakePtr->pos.y, snakePtr->sprite, true);
        snakePtr = snakePtr->nextSegment;
    }
}

/**
 * Draw the sprite for the foot
 */
void snakeDrawFood(void)
{
    // Draw the food
    snakeDrawSprite(snake.posFood.x, snake.posFood.y, FOOD, true);
}

/**
 * Draw the sprite for the critter on both the field and the timer area.
 * It's actually two sprites side by side
 */
void snakeDrawCritter(void)
{
    // draw the critter to eat
    snakeDrawSprite(snake.posCritter.x, snake.posCritter.y,              (snake.cSprite & 0xFFFF0000) >> 16, true);
    snakeDrawSprite(snake.posCritter.x + SPRITE_DIM, snake.posCritter.y, (snake.cSprite & 0x0000FFFF), true);

    // draw the critter next to the timer
    snakeDrawSprite(84, SNAKE_TEXT_OFFSET_Y, (snake.cSprite & 0xFFFF0000) >> 16, false);
    snakeDrawSprite(88, SNAKE_TEXT_OFFSET_Y, (snake.cSprite & 0x0000FFFF), false);
}

/**
 * Draw a sprite somewhere to the OLED
 *
 * @param x        The X position to draw the sprite at
 * @param y        The Y position to draw the sprite at
 * @param sprite   The sprite to draw
 * @param isOffset true to draw relative to the game board, false to draw relative to the OLED
 */
void snakeDrawSprite(uint8_t x, uint8_t y, snakeSprite sprite, bool isOffset)
{
    // If this should be drawn relative to the game board, offset X and Y
    if(isOffset)
    {
        x += SNAKE_FIELD_OFFSET_X;
        y += SNAKE_FIELD_OFFSET_Y;
    }

    // For each pixel, draw it either white or black
    uint8_t xDraw, yDraw, spriteIdx = 15;
    for(yDraw = 0; yDraw < SPRITE_DIM; yDraw++)
    {
        for(xDraw = 0; xDraw < SPRITE_DIM; xDraw++)
        {
            if(sprite & (1 << (spriteIdx--)))
            {
                drawPixel(x + xDraw, y + yDraw, WHITE);
            }
            else
            {
                drawPixel(x + xDraw, y + yDraw, BLACK);
            }
        }
    }
}

/**
 * @brief Draw the menu
 */
void snakeDrawMenu(void)
{
    // Clear the display
    snakeClearDisplay();

    // Plot button funcs
    fillDisplayArea(
        0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 2,
        20, OLED_HEIGHT,
        BLACK);
    plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "LEVEL", TOM_THUMB, WHITE);
    fillDisplayArea(
        OLED_WIDTH - 21, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 2,
        OLED_WIDTH, OLED_HEIGHT,
        BLACK);
    plotText(OLED_WIDTH - 19, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "START", TOM_THUMB, WHITE);

    // Draw the title, whatever it is
    plotText(SNAKE_FIELD_OFFSET_X, SNAKE_TEXT_OFFSET_Y, snake.title, TOM_THUMB, WHITE);

    // Draw the text difficulties
    for(uint8_t i = 0; i < NUM_DIFFICULTIES; i++)
    {
        // Copy from ROM to RAM for plotting
        char difficulty[sizeof(snakeDifficultyNames[i])] = {0};
        memcpy(difficulty, snakeDifficultyNames[i], sizeof(snakeDifficultyNames[i]));
        plotText(8 + SNAKE_FIELD_OFFSET_X + 1,
                 SNAKE_FIELD_OFFSET_Y + 3 + i * (FONT_HEIGHT_IBMVGA8 + 3),
                 (char*) difficulty,
                 IBM_VGA_8, WHITE);
    }

    // Draw the high scores
    char tmp[8];
    for(uint8_t i = 0; i < 3; i++)
    {
        char key[] = "snakehs0";
        key[7] += i;
        int32_t snakeHs = 0;
        if(readNvs32(key, &snakeHs))
        {
            snprintf(tmp, sizeof(tmp), "%4d", snakeHs);
            plotText(42 + SNAKE_FIELD_OFFSET_X + 1,
                     SNAKE_FIELD_OFFSET_Y + 3 + i * (FONT_HEIGHT_IBMVGA8 + 3),
                     tmp,
                     IBM_VGA_8, WHITE);
        }
    }

    // Draw cursor
    plotText(SNAKE_FIELD_OFFSET_X + 1,
             SNAKE_FIELD_OFFSET_Y + 3 + snake.cursorPos * (FONT_HEIGHT_IBMVGA8 + 3),
             ">",
             IBM_VGA_8, WHITE);
}

/**
 * @brief Point some LEDs in the direction of snek
 */
void snakeSetLeds(void)
{
    led_t leds[NUM_LEDS] = {{0}};
    uint8_t i;

    if (snake.ledBlinkCountdown > 0)
    {
        for(i = 0; i < NUM_LEDS; i++)
        {
            //leds[i].r = 4;
            //leds[i].g = 6;
            leds[i].b = 6;
        }
        snake.ledBlinkCountdown--;
    }
    else
    {
        /*for(i = 0; i < NUM_LEDS; i++)
        {
            leds[i].r = 0;
            leds[i].g = 6;
            leds[i].b = 0;
        }*/
        switch(snake.dir)
        {
            case S_UP:
            {
                //leds[0].g = 4;
                leds[1].g = 4;
                //leds[2].g = 4;
                break;
            }
            case S_RIGHT:
            {
                leds[0].g = 4;
                leds[5].g = 4;
                break;
            }
            case S_DOWN:
            {
                //leds[3].g = 4;
                leds[4].g = 4;
                //leds[5].g = 4;
                break;
            }
            case S_LEFT:
            {
                leds[2].g = 4;
                leds[3].g = 4;
                break;
            }
            default:
            case S_NUM_DIRECTIONS:
            {
                break;
            }
        }
    }
    setLeds(leds, sizeof(leds));
}
