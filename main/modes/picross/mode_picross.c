//==============================================================================
// Includes
//==============================================================================

#include <string.h>
#include <stdlib.h>

#include "swadgeMode.h"
#include "musical_buzzer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"

#include "linked_list.h"
#include "led_util.h"
#include "aabb_utils.h"
#include "settingsManager.h"

#include "mode_picross.h"
#include "picross_menu.h"
#include "bresenham.h"

//==============================================================================
// Constants
//==============================================================================


//===
// Structs
//===


//==============================================================================
// Function Prototypes
//==============================================================================
void picrossUserInput(void);
void picrossResetInput(void);
void picrossCheckLevel(void);
void picrossSetupPuzzle(uint8_t levelIndex);
box_t boxFromCoord(int8_t x, int8_t y);
picrossHint_t newHint(uint8_t index, bool isRow,uint8_t hints[5]);
void drawPicrossScene(display_t* d);
void drawPicrossHud(display_t* d, font_t* prompt, font_t* font);
void drawHint(display_t* d,font_t* font, picrossHint_t hint);


//==============================================================================
// Variables
//==============================================================================

static picrossGame_t* p = NULL;


//==============================================================================
// Functions
//==============================================================================
/**
 * Initialize all data needed for the jumper game
 *
 * @param disp The display to draw to
 * @param mmFont The font used for teh HUD, already loaded
 *
 */
void picrossStartGame(display_t* disp, font_t* mmFont)
{
    p = calloc(1, sizeof(picrossGame_t));
    p->currentPhase = PICROSS_SOLVING;
    p->d = disp;
    p->promptFont = mmFont;
    p->drawScale = 25;
    p->leftPad = 200;
    p->topPad = 130;
    loadFont("radiostars.font", &(p->game_font));

    p->puzzle = calloc(1, sizeof(picrossPuzzle_t));
    p->puzzle->width = 10;
    p->puzzle->height = 10;
    

    p->input = calloc(1, sizeof(picrossInput_t));
    p->input->x=0;
    p->input->y=0;
    p->input->btnState=0;

    p->controlsEnabled = true;


    //Setup level
    picrossSetupPuzzle(0);//<- get this from level select menu
}

void picrossSetupPuzzle(uint8_t levelIndex)
{
    p->levelIndex = levelIndex;

    uint8_t sampleRow[5] = {1,0,0,0,0};
    //rows
    p->puzzle->rowHints[0] = newHint(0,true,sampleRow);
    p->puzzle->rowHints[6] = newHint(1,true,sampleRow);
    p->puzzle->rowHints[1] = newHint(2,true,sampleRow);
    p->puzzle->rowHints[2] = newHint(3,true,sampleRow);
    p->puzzle->rowHints[3] = newHint(4,true,sampleRow);
    p->puzzle->rowHints[4] = newHint(5,true,sampleRow);
    p->puzzle->rowHints[5] = newHint(6,true,sampleRow);
    p->puzzle->rowHints[7] = newHint(7,true,sampleRow);
    p->puzzle->rowHints[8] = newHint(8,true,sampleRow);
    p->puzzle->rowHints[9] = newHint(9,true,sampleRow);
    //
    p->puzzle->colHints[0] = newHint(0,false,sampleRow);
    p->puzzle->colHints[1] = newHint(1,false,sampleRow);
    p->puzzle->colHints[2] = newHint(2,false,sampleRow);
    p->puzzle->colHints[3] = newHint(3,false,sampleRow);
    p->puzzle->colHints[4] = newHint(4,false,sampleRow);
    p->puzzle->colHints[5] = newHint(5,false,sampleRow);
    p->puzzle->colHints[6] = newHint(6,false,sampleRow);
    p->puzzle->colHints[7] = newHint(7,false,sampleRow);
    p->puzzle->colHints[8] = newHint(8,false,sampleRow);
    p->puzzle->colHints[9] = newHint(9,false,sampleRow);

    for(int i = 0;i<p->puzzle->width;i++)
    {
        for(int j = 0;j<p->puzzle->height;j++)
        {
            p->puzzle->level[i][j] = SPACE_EMPTY;
        }
    }
    //Get chosen level and load it's data...

    //Write the hint labels for each row and column

    picrossResetInput();
}

//lol i dont know how to do constructors in c++
picrossHint_t newHint(uint8_t index, bool isRow, uint8_t hints[5])
{
    picrossHint_t t;
    t.complete = false;
    t.isRow = isRow;
    t.index = index;
    //everything about this is wrong
    for(int i = 0;i<5;i++)
    {
        t.hints[i] = hints[i];
    }
   
    

    return t;
}

