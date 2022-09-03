#ifndef _MODE_PICROSS_H_
#define _MODE_PICROSS_H_

#include "swadgeMode.h"
#include "aabb_utils.h"

typedef enum
{
    SPACE_EMPTY = 0,
    SPACE_FILLED = 1,
    SPACE_MARKEMPTY = 2,
    SPACE_HINT = 3,
} picrossSpaceType_t;

typedef enum
{
    PICROSS_ENTRY,
    PICROSS_WINSTAGE,
} picrossGamePhase_t;

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t dx;
    uint16_t dy;
    uint8_t row;
    uint8_t column;
    uint8_t block;
    uint8_t dBlock;
    bool flipped;
    uint32_t btnState;

} picrossInput_t;


typedef struct
{
    uint8_t width;
    uint8_t height;
    picrossInput_t inputSquare;
    picrossSpaceType_t level[10][10]; 
} picrossPuzzle_t;

typedef struct
{
    picrossGamePhase_t currentPhase;
    display_t* d;
    font_t game_font;
    font_t* promptFont;
    picrossPuzzle_t* puzzle;
    bool controlsEnabled;

} picrossGame_t;

void picrossStartGame(display_t* disp, font_t* mmFont);
void picrossGameLoop(int64_t elapsedUs);
void picrossGameButtonCb(buttonEvt_t* evt);
void picrossExitGame(void);

#endif