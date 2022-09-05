#ifndef _MODE_PICROSS_H_
#define _MODE_PICROSS_H_

#include "swadgeMode.h"
#include "aabb_utils.h"
#include "picross_select.h"
typedef enum
{
    SPACE_EMPTY = 0,
    SPACE_FILLED = 1,
    SPACE_MARKEMPTY = 2,
    OUTOFBOUNDS = 3
} picrossSpaceType_t;

typedef enum
{
    PICROSS_SOLVING = 0,
    PICROSS_YOUAREWIN = 1,
} picrossGamePhase_t;

typedef enum
{
    PICROSSCOUNTER_IDLE = 0,
    PICROSSCOUNTER_LEFT = 1,
    PICROSSCOUNTER_RIGHT =2,
    PICROSSCOUNTER_DOWN = 3,
    PICROSSCOUNTER_UP =4,
    
} counterState_t;

typedef struct
{
    /* data */
    int32_t banks[8];
} picrossSaveData_t;


typedef struct
{
    uint8_t x;
    uint8_t y;
    uint16_t prevBtnState;
    uint16_t btnState;
    bool movedThisFrame;
    bool changedLevelThisFrame;
} picrossInput_t;

typedef struct
{
    bool complete;
    bool isRow;
    uint8_t index;
    uint8_t hints[5];//have to deal with 'flexible array member'
} picrossHint_t;

typedef struct
{
    uint8_t width;
    uint8_t height;
    picrossHint_t rowHints[10];
    picrossHint_t colHints[10];
    picrossSpaceType_t completeLevel[10][10]; 
    picrossSpaceType_t level[10][10]; 
} picrossPuzzle_t;

typedef struct
{
    picrossGamePhase_t previousPhase;
    picrossGamePhase_t currentPhase;
    display_t* d;
    font_t hint_font;
    picrossPuzzle_t* puzzle;
    bool controlsEnabled;
    picrossInput_t* input;
    uint8_t drawScale;
    uint8_t leftPad;
    uint8_t topPad;
    uint8_t clueGap;
    picrossLevelDef_t* selectedLevel;
    bool exitThisFrame;
    int8_t count;
    counterState_t countState;
    picrossSaveData_t* save;
} picrossGame_t;

void picrossStartGame(display_t* disp, font_t* mmFont, picrossLevelDef_t* selectedLevel, bool cont);
void picrossGameLoop(int64_t elapsedUs);
void picrossGameButtonCb(buttonEvt_t* evt);
void picrossExitGame(void);
void loadPicrossProgress(void);
void savePicrossProgress(void);
char * getBankName(int i);

#endif