void picrossResetInput()
{
    p->input->x = 0;
    p->input->y = 0;
    // p->input->btnState = 0;//?
}

void picrossGameLoop(int64_t elapsedUs)
{
    switch(p->currentPhase)
    {
        case PICROSS_YOUAREWIN:
            //could go to next level
            //picrossSetupState(j->scene->level);
            break;
        case PICROSS_SOLVING:
            // uh
            break;
    }

    picrossUserInput();

    //check for victory! Do we need to do this every frame?
    //or all of it every frame?
    picrossCheckLevel();

    drawPicrossScene(p->d);
}

void picrossCheckLevel()
{
    //Foreach row and column in the hints
    //check if any row is incomplete
    
    //if not...
    return;

    p->currentPhase = PICROSS_YOUAREWIN;
    p->controlsEnabled = false;
}


/**
 * @brief Process controls
 *
 */
void picrossUserInput(void)
{
    picrossInput_t* input = p->input;
    uint8_t x = input->x;
    uint8_t y = input->y;
    picrossSpaceType_t current = p->puzzle->level[x][y];

    //reset
    input-> movedThisFrame = false;

    if (p->controlsEnabled == false || p->currentPhase != PICROSS_SOLVING)
    {
        return;
    }

    //Input checks
    if (input->btnState & BTN_A && !(input->prevBtnState & BTN_A))
    {
        //Button has no use here
        //toggle Selection

        //not doing clever cast to int and increment, because we have HINT in the enum.
        //not worrying about hints until... later.
        if(current != SPACE_FILLED)
        {
            current = SPACE_FILLED;
        }else
        {
            current = SPACE_EMPTY;
        }
        //set the toggle.
        p->puzzle->level[x][y] = current;
    }

    if (input->btnState & BTN_B && !(input->prevBtnState & BTN_B))
    {
        //Button has no use here
        //toggle Hints
        //not doing clever cast to int and increment, because we have HINT in the enum.
        //not worrying about hints until... later.
        if(current != SPACE_MARKEMPTY)
        {
            current = SPACE_MARKEMPTY;
        }else
        {
            current = SPACE_EMPTY;
        }
        //set the toggle.
        p->puzzle->level[x][y] = current;
    }

    if (input->btnState & UP && !(input->prevBtnState & UP))
    {
        input->movedThisFrame = true;
        if(y == 0)
        {
            //wrap to opposite position
            y = p->puzzle->height-1; 
        }else
        {
            input->y = y - 1;
        }
    }
    else if (input->btnState & DOWN && !(input->prevBtnState & DOWN))
    {
        input->movedThisFrame = true;
        if(input->y == p->puzzle->height-1)
        {
            //wrap to bottom position
            input->y = 0; 
        }else
        {
            input->y = input->y + 1;
        } 
        
    }
    else if (input->btnState & LEFT && !(input->prevBtnState & LEFT))
    {
        input->movedThisFrame = true;
        if(input->x == 0)
        {
            //wrap to right position
            input->x = p->puzzle->width-1; 
        }else
        {
            input->x = input->x-1;
        }
    }
    else if (input->btnState & RIGHT && !(input->prevBtnState & RIGHT))
    {
        input->movedThisFrame = true;
        if(input->x == p->puzzle->width-1)
        {
            //wrap to left position
            input->x = 0; 
        }else
        {
            input->x = input->x+1;
        }
    }

    //check for holding input while using d-pad
    if(input->movedThisFrame)
    {
        if(input->btnState & BTN_A)
        {
            //held a while moving 
            p->puzzle->level[input->x][input->y] = SPACE_FILLED;
        }else if(input->btnState & BTN_B)
        {
            //held b while moving.
            p->puzzle->level[input->x][input->y] = SPACE_MARKEMPTY;
        }
    }

    input->prevBtnState = input->btnState; 
}

