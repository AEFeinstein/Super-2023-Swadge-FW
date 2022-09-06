//==============================================================================
// Includes
//==============================================================================

#include <string.h>
#include <stdlib.h>

#include "swadgeMode.h"
#include "musical_buzzer.h"
#include "esp_log.h"
#include "led_util.h"

#include "aabb_utils.h"
// #include "settingsManager.h"
#include "nvs_manager.h"

#include "mode_picross.h"
#include "picross_menu.h"
#include "picross_select.h"
#include "bresenham.h"

//==============================================================================
// Function Prototypes
//==============================================================================
void picrossUserInput(int64_t elapsedUs);
void picrossResetInput(void);
void picrossCheckLevel(void);
void picrossSetupPuzzle(bool cont);
void setCompleteLevelFromWSG(wsg_t* puzz);
void drawSinglePixelFromWSG(display_t* d,int x, int y, wsg_t* image);
bool hintsMatch(picrossHint_t a, picrossHint_t b);
box_t boxFromCoord(int8_t x, int8_t y);
picrossHint_t newHintFromPuzzle(uint8_t index, bool isRow, picrossSpaceType_t source[10][10]);
void drawPicrossScene(display_t* d);
void drawPicrossHud(display_t* d, font_t* font);
void drawHint(display_t* d,font_t* font, picrossHint_t hint);
void drawPicrossInput(display_t* d);
void drawBackground(display_t* d);
void countInput(picrossDir_t input);
int8_t getHintShift(uint8_t hint);
void saveCompletedOnSelectedLevel(bool completed);
void enterSpace(uint8_t x,uint8_t y,picrossSpaceType_t newSpace);
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
void picrossStartGame(display_t* disp, font_t* mmFont, picrossLevelDef_t* selectedLevel, bool cont)
{
    p = calloc(1, sizeof(picrossGame_t));
    p->selectedLevel = selectedLevel;
    p->currentPhase = PICROSS_SOLVING;
    p->d = disp;
    // p->promptFont = mmFont;
    p->drawScale = 25;
    p->leftPad = 200;
    p->topPad = 130;
    p->clueGap = 2;//currently only used for horizontal clues. 
    p->exitThisFrame = false;

    loadFont("ibm_vga8.font", &(p->hint_font));

    p->puzzle = calloc(1, sizeof(picrossPuzzle_t));
    p->puzzle->width = 10;
    p->puzzle->height = 10;

    //todo: im doing a lot of this again in ResetInput. Its a holdover from my boilerplate. todo: I want to most of this into there.
    p->input = calloc(1, sizeof(picrossInput_t));
    p->input->x=0;
    p->input->y=0;
    p->input->btnState=0;
    p->input->prevBtnState= 0x80 | 0x10 | 0x40 | 0x20;//prevents us from instantly fillling in a square because A is held from selecting level.
    p->countState = PICROSSDIR_IDLE;
    p->controlsEnabled = true;
    p->input->DASActive = false;
    p->input->firstDASTime = 500000;//.5 seconds
    p->input->DASTime = 220000;//1,000,000 = 1 second.
    p->input->startHeldType = OUTOFBOUNDS;
    p->input->inputBoxColor = c430;
    p->input->inputBoxDefaultColor = c430;
    p->input->inputBoxErrorColor = c500;

    p->input->blinkError = false;
    p->input->blinkAnimTimer =0;
    p->input->blinkTime = 120000;//half a blink cycle (on)(off) or full (on/off)(on/off)?
    p->input->blinkCount = 6;
    p->input->showHints = picrossGetSaveFlag(0);//0 is showHints.


    //cant tell if this is doing things the lazy way or not.
    for(int i = 0;i<NUM_LEDS;i++)
    {
        p->offLEDS[i].r = 0x00;
        p->offLEDS[i].g = 0x00;
        p->offLEDS[i].b = 0x00;

        if(i%2 == 0)
        {
            p->errorALEDBlinkLEDS[i].r = 0xFF;
            p->errorALEDBlinkLEDS[i].g = 0x00;
            p->errorALEDBlinkLEDS[i].b = 0x00;

            p->errorBLEDBlinkLEDS[i].r = 0x00;
            p->errorBLEDBlinkLEDS[i].g = 0x00;
            p->errorBLEDBlinkLEDS[i].b = 0x00;
        }else
        {
            p->errorALEDBlinkLEDS[i].r = 0x00;
            p->errorALEDBlinkLEDS[i].g = 0x00;
            p->errorALEDBlinkLEDS[i].b = 0x00;

            p->errorBLEDBlinkLEDS[i].r = 0xFF;
            p->errorBLEDBlinkLEDS[i].g = 0x00;
            p->errorBLEDBlinkLEDS[i].b = 0x00;
        }
    }

    //save data configure
    p->save = calloc(1, sizeof(picrossSaveData_t));
    for(int i=0;i<8;i++)
    {
        p->save->banks[i] = 0;
        // //pic_b_i
        // p->save->bankNames[i][0] = "p";
        // p->save->bankNames[i][1] = "i";
        // p->save->bankNames[i][2] = "c";
        // p->save->bankNames[i][3] = "_";
        // p->save->bankNames[i][4] = "b";
        // p->save->bankNames[i][5] = "_";
        //im way to tired to look up how to case from int to char.
        //just realized these can basically be random as long as they dont interfere with anyone elses code.
    }

    //Setup level
    picrossSetupPuzzle(cont);
}

