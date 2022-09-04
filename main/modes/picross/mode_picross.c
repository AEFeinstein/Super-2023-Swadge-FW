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
#include "nvs_manager.h"

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
void setCompleteLevelFromWSG(wsg_t* puzz);
void drawSinglePixelFromWSG(display_t* d,int x, int y, wsg_t* image);
bool hintsMatch(picrossHint_t a, picrossHint_t b);
box_t boxFromCoord(int8_t x, int8_t y);
picrossHint_t newHintFromPuzzle(uint8_t index, bool isRow, picrossSpaceType_t source[10][10]);
void drawPicrossScene(display_t* d);
void drawPicrossHud(display_t* d, font_t* prompt, font_t* font);
void drawHint(display_t* d,font_t* font, picrossHint_t hint);
void drawPicrossInput(display_t* d);

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
//todo: this should receive a levelIndex. allocate memory as appropriate for level size, and pass into setuplevel
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
    picrossSetupPuzzle(0);
}

void picrossSetupPuzzle(uint8_t levelIndex)
{
    p->levelIndex = levelIndex;

    wsg_t levelwsg;
    loadWsg("test1.wsg", &levelwsg);
    loadWsg("test1_complete.wsg", &p->puzzle->completeImage);
    setCompleteLevelFromWSG(&levelwsg);

    //one step closer to this working correctly!
    p->puzzle->width = levelwsg.w;
    p->puzzle->height = levelwsg.h;
    //maxhints = max(width/2 -1,height/2 -1) i ... think?

    //rows
    for(int i = 0;i<p->puzzle->height;i++)
    {
        p->puzzle->rowHints[i] = newHintFromPuzzle(i,true,p->puzzle->completeLevel);  
    }
    
    //cols
    for(int i = 0;i<p->puzzle->height;i++)
    {
        p->puzzle->colHints[i] = newHintFromPuzzle(i,false,p->puzzle->completeLevel);  
    }

    //Set the actual level to be empty.
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

    //free now or at the end? We only need the victory one.
    freeWsg(&levelwsg);
}

void setCompleteLevelFromWSG(wsg_t* puzz)
{
    //go row by row
    paletteColor_t col = c555;
    for(int r = 0;r<puzz->h;r++)//todo: puzzles that dont have the right size?
    {
        for(int c=0;c<puzz->w;c++){
            col = puzz->px[(r * puzz->w) + c];
            if(col == cTransparent || col == c555){
                p->puzzle->completeLevel[c][r] = SPACE_EMPTY;
            }else{
                p->puzzle->completeLevel[c][r] = SPACE_FILLED;
            }
        }
    }
    
}