void drawPicrossScene(display_t* d)
{
    uint8_t w = p->puzzle->width;
    uint8_t h = p->puzzle->height;
    
    d->clearPx();

    for(int i = 0;i<w;i++)
    {
        for(int j = 0;j<h;j++)
        {
            //shapw of boxDraw
            //we will probably replace with "drawwsg"
            box_t box1 = boxFromCoord(i,j);          
            switch(p->puzzle->level[i][j])
            {
                case SPACE_EMPTY:
                {
                    drawBox(d, box1, c555, true, 1);
                    break;
                }
                case SPACE_FILLED:
                {
                    drawBox(d, box1, c000, true, 1);
                    break;
                }
                case SPACE_MARKEMPTY:
                {
                    drawBox(d, box1, c531, true, 1);
                    break;
                }
                case SPACE_HINT:
                {
                    drawBox(d, box1, c050, true, 1);
                    break;
                }
            }
        }
    }

    //Draw player input box
    box_t inputBox = boxFromCoord(p->input->x,p->input->y); 
    drawBox(d, inputBox, c050, false, 1);
    drawPicrossHud(d, p->promptFont, &p->game_font);
}

box_t boxFromCoord(int8_t x,int8_t y)
{
    uint8_t s = p->drawScale;
    box_t box =
        {
            .x0 = (x * s) + s + p->leftPad,
            .y0 = (y * s) + s + p->topPad,
            .x1 = (x * s) + s + s + p->leftPad,
            .y1 = (y * s) + s + s + p->topPad,
        };  
        return box;
}

void drawPicrossHud(display_t* d, font_t* prompt, font_t* font)
{
    char textBuffer[12];
    snprintf(textBuffer, sizeof(textBuffer) - 1, "%d,%d", p->input->x+1,p->input->y+1);

    drawText(d, font, c555, textBuffer, 230, 220);

    uint8_t w = 5;
    //draw a vertical line every 5 units.
    for(int i=0;i<p->puzzle->width;i=i+5)
    {
        if(i==0)
        {
            continue;
        }
        uint8_t s = p->drawScale;
      box_t box =
        {
            .x0 = (i * s) + s + p->leftPad - w/2,
            .y0 = s + p->topPad,
            .x1 = (i * s) + s + w/2 + p->leftPad,//3 is width
            .y1 = (p->puzzle->height * s) + s + p->topPad,
        };  
         drawBox(d,box,c441,true,1);
    }

     //draw a horizontal line every 5 units.
    for(int i=0;i<p->puzzle->height;i=i+5)
    {
        if(i==0)
        {
            continue;
        }
        uint8_t s = p->drawScale;
      box_t box =
        {
            .y0 = (i * s) + s + p->topPad-w/2,
            .x0 = s + p->leftPad,
            .y1 = (i * s) + s + w/2 + p->topPad,//5 is width
            .x1 = (p->puzzle->height * s) + s + p->leftPad,
        };  
         drawBox(d,box,c441,true,1);
    }


    if (p->currentPhase == PICROSS_SOLVING)
    {
        // drawText(d, prompt, c000, "Ready?", 102, 128);
        // drawText(d, prompt, c555, "Ready?", 100, 128);
    }else if (p->currentPhase == PICROSS_YOUAREWIN)
    {
       drawText(d, prompt, c000, "You are win!", 122, 129);
       drawText(d, prompt, c555, "You are win!", 120, 128); 
    }

    //Draw hints
    for(int i=0;i<10;i++)
    {
        drawHint(d,font,p->puzzle->rowHints[i]);
        drawHint(d,font,p->puzzle->colHints[i]);
    }
}

void drawHint(display_t* d,font_t* font, picrossHint_t hint)
{
    if(hint.isRow){
        for(int i = 0;i<5;i++)
        {
            box_t t = boxFromCoord(-i-1,hint.index);
            char letter[1];
            // calling sprintf is setting x0 on t to 0. I think its a poking at memory out of bounds
            // sprintf(letter, "%d", hint.hints[i]);
            drawBox(d,t,c333,false,1);
            // drawChar(d,c444, font->h, &font->chars[(*letter) - ' '], t.x0, t.y0);
        }
    }else{
        for(int i = 0;i<5;i++)
        {
            int k = -i-1;
            box_t t = boxFromCoord(hint.index,k);
            char letter[1];
            // sprintf(letter, "%d", hint.hints[i]);
            drawBox(d,t,c333,false,1);

            // drawChar(d,c444, font->h, &font->chars[(*letter) - ' '], t.x0, t.y0);
        }
    }
}


void picrossGameButtonCb(buttonEvt_t* evt)
{
    p->input->btnState = evt->state;
}

void picrossExitGame(void)
{
    if (NULL != p)
    {
        freeFont(&(p->game_font));
        // free(p->puzzle);
        free(p->input);
        free(p);
        p = NULL;
    }
}