void picrossSetupPuzzle(bool cont)
{   
    wsg_t *levelwsg = &p->selectedLevel->levelWSG;

    setCompleteLevelFromWSG(levelwsg);

    //one step closer to this working correctly!
    p->puzzle->width = levelwsg->w;
    p->puzzle->height = levelwsg->h;
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

    //Set the current level appropriately.
    if(cont)
    {
        //Set the actual level from save data
        loadPicrossProgress();
    }else
    {
        //Set the actual level to be empty
        for(int i = 0;i<p->puzzle->width;i++)
        {
            for(int j = 0;j<p->puzzle->height;j++)
            {
                p->puzzle->level[i][j] = SPACE_EMPTY;
            }
        }
        savePicrossProgress();//clear out your previous (victory) data.
        saveCompletedOnSelectedLevel(false);//clear out the fact that you have, perhaps, beaten this level before.
    }

    //Get chosen level and load it's data...

    //Write the hint labels for each row and column

    picrossResetInput();
}

//Scans the levelWSG and creates the finished version of the puzzle in the proper data format (2D array of enum).
void setCompleteLevelFromWSG(wsg_t* puzz)
{
    // wsg_t puzz = puzzle;
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

//lol i dont know how to do constructors, im a unity developer! anyway this constructs a source.
//we use the same data type for rows and columns, hence the bool flag. index is the position in the row/col, and we pass in the level.
//I guess that could be a pointer? Somebody more experienced with c, code review this and optimize please.
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
    picrossSpaceType_t prev = OUTOFBOUNDS;//starts empty because out of bounds is empty for how hints work.
    int8_t blockNumber = 0;// 
    if(isRow){
        for(int i = 0;i<p->puzzle->width;i++)
        {
            if(source[i][index] == SPACE_FILLED)//we have a clue.
            {
                //Counting up the hint size.
                if(prev == SPACE_FILLED)
                {
                    t.hints[blockNumber]++;
                }else if(prev == SPACE_EMPTY)//this is a new block, and not the first block.
                {
                    blockNumber++;//next block! (but we go left to right, which is the clue right to left)
                    t.hints[blockNumber]++;
                }else if(prev == OUTOFBOUNDS)//first block in sequence.
                {
                    //same as above but dont increase blocknumber.
                    t.hints[blockNumber]++;
                }
                prev = SPACE_FILLED;
            }else // if the space is empty
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
                }else if(prev == SPACE_EMPTY)//this is a new block, and not the first block.
                {
                    blockNumber++;
                    t.hints[blockNumber]++;
                }else if(prev == OUTOFBOUNDS)
                {
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
    p->input->movedThisFrame = false;
    p->input->changedLevelThisFrame = false;
    p->input->prevBtnState = 0;
    p->countState = PICROSSDIR_IDLE;
    p->count = 1;
    p->input->DASActive = false;
    p->input->holdingDir = PICROSSDIR_IDLE; 
    // p->input->btnState = 0;//?
}

void picrossGameLoop(int64_t elapsedUs)
{
    picrossUserInput(elapsedUs);
    if(p->exitThisFrame)
    {
        picrossExitGame();//free variables
        returnToPicrossMenu();//change to menu
        //dont do more processing, we have switched back to level select screen.
        return;
    }

    //userInput sets this value when we change a block to or from filled.
    if(p->input->changedLevelThisFrame)
    {
        picrossCheckLevel();
    }

    drawPicrossScene(p->d);

    
    //You won! Only called once, since you cant go from win->solving without resetting everything (ie: menu and back)
    if(p->previousPhase == PICROSS_SOLVING && p->currentPhase == PICROSS_YOUAREWIN)
    {
        //Set LEDs to ON BECAUSE YOU ARE WIN
        //The game doesn't use a timer, so making them blink ABAB->BABA like I want will have to wait.
        led_t leds[NUM_LEDS] =
        {
            {.r = 0xFF, .g = 0xFF, .b = 0xFF},
            {.r = 0xFF, .g = 0xFF, .b = 0xFF},
            {.r = 0xFF, .g = 0xFF, .b = 0xFF},
            {.r = 0xFF, .g = 0xFF, .b = 0xFF},
            {.r = 0xFF, .g = 0xFF, .b = 0xFF},
            {.r = 0xFF, .g = 0xFF, .b = 0xFF},
            {.r = 0xFF, .g = 0xFF, .b = 0xFF},
            {.r = 0xFF, .g = 0xFF, .b = 0xFF}
        };

        setLeds(leds, NUM_LEDS);

        //save the fact that we won
        saveCompletedOnSelectedLevel(true);
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
                if(p->puzzle->completeLevel[r][c] != SPACE_EMPTY)
                {
                    return;
                }
            }else//if space == SPACE_FILLED (we aren't using filled-hints so lets not bother specific if for now)
            {
                if(p->puzzle->completeLevel[r][c] != SPACE_FILLED)
                {
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

//==============================================
//CONTROLS & INPUT
//==============================================

void picrossUserInput(int64_t elapsedUs)
{
    picrossInput_t* input = p->input;
    uint8_t x = input->x;
    uint8_t y = input->y;
    picrossSpaceType_t current = p->puzzle->level[x][y];

    //reset
    input-> movedThisFrame = false;
    input-> changedLevelThisFrame = false;//do we need to check for solution?

    //check for "exit" button before we escape when not in solving mode.
    //todo: how should exit work? A button when game is solved?
    if(input->btnState & SELECT && !(input->prevBtnState & SELECT) && !(input->btnState & BTN_A))//if we are holding a down when we leave, we instantly select a level on the select screen.
    {
        savePicrossProgress();
        // returnToLevelSelect();//im on the fence about how this should behave. to level or main.
        // returnToPicrossMenu();//now that it hovers over continue, i think main is better.
        p->exitThisFrame = true;//stops drawing to the screen, stops messing with variables, frees memory.
        return;
    }

    if (p->controlsEnabled == false || p->currentPhase != PICROSS_SOLVING)
    {
        return;
    }

    //Input checks
    
    //Reset the counter by pressing start. It should auto-reset, but it can be wonky.
    if (input->btnState & START && !(input->prevBtnState & START))
    {
        p->count = 1;
    }
    
    //DAS
    //todo: we can cache the != checks, so we do 1 butmask instead of 3 (x4)
    
    if(input->btnState & UP && !((input->btnState & RIGHT) || (input->btnState & LEFT) || (input->btnState & DOWN)))
    {
        if(p->input->holdingDir == PICROSSDIR_UP)
        {
            p->input->timeHeldDirection = p->input->timeHeldDirection + elapsedUs;
        }else{
            p->input->timeHeldDirection = 0;
            p->input->holdingDir = PICROSSDIR_UP;
            p->input->DASActive = false;
        }
    }else if(input->btnState & RIGHT && !((input->btnState & DOWN) || (input->btnState & UP) || (input->btnState & LEFT)))
    {
        if(p->input->holdingDir == PICROSSDIR_RIGHT)
        {
            p->input->timeHeldDirection = p->input->timeHeldDirection + elapsedUs;
        }else{
            p->input->timeHeldDirection = 0;
            p->input->holdingDir = PICROSSDIR_RIGHT;
            p->input->DASActive = false;
        }
    }else if(input->btnState & LEFT && !((input->btnState & DOWN) || (input->btnState & UP) || (input->btnState & RIGHT)))
    {
        if(p->input->holdingDir == PICROSSDIR_LEFT)
        {
            p->input->timeHeldDirection = p->input->timeHeldDirection + elapsedUs;
        }else{
            p->input->timeHeldDirection = 0;
            p->input->holdingDir = PICROSSDIR_LEFT;
            p->input->DASActive = false;//active not true till second.
        }
    }else if(input->btnState & DOWN && !((input->btnState & LEFT) || (input->btnState & RIGHT) || (input->btnState & UP)))
    {
        if(p->input->holdingDir == PICROSSDIR_DOWN)
        {
            p->input->timeHeldDirection = p->input->timeHeldDirection + elapsedUs;
        }else{
            p->input->timeHeldDirection = 0;
            p->input->holdingDir = PICROSSDIR_DOWN;
            p->input->DASActive = false;
        }
    }else{
        //0 or more than one button
        p->input->timeHeldDirection = 0;
        p->input->holdingDir = PICROSSDIR_IDLE;
        p->input->DASActive = false;
    }//now that checks have been done.

    //if we have been doing DAS and are faster than its the autospeed, OR (das is inactive and) we have held longer than the longer time.
    if((p->input->DASActive && p->input->timeHeldDirection > p->input->DASTime) || (!p->input->DASActive && p->input->timeHeldDirection > p->input->firstDASTime))
    {
        //all buts except l/r/u/d so we dont mess with start/select in the das
        p->input->prevBtnState = p->input->prevBtnState & ~(LEFT | RIGHT | UP | DOWN);
        p->input->timeHeldDirection = 0;
        p->input->DASActive = true;
    }

    if (input->btnState & BTN_A && !(input->prevBtnState & BTN_A))
    {
        //Button has no use here
        //toggle Selection

        //not doing clever cast to int and increment, because we have HINT in the enum.
        //not worrying about hints until... later.
        if(current != SPACE_FILLED)
        {
            current = SPACE_FILLED;
            input->startHeldType = SPACE_FILLED;
            input->changedLevelThisFrame = true;
        }else
        {
            current = SPACE_EMPTY;
            input->startHeldType = SPACE_EMPTY;
            input->changedLevelThisFrame = true;
        }
        //set the toggle.
        // p->puzzle->level[x][y] = current;
        enterSpace(x,y,current);
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
            input->startHeldType = SPACE_MARKEMPTY;
            input->changedLevelThisFrame = true;//shouldnt be needed
        }else
        {
            current = SPACE_EMPTY;
            input->startHeldType = SPACE_EMPTY;
            input->changedLevelThisFrame = true;//shouldnt be needed
        }
        //set the toggle.
        // p->puzzle->level[x][y] = current;
        enterSpace(x,y,current);
    }

    if (input->btnState & UP && !(input->prevBtnState & UP))
    {
        if(input->y == 0)
        {
            if(!input->DASActive){
                //wrap to opposite position
                input->y = p->puzzle->height-1; 
                p->count = 1;//reset counter. Its not helpful to just keep increasing or decreasing, although it is funny.
                input->movedThisFrame = true;
                countInput(PICROSSDIR_UP);
            }
        }else
        {
            input->y = y - 1;
            input->movedThisFrame = true;
            countInput(PICROSSDIR_UP);
        }
    }
    else if (input->btnState & DOWN && !(input->prevBtnState & DOWN))
    {
        if(input->y == p->puzzle->height-1 )
        {
            if(!input->DASActive){
                //wrap to bottom position
                input->y = 0; 
                p->count = 1;
                input->movedThisFrame = true;
                countInput(PICROSSDIR_DOWN);
            }
        }else
        {
            input->y = input->y + 1;
            input->movedThisFrame = true;
            countInput(PICROSSDIR_DOWN);
        } 
    }
    else if (input->btnState & LEFT && !(input->prevBtnState & LEFT))
    {
        if(input->x == 0 )
        {
            if(!input->DASActive)
            {
                //wrap to right position
                input->x = p->puzzle->width-1;
                p->count = 1;
                input->movedThisFrame = true;
                countInput(PICROSSDIR_LEFT);
            }
        }else
        {
            input->x = input->x-1;
            input->movedThisFrame = true;
            countInput(PICROSSDIR_LEFT);
        }
    }
    else if (input->btnState & RIGHT && !(input->prevBtnState & RIGHT))
    {
        if(input->x == p->puzzle->width-1)
        {
            if(!input->DASActive){
                //wrap to left position
                input->x = 0;
                p->count = 1;
                input->movedThisFrame = true;
                countInput(PICROSSDIR_RIGHT);
            }
        }else
        {
            input->x = input->x+1;
            input->movedThisFrame = true;
            countInput(PICROSSDIR_RIGHT);
        }
    }


    //check for holding input while using d-pad
    if(input->movedThisFrame)
    {
        //todo: toggle.
        if(input->btnState & BTN_A)
        {
            if(input->startHeldType == SPACE_FILLED)
            {
                enterSpace(input->x,input->y,SPACE_FILLED);
            }else if(input->startHeldType == SPACE_EMPTY)
            {
                enterSpace(input->x,input->y,SPACE_EMPTY);
            }
        }else if(input->btnState & BTN_B)
        {
            if(input->startHeldType == SPACE_MARKEMPTY)
            {
                enterSpace(input->x,input->y,SPACE_MARKEMPTY);
            }else if(input->startHeldType == SPACE_EMPTY)
            {
                enterSpace(input->x,input->y,SPACE_EMPTY);
            }
        }
    }    
    
    //lastly, set prevBtnState to btnState
    input->prevBtnState = input->btnState;
    
    //doing prevBtn set below the blink code below breaks it?

    //blink timer.
    //if SHOW HINTS.
    if(input->blinkError)
    {
        uint64_t totalTime = input->blinkTime*(input->blinkCount);
        input->blinkAnimTimer = input->blinkAnimTimer + elapsedUs;
        if(input->blinkAnimTimer > totalTime)
        {
            //finished!
            input->blinkError = false;//will have to get set back to true elsewhere.
            input->blinkAnimTimer = 0;
            
            //reset default values
            input->inputBoxColor = input->inputBoxDefaultColor;
            setLeds(p->offLEDS,NUM_LEDS);
        }else
        {
            //check how many times blinkTime devides into remaining time.
            int times = (totalTime-input->blinkAnimTimer)/input->blinkTime;
            //remainder is still in microseconds
            //lets set it to something else.
           
            if(times %2 == 0)
            {
                //turn off the LEDs
                input->inputBoxColor = input->inputBoxDefaultColor;
                setLeds(p->errorALEDBlinkLEDS,NUM_LEDS);
            }else{
                //turn on the LEDs
                setLeds(p->errorBLEDBlinkLEDS,NUM_LEDS);
                input->inputBoxColor = input->inputBoxErrorColor;  
            }
        }       
    }
}

 void enterSpace(uint8_t x,uint8_t y,picrossSpaceType_t newSpace)
 {
    //this gets called frequently even if newSpace is current. 
    if(p->puzzle->level[x][y] != newSpace)
    {
        p->puzzle->level[x][y] = newSpace;
        //if SHOW HINTS is active
        //showing hints for the "mark as empty" felt wrong. its a note, not a commitment, and you can use it to remember locations too.
        if(newSpace == SPACE_FILLED && newSpace != p->puzzle->completeLevel[x][y])
        {
            if(p->input->showHints)
            {
                p->input->blinkError = true;
            }
        }
        //force validation check
        p->input->changedLevelThisFrame = true;
    }
 }

void countInput(picrossDir_t input)
{
    //I need to work out how this should properly behave.
    //in my game, you can just hold shift to make a counter with a nice tooltip and hoverbox, and move it around. its great.
    //but we dont have enough inputs here. Maybe you have to activate the counter with start?
    if(input != p->countState)
    {
        if(p->count == 1)
        {
            p->count = 2;
        }else{
            p->count = 1;
        }
    }else{
        p->count++;
    }
    p->countState = input;
}

//==========================================================
/// DRAWING SCENE
//========================================================

void drawPicrossScene(display_t* d)
{
    uint8_t w = p->puzzle->width;
    uint8_t h = p->puzzle->height;
    
    d->clearPx();
    drawBackground(d);

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
                    case OUTOFBOUNDS:
                    {
                        //uh oh!
                        break;
                    }
                }
            }
        }
        drawPicrossHud(d,&p->hint_font);
        drawPicrossInput(d);
    }//end if phase is solving   
    else if (p->currentPhase == PICROSS_YOUAREWIN)
    {
        for(int i = 0;i<w;i++)
        {
            for(int j = 0;j<h;j++)
            {
                drawSinglePixelFromWSG(d,i,j,&p->selectedLevel->completedWSG);
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

void drawPicrossHud(display_t* d,font_t* font)
{
    //Draw coordinates
    char textBuffer[6];
    snprintf(textBuffer, sizeof(textBuffer) - 1, "%d,%d", p->input->x+1,p->input->y+1);
    drawText(d, font, c555, textBuffer, 230, 220);

    //Draw counter
    snprintf(textBuffer, sizeof(textBuffer) - 1, "%d", p->count);
    drawText(d, font, c334, textBuffer, 200, 220);

    //width of thicker center lines
    uint8_t w = 5;
    //draw a vertical line every 5 units.
    for(int i=0;i<=p->puzzle->width;i++)//skip 0 and skip last. literally the fence post problem.
    {
        uint8_t s = p->drawScale;
        if(i == 0 || i == p->puzzle->width)
        {
            //draw border
            plotLine(d,
            ((i * s) + s + p->leftPad) >> 1,
            (s + p->topPad) >> 1,
            ((i * s) + s + p->leftPad) >> 1,
            ((p->puzzle->height * s) + s + p->topPad) >> 1,
            c115,0);
            continue;
        }
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
    for(int i=0;i<=p->puzzle->height;i++)
    {
        uint8_t s = p->drawScale;
        if(i == 0 || i == p->puzzle->width)
        {
            //draw border
            plotLine(d,
            (s + p->leftPad) >> 1,
            ((i * s) + s + p->topPad) >> 1,
            ((p->puzzle->width * s) + s + p->leftPad) >> 1,
            ((i * s) + s + p->topPad) >> 1,
            c115,0);
            continue;
        }
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
    int8_t vHintShift = 2;
    uint8_t h;
    uint8_t g = p->clueGap;//
    //todo: handle 10 or double-digit input 
    if(hint.isRow){
        int j = 0;
        for(int i = 0;i<5;i++)
        {
            h = hint.hints[4-i];
            box_t hintbox = boxFromCoord(-j-1,hint.index);
            hintbox.x0 = hintbox.x0 - (g * (j));
            hintbox.x1 = hintbox.x1 - (g * (j));
            //we have to flip the hints around. 
            if(h == 0){
                //dont increase j.
                
            }else if(h < 10){
                
                //drawBox(d,hintbox,c111,false,1);//for debugging. we will draw nothing when proper.

                char letter[1];
                sprintf(letter, "%d", h);//this function appears to modify hintbox.x0
                //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
                drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+hintbox.x1-p->drawScale) >> 1, (hintbox.y1-p->drawScale+vHintShift) >> 1);
                j++;//the index position, but only for where to draw. shifts the clues to the right.
            }else{
                //drawBox(d,hintbox,c111,false,1);//same as above
                char letter[1];
                sprintf(letter, "%d", h);
                //as a "temporary" workaround, we will use x1 and y1 and subtract the drawscale.
                drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+(hintbox.x1-p->drawScale)) >> 1, (hintbox.y1-p->drawScale+vHintShift) >> 1);
                sprintf(letter, "%d", hint.hints[4-i]%10);
                drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+(hintbox.x1-p->drawScale+p->drawScale/2)) >> 1, (hintbox.y1-p->drawScale+vHintShift) >> 1);
                j++;
            }
        }
    }else{
        int j = 0;
        for(int i = 0;i<5;i++)
        {
            h = hint.hints[4-i];
            box_t hintbox = boxFromCoord(hint.index,-j-1);
             if(h == 0){      
                //         
            }else if(h < 10){
                //drawBox(d,hintbox,c111,false,1);
                char letter[1];
                sprintf(letter, "%d", h);
                //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
                drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+hintbox.x1-p->drawScale) >> 1, (hintbox.y1-p->drawScale+vHintShift) >> 1);
                j++;//the index position, but only for where to draw. shifts the clues to the right.
            }else{
                //drawBox(d,hintbox,c111,false,1);//same as above
                char letter[1];
                sprintf(letter, "%d", h);
                //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
                drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+hintbox.x1-p->drawScale) >> 1, (hintbox.y1-p->drawScale+vHintShift) >> 1);
                sprintf(letter, "%d", hint.hints[4-i]%10);
                drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+hintbox.x1-p->drawScale+p->drawScale/2) >> 1, (hintbox.y1-p->drawScale+vHintShift) >> 1);
                j++;
            }
        }
    }
}

