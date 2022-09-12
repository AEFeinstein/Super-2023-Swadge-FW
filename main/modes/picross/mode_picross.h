#ifndef _MODE_PICROSS_H_
#define _MODE_PICROSS_H_

#include "swadgeMode.h"
#include "aabb_utils.h"
#include "picross_select.h"
#include "led_util.h"

typedef enum
{
    SPACE_EMPTY = 0,//00
    SPACE_FILLED = 1,//01
    SPACE_MARKEMPTY = 2,//10
    OUTOFBOUNDS = 3//11
} picrossSpaceType_t;

typedef enum
{
    PICROSS_SOLVING = 0,
    PICROSS_YOUAREWIN = 1,
} picrossGamePhase_t;

typedef enum
{
    PICROSSDIR_IDLE = 0,
    PICROSSDIR_LEFT = 1,
    PICROSSDIR_RIGHT =2,
    PICROSSDIR_DOWN = 3,
    PICROSSDIR_UP =4,
} picrossDir_t;//this could be made generic and used for counter or 

//todo: remove this
typedef struct
{
    /* data */
    int32_t banks[8];
} picrossSaveData_t;

typedef struct
{
    picrossSpaceType_t startHeldType;
    uint8_t x;
    uint8_t y;
    uint16_t prevBtnState;
    uint16_t btnState;
    bool movedThisFrame;
    bool changedLevelThisFrame;
    int64_t timeHeldDirection;
    picrossDir_t holdingDir;
    int64_t DASTime;
    int64_t firstDASTime;
    paletteColor_t inputBoxColor;
    paletteColor_t inputBoxDefaultColor;
    paletteColor_t inputBoxErrorColor;
    //blinking
    bool blinkError;
    uint64_t blinkAnimTimer;
    uint64_t blinkTime;//half a blink cycle (on)(off) or full (on/off)(on/off)?
    uint8_t blinkCount;
    bool showHints;
    bool DASActive;//true after the first DAS input has happened.
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
    picrossDir_t countState;
    picrossSaveData_t* save;
    led_t errorALEDBlinkLEDS[NUM_LEDS];
    led_t errorBLEDBlinkLEDS[NUM_LEDS];
    led_t offLEDS[NUM_LEDS];

} picrossGame_t;

void picrossStartGame(display_t* disp, font_t* mmFont, picrossLevelDef_t* selectedLevel, bool cont);
void picrossGameLoop(int64_t elapsedUs);
void picrossGameButtonCb(buttonEvt_t* evt);
void picrossExitGame(void);
void loadPicrossProgress(void);
void savePicrossProgress(void);

char * getBankName(int i);

#endif