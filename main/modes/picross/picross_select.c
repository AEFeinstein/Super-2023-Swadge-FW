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
void drawWsgScaled(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff);
//====
// Functions
//====

//Initiation
void picrossStartLevelSelect(display_t* disp, font_t* font, picrossLevelDef_t levels[], picrossSelectLevelFunc_t* selectLevelFunc)
{
    ls = calloc(1, sizeof(picrossLevelSelect_t));
    ls->disp = disp;
    ls->game_font = font;
    ls->selectLevel = (picrossSelectLevelFunc_t*)selectLevelFunc;
    //8 is numLevels
    ls->levelCount = 8;
    loadWsg("unknownPuzzle.wsg",&ls->unknownPuzzle);
    //Load in which levels have been completed.
    int32_t victories = 0;
    //picross_Solves1 would be for levels 33->64
    readNvs32("picross_Solves0", &victories);

    for(int i = 0;i<ls->levelCount;i++)
    {
        //set completed data from save data.
        if(1== (victories & ( 1 << i )) >> i){
            levels[i].completed = true;
        }else{
            levels[i].completed = false;
        }

        ls->levels[i] = levels[i];
    }
    
    ls->hoverX = 0;
    ls->hoverY = 0;
    ls->hoverLevelIndex = 0;
    ls->prevBtnState = '0xbFFFFFFFF';//when entering this mode, the button may still be held, so we dont want to activate an imput instantly.
    ls->btnState = 0;
    
    //todo: where to store that?

    //visual settings
    ls->cols = 4;
    ls->paddingLeft = 40;
    ls->paddingTop = 50;
    ls->gap = 5;
    ls->gridScale = 40;

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
        //actually we dont do this.
        // picrossExitLevelSelect();
        return;
    }
    //Input Movement checks
    if (ls->btnState & RIGHT && !(ls->prevBtnState & RIGHT))
    {
        ls->hoverX++;
        if(ls->hoverX >= ls->cols)
        {
            ls->hoverX = 0;
        }
    }else if (ls->btnState & LEFT && !(ls->prevBtnState & LEFT))
    {
        ls->hoverX--;
        if(ls->hoverX < 0)
        {
            ls->hoverX = ls->cols-1;
        }
    }else if (ls->btnState & DOWN && !(ls->prevBtnState & DOWN))
    {
        ls->hoverY++;
        if(ls->hoverY >= ls->cols)//todo: rows?
        {
            ls->hoverY = 0;
        }
    }
    else if (ls->btnState & UP && !(ls->prevBtnState & UP))
    {
        ls->hoverY--;
        if(ls->hoverY < 0)
        {
            ls->hoverY = ls->cols-1;
        }
    }

    ls->hoverLevelIndex = ls->hoverY*ls->cols+ls->hoverX;

    ls->prevBtnState = ls->btnState;
}

void drawLevelSelectScreen(display_t* d,font_t* font)
{
    d->clearPx();
    uint8_t s = ls->gridScale;//scale
    uint8_t x;
    uint8_t y;

    //todo: Draw Choose Level Text.
    drawText(d, font, c555, "Select", 158, 30); 
    drawText(d, font, c555, "Level", 158, 60);   
  
    //dont have the number of levels stored anywhere... 16 is goal, not 8.
    for(int i=0;i<ls->levelCount;i++)
    {
        y = i / ls->cols;
        x = 0;
        if(i!=0){
            x = (i%ls->cols);
        }
        x = x * s + ls->paddingLeft + ls->gap*x;
        y = y * s + ls->paddingTop + ls->gap*y;
        if(ls->levels[i].completed)
        {
            drawWsgScaled(d,&ls->levels[i].completedWSG,x >> 1,y >> 1);
        }else
        {
            //Draw ? sprite
            drawWsgScaled(d,&ls->unknownPuzzle,x >> 1,y >> 1);
        }
    }
    
    //draw level choose input
    x = ls->hoverX;
    y = ls->hoverY;
    
    box_t inputBox =
    {
        .x0 = (x * s) + ls->paddingLeft+ ls->gap*x,
        .y0 = (y * s) + ls->paddingTop+ ls->gap*y,
        .x1 = (x * s) + s + ls->paddingLeft+ ls->gap*x,
        .y1 = (y * s) + s + ls->paddingTop+ ls->gap*y,
    }; 
    drawBox(d,inputBox,c500,false,1);

    //Draw "name" of current level
    char letter[1];
    sprintf(letter, "%d", ls->hoverLevelIndex+1);//this function appears to modify hintbox.x0
    //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
    drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], 158, 120);
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
        // free(ls->unknownPuzzle);
        free(ls);
        ls = NULL;
    }
}

//Little replacement of drawWsg to draw the 2px for each px. Sprite scaling doesnt appear to be a thing yet, so this is my hack.
//we could do 4x and have plenty of room?
void drawWsgScaled(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff)
{
    if(NULL == wsg->px)
    {
        return;
    }

    // Draw the image's pixels
    for(int16_t srcY = 0; srcY < wsg->h; srcY++)
    {
        for(int16_t srcX = 0; srcX < wsg->w; srcX++)
        {
            // Draw if not transparent
            if (cTransparent != wsg->px[(srcY * wsg->w) + srcX])
            {
                // Transform this pixel's draw location as necessary
                int16_t dstX = srcX*2+xOff;
                int16_t dstY = srcY*2+yOff;
                
                // Check bounds
                if(0 <= dstX && dstX < disp->w && 0 <= dstY && dstY <= disp->h)
                {
                    // Draw the pixel
                    disp->setPx(dstX, dstY, wsg->px[(srcY * wsg->w) + srcX]);
                    disp->setPx(dstX+1, dstY, wsg->px[(srcY * wsg->w) + srcX]);
                    disp->setPx(dstX, dstY+1, wsg->px[(srcY * wsg->w) + srcX]);
                    disp->setPx(dstX+1, dstY+1, wsg->px[(srcY * wsg->w) + srcX]);
                }
            }
        }
    }
}