int8_t getHintShift(uint8_t hint)
{
    //these values determined by trial and error with the hint_font.
    switch(hint)
    {
        case 0:
            return 0;
        case 1:
            return 7;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            return 5;
        case 10:
            return -4;
    }

    return 0;
}

void drawPicrossInput(display_t* d)
{
    //Draw player input box
    box_t inputBox = boxFromCoord(p->input->x,p->input->y);
    inputBox.x1 = inputBox.x1+1;
    inputBox.y1 = inputBox.y1+1;//cover up both marking lines
    drawBox(d, inputBox, p->input->inputBoxColor, false, 1);
    //draw it twice as thicc 
    inputBox.x0 = inputBox.x0-2; 
    inputBox.y0 = inputBox.y0-2; 
    inputBox.x1 = inputBox.x1+2; 
    inputBox.y1 = inputBox.y1+2;
    drawBox(d, inputBox, p->input->inputBoxColor, false, 1);
}

void drawBackground(display_t* d)
{
    // Draw a dim blue background with a grey grid

    for(int16_t y = 0; y < d->h; y++)
    {
        for(int16_t x = 0; x < d->w; x++)
        {
            if(((x % 20) == 0) || ((y % 20) == 0))
            {
                d->setPx(x, y, c222); // Grid
            }
            else
            {
                d->setPx(x, y, c111); // Background
            }
        }
    }
}

