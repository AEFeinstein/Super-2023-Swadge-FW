#ifndef _MODE_PICROSSLEVELSELECT_H_
#define _MODE_PICROSSLEVELSELECT_H_

#include "swadgeMode.h"
#include "aabb_utils.h"

typedef struct {
    uint8_t index;
    wsg_t levelWSG;
    wsg_t* completedWSG;
    // bool completed;
} picrossLevelDef_t;

typedef void (*picrossSelectLevelFunc_t)(picrossLevelDef_t* level);

typedef struct
{
    display_t* disp;
    font_t* game_font;
    picrossLevelDef_t levels[8];   
    picrossLevelDef_t* chosenLevel;
    uint8_t gridScale;
    uint8_t hoverLevelIndex;
    uint8_t hoverX;
    uint8_t hoverY;
    uint16_t prevBtnState;
    uint16_t btnState;
    picrossSelectLevelFunc_t selectLevel;
} picrossLevelSelect_t;



void picrossStartLevelSelect(display_t* disp, font_t* mmFont, picrossLevelDef_t levels[], picrossSelectLevelFunc_t* selectLevelFunc);
void picrossLevelSelectLoop(int64_t elapsedUs);
void picrossLevelSelectButtonCb(buttonEvt_t* evt);
void picrossExitLevelSelect(void);

#endif