//lol i dont know how to do constructors in c++
picrossHint_t newHintFromPuzzle(uint8_t index, bool isRow, picrossSpaceType_t source[10][10])
{
    picrossHint_t t;
    t.complete = false;
    t.isRow = isRow;
    t.index = index;
    
    //set all hints to 0. 5 here is the max number of hints we can display, currently hard-coded.
    for(int i = 0;i<5;i++)
    {
         t.hints[i] = 0;
    }

    //todo: non-square puzzles. get width/height passed in from newHint.
    picrossSpaceType_t prev = SPACE_EMPTY;//starts empty because out of bounds is empty for how hints work.
    uint8_t blockNumber = 0;
    if(isRow){
        for(int i = 0;i<p->puzzle->width;i++)
        {
            if(source[i][index] == SPACE_FILLED)//we have a clue.
            {
                //Counting up the hint size.
                if(prev == SPACE_FILLED)
                {
                    t.hints[blockNumber]++;
                }else if(prev == SPACE_EMPTY)//this is a new block
                {
                    blockNumber++;//next block! (but we go left to right, which is the clue right to left)
                    t.hints[blockNumber]++;
                }
                prev = SPACE_FILLED;
            }else // if empty
            {
                prev = SPACE_EMPTY;
                //i continues to increase.
            }
            //set prev for next
        }
    }else{
        //same logic but for columns
        for(int i = 0;i<p->puzzle->height;i++)
        {
            if(source[index][i]==SPACE_FILLED)
            {
                //Counting up the block size.
                if(prev == SPACE_FILLED)
                {
                    t.hints[blockNumber]++;
                }else if(prev == SPACE_EMPTY)//this is a new block
                {
                    blockNumber++;
                    t.hints[blockNumber]++;
                }
                prev = SPACE_FILLED;
            }else // if empty
            {
                prev = SPACE_EMPTY;
                //i continues to increase.
            }
        }
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

    //userInput sets this value when we change a block to or from filled.
    if(p->input->changedLevelThisFrame)
    {
        picrossCheckLevel();
    }

    drawPicrossScene(p->d);

    
    //todo: this has not been tested yet.
    if(p->previousPhase == PICROSS_SOLVING && p->currentPhase == PICROSS_YOUAREWIN)
    {
        //Save the fact that we won.
        int32_t victories = 0;
        readNvs32("picrossSolves", &victories);
        
        //shift 1 (0x0001) over levelIndex times, then OR it with victories.
        victories = victories | (1 << p->levelIndex);
        //Save.
        writeNvs32("picrossSolves", victories);
    }
    p->previousPhase = p->currentPhase;
}

void picrossCheckLevel()
{
    for(int c =0;c<p->puzzle->width;c++)
    {
        for(int r = 0;r<p->puzzle->height;r++)
        {
            if(p->puzzle->level[r][c] == SPACE_EMPTY || p->puzzle->level[r][c] == SPACE_MARKEMPTY)
            {
                if(p->puzzle->completeLevel[r][c] != SPACE_EMPTY){
                    return;
                }
            }else//if space == SPACE_FILLED (we aren't using filled-hints so lets not bother specific if for now)
            {
                if(p->puzzle->completeLevel[r][c] != SPACE_FILLED){
                    return;
                }
            }
        }
    }

    //Check level by clues.
    //We use this if we can't ensure there are unique solutions to the clues.
    //because if you got ANY solution, you should win.

    //but its solwer than just comparing numbers in the grid.
    // picrossHint_t testingHint;
    // for(int i = 0;i<p->puzzle->width;i++)
    // {
    //     //rows
    //     testingHint = newHintFromPuzzle(i,true,p->puzzle->level);
    //     if(!hintsMatch(testingHint,p->puzzle->rowHints[i])){
    //         return;
    //     }
    // }
    // for(int i = 0;i<p->puzzle->height;i++)
    // {
    //     //cols
    //     //rows
    //     testingHint = newHintFromPuzzle(i,false,p->puzzle->level);
    //     if(!hintsMatch(testingHint,p->puzzle->colHints[i])){
    //         return;
    //     }
    // }
    
    p->currentPhase = PICROSS_YOUAREWIN;
    p->controlsEnabled = false;
    
}

bool hintsMatch(picrossHint_t a, picrossHint_t b)
{
    for(int i = 0;i<5;i++)
    {
        if(a.hints[i] != b.hints[i])
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Process controls
 *
 */
void picrossUserInput(void)
{
    picrossInput_t* input = p->input;
    uint8_t x = input->x;//todo: make these pointers
    uint8_t y = input->y;//todo
    picrossSpaceType_t current = p->puzzle->level[x][y];

    //reset
    input-> movedThisFrame = false;
    input-> changedLevelThisFrame = false;//do we need to check for solution?

    if (p->controlsEnabled == false || p->currentPhase != PICROSS_SOLVING)
    {
        //TODO: return to level select screen on input
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
            input->changedLevelThisFrame = true;
        }else
        {
            current = SPACE_EMPTY;
            input->changedLevelThisFrame = true;
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
        if(input->y == 0)
        {
            //wrap to opposite position
            input->y = p->puzzle->height-1; 
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
        //todo: toggle.
        if(input->btnState & BTN_A)
        {
            //held a while moving 
            p->puzzle->level[input->x][input->y] = SPACE_FILLED;
            input->changedLevelThisFrame = true;//not strictly true, but this behaviour will become toggle in future
        }else if(input->btnState & BTN_B)
        {
            //held b while moving.
            p->puzzle->level[input->x][input->y] = SPACE_MARKEMPTY;
            input->changedLevelThisFrame = true;//not strictly true, but this behaviour will become toggle in future
        }
    }

    

    input->prevBtnState = input->btnState; 
}

void drawPicrossScene(display_t* d)
{
    uint8_t w = p->puzzle->width;
    uint8_t h = p->puzzle->height;
    
    d->clearPx();
    box_t box;
    if(p->currentPhase == PICROSS_SOLVING)
    {
        for(int i = 0;i<w;i++)
        {
            for(int j = 0;j<h;j++)
            {
                //shapw of boxDraw
                //we will probably replace with "drawwsg"
                box = boxFromCoord(i,j);          
                switch(p->puzzle->level[i][j])
                {
                    case SPACE_EMPTY:
                    {
                        drawBox(d, box, c555, true, 1);
                        break;
                    }
                    case SPACE_FILLED:
                    {
                        drawBox(d, box, c000, true, 1);
                        break;
                    }
                    case SPACE_MARKEMPTY:
                    {
                        drawBox(d, box, c531, true, 1);
                        break;
                    }
                    case SPACE_HINT:
                    {
                        drawBox(d, box, c050, true, 1);
                        break;
                    }
                }
            }
        }
        drawPicrossHud(d, p->promptFont, &p->game_font);
        drawPicrossInput(d);
    }//end if phase is solving   
    else if (p->currentPhase == PICROSS_YOUAREWIN)
    {
        for(int i = 0;i<w;i++)
        {
            for(int j = 0;j<h;j++)
            {
                drawSinglePixelFromWSG(d,i,j,&p->puzzle->completeImage);
            }
        }
    }
}

void drawSinglePixelFromWSG(display_t* d,int x, int y, wsg_t* image)
{
    box_t box = boxFromCoord(x,y);
    paletteColor_t v = image->px[(y * p->puzzle->width) + x];
    drawBox(d, box, v, true, 1);
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

    //width of thicker center lines
    uint8_t w = 5;
    //draw a vertical line every 5 units.
    for(int i=1;i<p->puzzle->width;i++)//skip 0 and skip last. literally the fence post problem.
    {
        uint8_t s = p->drawScale;
        if(i%5 == 0)//draw thicker line every 5.
        {
            box_t box =
            {
                .x0 = (i * s) + s + p->leftPad - w/2,
                .y0 = s + p->topPad,
                .x1 = (i * s) + s + w/2 + p->leftPad,//3 is width
                .y1 = (p->puzzle->height * s) + s + p->topPad,
            };  
            drawBox(d,box,c441,true,1);
            continue;
        }
        //this could be less confusing.
        plotLine(d,
            ((i * s) + s + p->leftPad) >> 1,
            (s + p->topPad) >> 1,
            ((i * s) + s + p->leftPad) >> 1,
            ((p->puzzle->height * s) + s + p->topPad) >> 1,
            c111,0);
        
    }

     //draw a horizontal line every 5 units.
     //todo: also do the full grid.
    for(int i=1;i<p->puzzle->height;i++)
    {
        uint8_t s = p->drawScale;
        if(i%5 == 0)
        {
            box_t box =
            {
                .x0 = s + p->leftPad,
                .y0 = (i * s) + s + p->topPad - w/2,
                .x1 = (p->puzzle->width * s) + s + p->leftPad,
                .y1 = (i * s) + s + p->topPad + w/2,
            };  
            drawBox(d,box,c441,true,1);
            continue;
        }
        //this should be less confusing.
        plotLine(d,
            (s + p->leftPad) >> 1,
            ((i * s) + s + p->topPad) >> 1,
            ((p->puzzle->width * s) + s + p->leftPad) >> 1,
            ((i * s) + s + p->topPad) >> 1,
            c111,0);
     
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
    //todo: handle 10 or double-digit input 
    if(hint.isRow){
        int j = 0;
        for(int i = 0;i<5;i++)
        {
            box_t hintbox = boxFromCoord(-j-1,hint.index);
            //we have to flip the hints around. 
            if(hint.hints[4-i] == 0){
                //dont increase j.
                
               // drawBox(d,hintbox,c333,false,1);//for debugging. we will draw nothing when proper.
            }else{
                drawBox(d,hintbox,c111,false,1);//for debugging. we will draw nothing when proper.

                char letter[1];
                sprintf(letter, "%d", hint.hints[4-i]);//this function appears to modify hintbox.x0
                //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
                drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], (hintbox.x1-p->drawScale) >> 1, (hintbox.y1-p->drawScale) >> 1);
                j++;//the index position, but only for where to draw. shifts the clues to the right.
            }

            }
    }else{
        int j = 0;
        for(int i = 0;i<5;i++)
        {
            box_t hintbox = boxFromCoord(hint.index,-j-1);
             if(hint.hints[4-i] == 0){               
               // drawBox(d,hintbox,c333,false,1);//for debugging. we will draw nothing when proper.
            }else{
                drawBox(d,hintbox,c111,false,1);//for debugging. we will draw nothing when proper.
                char letter[1];
                sprintf(letter, "%d", hint.hints[4-i]);//this function appears to modify hintbox.x0
                //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
                drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], (hintbox.x1-p->drawScale) >> 1, (hintbox.y1-p->drawScale) >> 1);
                j++;//the index position, but only for where to draw. shifts the clues to the right.
            }

        }
    }
}

void drawPicrossInput(display_t* d)
{
    //Draw player input box
    box_t inputBox = boxFromCoord(p->input->x,p->input->y);
    inputBox.x1 = inputBox.x1+1;
    inputBox.y1 = inputBox.y1+1;//cover up both marking lines
    drawBox(d, inputBox, c050, false, 1);
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
        free(&p->puzzle->completeImage);
        free(p->puzzle);
        free(p->input);
        free(p);
        p = NULL;
    }
}