void picrossGameButtonCb(buttonEvt_t* evt)
{
    p->input->btnState = evt->state;
}

void picrossExitGame(void)
{
    //set LED's to off.
    setLeds(p->offLEDS, NUM_LEDS);

    if (NULL != p)
    {
        freeFont(&(p->hint_font));
        // free(p->puzzle);
        free(p->input);
        // free(p->save);
        free(p);
        p = NULL;
    }    
}

///=====
// SAVING AND LOADING
//===========

void savePicrossProgress()
{
    for(int y = 0;y<10;y++)
    {
        for(int x = 0;x<10;x++)
        {
            //because level enum is <4, it will only write last 2 bits in the OR
            p->save->banks[y] =  p->save->banks[y] << 2;
            p->save->banks[y] = (p->save->banks[y] | p->puzzle->level[x][y]);  
        }
        writeNvs32(getBankName(y), p->save->banks[y]);
    }    
}
void loadPicrossProgress()
{
    for(int y = 0;y<10;y++)
    {
        readNvs32(getBankName(y), &p->save->banks[y]);
        for(int x = 0;x<10;x++)
        {
            //x is flipped because of the bit shifting direction.
            p->puzzle->level[9-x][y] = p->save->banks[y] & 3;//0x...00000011, we only care about last two bits
            p->save->banks[y] = p->save->banks[y] >> 2;//shift over twice for next load. Wait are we mangling it here on the load? Does it matter?
        }
    }    
}
void saveCompletedOnSelectedLevel(bool completed)
{
        //Save Victory
        int32_t victories = 0;
        if(p->selectedLevel->index <= 32)//levels 0-31
        {
            //Save the fact that we won.
            
            readNvs32("picross_Solves0", &victories);
            
            //shift 1 (0x00...001) over levelIndex times, then OR it with victories.
            if(completed)
            {
                victories = victories | (1 << (p->selectedLevel->index));
            }else{
                victories = victories & ~(1 << (p->selectedLevel->index));
            }
            //Save new number
            writeNvs32("picross_Solves0", victories);
        }else if(p->selectedLevel->index < 64)//levels 32-64
        {
            victories = 0;
            readNvs32("picross_Solves1", &victories);
            if(completed){
                victories = victories | (1 << (p->selectedLevel->index - 32));
            }else{
                victories = victories & ~(1 << (p->selectedLevel->index - 32));
            }
            writeNvs32("picross_Solves1", victories);
        }else if(p->selectedLevel->index < 96)//levels 65-95
        {
            victories = 0;
            readNvs32("picross_Solves2", &victories);
            if(completed){
                victories = victories | (1 << (p->selectedLevel->index - 64));
            }else{
                victories = victories & ~(1 << (p->selectedLevel->index - 64));
            }
            writeNvs32("picross_Solves2", victories);
        }
}
//I promise I tried to do a proper index->row/col mapping here, since we only need 100*2 bits of data. (<320 used here) but i kept messing the math up.
//I know that 120 bits is not a lot, but on principle, it bothers me and I would like to optimize this in the future.

char * getBankName(int i)
{
    switch(i)
    {
        case 0:
            return "pic_b_0";
        case 1:
            return "pic_b_1";
        case 2:
            return "pic_b_2";
        case 3:
            return "pic_b_3";
        case 4:
            return "pic_b_4";
        case 5:
            return "pic_b_5";
        case 6:
            return "pic_b_6";
        case 7:
            return "pic_b_7";
        case 8:
            return "pic_b_8";
        case 9:
            return "pic_b_9";
    }
    return "pic_b_x";
}