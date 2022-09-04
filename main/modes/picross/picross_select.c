//==============================================================================
// Includes
//==============================================================================

#include <string.h>
#include <stdlib.h>

#include "swadgeMode.h"
#include "esp_log.h"

#include "aabb_utils.h"
#include "nvs_manager.h"

#include "picross_select.h"
#include "picross_menu.h"
#include "bresenham.h"

//====
// globals, basically
//====
picrossLevelSelect_t* ls;

//===
//Function Prototypes
//
void levelSelectInput(void);
void drawLevelSelectScreen(display_t* d,font_t* font);
//====
// Functions
//====

//Initiation
void picrossStartLevelSelect(display_t* disp, font_t* font, picrossLevelDef_t levels[], picrossSelectLevelFunc_t* selectLevelFunc)
{
    ls = calloc(1, sizeof(picrossLevelSelect_t));
    ls->disp = disp;
    ls->game_font = font;
    ls->selectLevel = selectLevelFunc;

    //8 is numLevels
    for(int i = 0;i<8;i++)
    {
        ls->levels[i] = levels[i];
    }
    
    ls->hoverX = 0;
    ls->hoverY = 0;
    ls->hoverLevelIndex = 0;
    ls->gridScale = 32;
}


void picrossLevelSelectLoop(int64_t elapsedUs)
{
    
    //Draw The Screen
    drawLevelSelectScreen(ls->disp,ls->game_font);

    //Handle Input
    //has to happen last so we can free up on exit.
    //todo: make a (free/exit) bool flag.
    levelSelectInput();
    
}

void levelSelectInput()
{
    //Choosing a Level
    if (ls->btnState & BTN_A && !(ls->prevBtnState & BTN_A))
    {
        ls->chosenLevel = &ls->levels[ls->hoverLevelIndex];
        ls->selectLevel(ls->chosenLevel);
        picrossExitLevelSelect();
        return;
    }
    //Input Movement checks
    if (ls->btnState & UP && !(ls->prevBtnState & UP))
    {
        ls->hoverLevelIndex++;
        if(ls->hoverLevelIndex >= 8)
        {
            ls->hoverLevelIndex = 0;
        }
    }else if (ls->btnState & DOWN && !(ls->prevBtnState & DOWN))
    {
        
        if(ls->hoverLevelIndex == 0)
        {
            ls->hoverLevelIndex = 7;
        }else
        {
            ls->hoverLevelIndex--;
        }
    }

    ls->prevBtnState = ls->btnState;
}

void drawLevelSelectScreen(display_t* d,font_t* font)
{
    d->clearPx();

     char letter[1];
    sprintf(letter, "%d", ls->hoverLevelIndex+1);//this function appears to modify hintbox.x0
    //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
    drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], 100, 100);
}

void picrossLevelSelectButtonCb(buttonEvt_t* evt)
{
    ls->btnState = evt->state;
}

void picrossExitLevelSelect()
{
    if (NULL != ls)
    {
        // freeFont((ls->game_font));
        // free(&ls->chosenLevel);
        // free(&ls->levels);
        free(ls);
        ls = NULL;
    }
}