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
#include "nvs_manager.h"

#include "mode_picross.h"
#include "picross_menu.h"
#include "picross_select.h"
#include "bresenham.h"
#include "picross_consts.h"//wait already included by mode_picross.h? i gotta look up how includes work

//==============================================================================
// Function Prototypes
//==============================================================================
void picrossUserInput(int64_t elapsedUs);
void picrossCheckLevel(void);
void picrossSetupPuzzle(bool cont);
void setCompleteLevelFromWSG(wsg_t* puzz);
void drawSinglePixelFromWSG(display_t* d,int x, int y, wsg_t* image);
bool hintsMatch(picrossHint_t a, picrossHint_t b);
bool hintIsFilledIn(picrossHint_t* hint);
box_t boxFromCoord(int8_t x, int8_t y);
picrossHint_t newHintFromPuzzle(uint8_t index, bool isRow, picrossSpaceType_t source[PICROSS_MAX_LEVELSIZE][PICROSS_MAX_LEVELSIZE]);
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
void picrossStartGame(display_t* disp, font_t* mmFont, picrossLevelDef_t* selectedLevel, bool cont)
{
    //calloc is 0'd and malloc leaves memory uninitialized. I dont know which to use so im not gonna touch it, and doing things once on load can be slower.
    p = calloc(1, sizeof(picrossGame_t));
    p->selectedLevel = selectedLevel;
    p->currentPhase = PICROSS_SOLVING;
    p->d = disp;
    p->exitThisFrame = false;

    //bg scrolling
    p->bgScrollTimer = 0;
    p->bgScrollSpeed = 50000;
    p->bgScrollXFrame = 0;
    p->bgScrollYFrame = 0;

    //Puzzle
    p->puzzle = calloc(1, sizeof(picrossPuzzle_t));    
    //puzzle gets set in picrossSetupPuzzle.
    
    //Input Setup
    p->input = calloc(1, sizeof(picrossInput_t));
    p->input->x=0;
    p->input->y=0;
    p->input->btnState=0;
    p->input->prevBtnState= 0x80 | 0x10 | 0x40 | 0x20;//prevents us from instantly fillling in a square because A is held from selecting level.
    p->countState = PICROSSDIR_IDLE;
    p->controlsEnabled = true;
    p->input->DASActive = false;
    p->input->blinkError = false;
    p->input->blinkAnimTimer =0;
    p->input->startHeldType = OUTOFBOUNDS;
    p->input->inputBoxColor = c430;
    p->input->inputBoxDefaultColor = c430;
    p->input->inputBoxErrorColor = c500;
    p->input->movedThisFrame = false;
    p->input->changedLevelThisFrame = false;
    p->count = 1;
    p->input->holdingDir = PICROSSDIR_IDLE; 

    //input settings (adjust these)
    p->input->firstDASTime = 400000;//.5 seconds
    p->input->DASTime = 180000;//1,000,000 = 1 second.
    p->input->blinkTime = 120000;//error blink. half a blink cycle (on)(off).
    p->input->blinkCount = 6;//twice blink count (3 blinks)

    //load options data
    p->input->showHints = picrossGetSaveFlag(0);
    p->input->showGuides = picrossGetLoadedSaveFlag(1);
    p->animateBG = picrossGetLoadedSaveFlag(2);

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
    for(int i=0;i<PICROSS_MAX_LEVELSIZE;i++)
    {
        p->saveBanks[i] = 0;
    }

    //Setup level
    picrossSetupPuzzle(cont);
}

void picrossSetupPuzzle(bool cont)
{   
    wsg_t *levelwsg = &p->selectedLevel->levelWSG;

    setCompleteLevelFromWSG(levelwsg);

    p->puzzle->width = levelwsg->w;
    p->puzzle->height = levelwsg->h;
    
    //Calculate display information, now that we know the width/height of the puzzle.

    //rows
    for(int i = 0;i<p->puzzle->height;i++)
    {
        //todo: incompatible pointer type here?
        p->puzzle->rowHints[i] = newHintFromPuzzle(i,true,p->puzzle->completeLevel);  
    }
    
    //cols
    for(int i = 0;i<p->puzzle->width;i++)
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
        savePicrossProgress();//clear out your previous data.
        saveCompletedOnSelectedLevel(false);//clear out victory data: opening a level resets victory.
    }

    //Write the hint labels for each row and column
    
    //TFT_WIDTH = 240. max width is width+(ceil(width/2)) (15+8 = 23) = 10

    //max w/h. Non-square puzzles? I hope to write it so they will just... kind of work!
    
    uint16_t totalXCount = p->puzzle->width+p->maxHintsX+1;
    uint16_t totalYCount = p->puzzle->height+p->maxHintsY+1;
    uint16_t size = (((totalXCount) > (totalYCount)) ? (totalXCount) : (totalYCount)) ;//+1
    uint16_t screenWidth = (p->d->w);
    uint16_t screenHeight = (p->d->h);

    p->clueGap = 2;//this only being used for horizontal clues has added some complexity.
    uint16_t clueGapTotal = (p->maxHintsX-1)*p->clueGap;

    //x/y rounds down, x(+y-1)/y rounds up. for positive numbers.
    p->drawScale = (screenWidth-(2*p->maxHintsX)+(size-1)-PICROSS_EXTRA_PADDING-clueGapTotal)/(size);//h drawscale
    uint16_t vdrawScale = (screenHeight-(2*p->maxHintsY)+(size-1)-PICROSS_EXTRA_PADDING)/(size);
    //min between hDrawScale and vDrawScale
    p->drawScale = (((vdrawScale) < (p->drawScale)) ? (vdrawScale) : (p->drawScale));
    //paddingL = (TFTWidth - (totalCount*scale))/2;

    //My bug right now is the /2. I need to make it ceil not floor
    p->leftPad = (screenWidth - ((totalXCount*p->drawScale)))/2 + p->maxHintsX*p->drawScale;
    p->topPad = (screenHeight - ((totalYCount*p->drawScale)))/2 + p->maxHintsY*p->drawScale;

    //load the font
    if(p->drawScale < 12)
    {
        //font
        loadFont("tom_thumb.font", &(p->hint_font));
    }else if(p->drawScale < 24){
        loadFont("ibm_vga8.font", &(p->hint_font));
    }else{
        loadFont("early_gameboy.font", &(p->hint_font));
    }
    p->vFontPad = (p->drawScale - p->hint_font.h)/2;
    //Calculate the shift to move the font square to the center of the level square.
    //fontShiftLeft = p->hont_font->w
}

//Scans the levelWSG and creates the finished version of the puzzle in the proper data format (2D array of enum).
void setCompleteLevelFromWSG(wsg_t* puzz)
{
    // wsg_t puzz = puzzle;
    //go row by row
    paletteColor_t col = c555;
    //we can't ignore out of bounds data, because we want to be able to reset without reading garbage
    for(int r = 0;r<PICROSS_MAX_LEVELSIZE;r++)//todo: puzzles that dont have the right size?
    {
        if(r < puzz->h){
            for(int c=0;c<PICROSS_MAX_LEVELSIZE;c++){
                if(c < puzz->w){
                    col = puzz->px[(r * puzz->w) + c];
                    if(col == cTransparent || col == c555){
                        p->puzzle->completeLevel[c][r] = SPACE_EMPTY;
                        continue;
                    }else{
                        p->puzzle->completeLevel[c][r] = SPACE_FILLED;
                        continue;
                    }
                }else{
                    p->puzzle->completeLevel[c][r] = OUTOFBOUNDS;
                    continue;
                }
            }
        }else{
            for(int c=0;c<PICROSS_MAX_LEVELSIZE;c++){
                if(c < puzz->h){
                    p->puzzle->completeLevel[c][r] = OUTOFBOUNDS;
                    continue; 
                }
            }
        }
    }
}

//lol i dont know how to do constructors, im a unity developer! anyway this constructs a source.
//we use the same data type for rows and columns, hence the bool flag. index is the position in the row/col, and we pass in the level.
//I guess that could be a pointer? Somebody more experienced with c, code review this and optimize please.
picrossHint_t newHintFromPuzzle(uint8_t index, bool isRow, picrossSpaceType_t source[PICROSS_MAX_LEVELSIZE][PICROSS_MAX_LEVELSIZE])
{
    picrossHint_t t;
    t.complete = false;
    t.filledIn = false;
    t.isRow = isRow;
    t.index = index;
    
    //set all hints to 0.
    //for smaller levels than 15x15, we should set this dynamically to the current max hint size.
    //bit things will _work_ with larger hints, so its a minor refactor later.
    for(int i = 0;i<PICROSS_MAX_HINTCOUNT;i++)
    {
         t.hints[i] = 0;
    }

    //todo: non-square puzzles. get width/height passed in from newHint.
    picrossSpaceType_t prev = OUTOFBOUNDS;//starts empty because out of bounds is empty for how hints work.
    int8_t blockNumber = 0;// 
    if(isRow){
        for(int i = 0;i<p->puzzle->width;i++)
        {
            if(source[i][index] == OUTOFBOUNDS)
            {
                continue;
            }

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
        }
        //Store the maximum number of actual visible hints, used for calculating display position.
        if(blockNumber > p->maxHintsX)
        {
            p->maxHintsX = blockNumber;
        }
    }else{
        //same logic but for columns
        for(int i = 0;i<p->puzzle->height;i++)
        {
            if(source[index][i] == OUTOFBOUNDS)
            {
                continue;
            }

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
         //Store the maximum number of actual visible hints, used for calculating display position.
        if(blockNumber > p->maxHintsY)
        {
            p->maxHintsY = blockNumber;
        }
    }
    return t;
}

void picrossGameLoop(int64_t elapsedUs)
{

    p->bgScrollTimer += elapsedUs;

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

        //Unsave progress. Hides "current" in the main menu. we dont need to zero-out the actual data that will just happen when we load a new level.
        writeNvs32(picrossCurrentPuzzleIndexKey, -1);

        //save the fact that we won
        saveCompletedOnSelectedLevel(true);

    }

    p->previousPhase = p->currentPhase;
}

void picrossCheckLevel()
{
    bool allFilledIn = false;
    bool f = false;
    //Update if the puzzle is filled in, which we use to grey-out hints when drawing them.
    //if performance is bad, lets just cut this feature.
    for(int i = 0;i<p->puzzle->height;i++)
    {
        f = hintIsFilledIn(&p->puzzle->rowHints[i]);
        allFilledIn = allFilledIn | f;
    }
    for(int i = 0;i<p->puzzle->width;i++)
    {
        f = hintIsFilledIn(&p->puzzle->colHints[i]);
        allFilledIn = allFilledIn | f;
    }

    //we only need to do this if the puzzle is completely filled in, and we already checked that above. 

    //check if the puzzle is correctly completed.
    for(int c = 0;c<p->puzzle->width;c++)
    {
        for(int r = 0;r<p->puzzle->height;r++)//flipped
        {
            if(p->puzzle->level[c][r] == SPACE_EMPTY || p->puzzle->level[c][r] == SPACE_MARKEMPTY)
            {
                if(p->puzzle->completeLevel[c][r] != SPACE_EMPTY)
                {
                    return;
                }
            }else//if space == SPACE_FILLED (we aren't using filled-hints so lets not bother specific if for now)
            {
                if(p->puzzle->completeLevel[c][r] != SPACE_FILLED)
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
    for(int i = 0;i<PICROSS_MAX_HINTCOUNT;i++)
    {
        if(a.hints[i] != b.hints[i])
        {
            return false;
        }
    }
    return true;
}
bool hintIsFilledIn(picrossHint_t* hint)
{
    if(hint->isRow)
    {
        for(uint8_t i = 0;i<p->puzzle->width;i++)
        {
            if(p->puzzle->level[i][hint->index] == SPACE_EMPTY)
            {
                hint->filledIn = false;
                return false;
            }
        }
    }else{
        for(uint8_t i = 0;i<p->puzzle->height;i++)
        {
            if(p->puzzle->level[hint->index][i] == SPACE_EMPTY)
            {
                hint->filledIn = false;
                return false;
            }
        }
    }
    hint->filledIn = true;
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

/**
 * @brief Master draw function for picross gameplay. Draws the board and calls other picross draw functions.
 * 
 */
void drawPicrossScene(display_t* d)
{
    uint8_t w = p->puzzle->width;
    uint8_t h = p->puzzle->height;
    
    d->clearPx();
    drawBackground(d);

    box_t box;
    //todo: move color selection to constants somewhere
    paletteColor_t emptySpaceCol = c555;
    paletteColor_t hoverSpaceCol = emptySpaceCol;//this value is only different if guides are turned on.
    if(p->input->showGuides)
    {
        //I wish I could tint this less than I can. What if, instead, we tint the border-lines surrounding the spaces, and not the spaces?
        hoverSpaceCol = c445;
    }

    if(p->currentPhase == PICROSS_SOLVING)
    {
        for(int i = 0;i<w;i++)
        {
            for(int j = 0;j<h;j++)
            {
                //shape of boxDraw
                //we will probably replace with "drawwsg"
                box = boxFromCoord(i,j);          
                switch(p->puzzle->level[i][j])
                {
                    case SPACE_EMPTY:
                    {
                        //slightly tint rows or columns
                        //todo: move this logic around to make code more efficient? 
                        if(!(p->input->x == i) != !(p->input->y == j))//this is a strange boolean logic hack to do (x==i XOR y==j)
                        {
                            drawBox(d, box, hoverSpaceCol, true, 0);
                        }else
                        {
                            drawBox(d, box, emptySpaceCol, true, 0);
                        }
                        break;
                    }
                    case SPACE_FILLED:
                    {
                        drawBox(d, box, c000, true, 0);
                        break;
                    }
                    case SPACE_MARKEMPTY:
                    {
                        drawBox(d, box, c531, true, 0);
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
    drawBox(d, box, v, true, 0);
}

//*
// * @brief Creates a box_t struct given the x/y coordinates. I haven't gotten to totally standardizing the coordinate system for picross yet, but if we are trying to adjust where on screen something goes, this function determines _most_ of it.
// * 
// * @param x 
// * @param y 
// * @return box_t 
// */
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
    char textBuffer[9];
    snprintf(textBuffer, sizeof(textBuffer) - 1, "%d,%d", p->input->x+1,p->input->y+1);
    drawText(d, font, c555, textBuffer, 10, 20);

    //Draw counter
    snprintf(textBuffer, sizeof(textBuffer) - 1, "%d", p->count);
    drawText(d, font, c334, textBuffer, 10, 20+font->h*2);

    //width of thicker center lines
    uint8_t w = 2;//p->drawScale/4;
    //draw a vertical line every grid
    for(int i=0;i<=p->puzzle->width;i++)//skip 0 and skip last. literally the fence post problem.
    {
        uint8_t s = p->drawScale;
        if(i == 0 || i == p->puzzle->width)
        {
            //draw border
            plotLine(d,
            ((i * s) + s + p->leftPad),
            (s + p->topPad),
            ((i * s) + s + p->leftPad),
            ((p->puzzle->height * s) + s + p->topPad),
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
            drawBox(d,box,c441,true,0);
            continue;
        }
        //this could be less confusing.
        plotLine(d,
            ((i * s) + s + p->leftPad),
            (s + p->topPad),
            ((i * s) + s + p->leftPad),
            ((p->puzzle->height * s) + s + p->topPad),
            c111,0);
        
    }

     //draw a horizontal line every 5 units.
     //todo: also do the full grid.
    for(int i=0;i<=p->puzzle->height;i++)
    {
        uint8_t s = p->drawScale;
        if(i == 0 || i == p->puzzle->height)
        {
            //draw border
            plotLine(d,
            (s + p->leftPad),
            ((i * s) + s + p->topPad),
            ((p->puzzle->width * s) + s + p->leftPad),
            ((i * s) + s + p->topPad),
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
            drawBox(d,box,c441,true,0);
            continue;
        }
        //this should be less confusing.
        plotLine(d,
            (s + p->leftPad),
            ((i * s) + s + p->topPad),
            ((p->puzzle->width * s) + s + p->leftPad),
            ((i * s) + s + p->topPad),
            c111,0);
     
    }

    //Draw the hints.
    //With square puzzles, we don't need two for loops. Perhaps this is... aspirational.
    for(int i=0;i<p->puzzle->height;i++)
    {
        drawHint(d,font,p->puzzle->rowHints[i]);
    }
    for(int i=0;i<p->puzzle->width;i++)
    {
        drawHint(d,font,p->puzzle->colHints[i]);
    }

}

void drawHint(display_t* d,font_t* font, picrossHint_t hint)
{
    int8_t vHintShift = p->vFontPad;//was 2
    uint8_t h;
    uint8_t g = p->clueGap;
    box_t hintbox = boxFromCoord(-1,hint.index);
    paletteColor_t hintShadeColor = c001;//todo: move to struct if we decide to keep this.
    paletteColor_t hintColor = hint.filledIn ? c333 : c555;

    if(hint.isRow){
        int j = 0;
        //if current row, draw background squares.
        if(p->input->showGuides && hint.index == p->input->y)
        {
            hintbox = boxFromCoord(-1,hint.index);
            hintbox.x0 = 0;
            drawBox(d,hintbox,hintShadeColor,true,0);
        }
        
        //draw clues if they... are a thing.
        //todo: do the math on the max number of hints that _this_ level has, and just use that for all the math.
        for(int i = 0;i<PICROSS_MAX_HINTCOUNT;i++)
        {
            h = hint.hints[PICROSS_MAX_HINTCOUNT-1-i];
            hintbox = boxFromCoord(-j-1,hint.index);
            hintbox.x0 = hintbox.x0 - (g * (j));
            hintbox.x1 = hintbox.x1 - (g * (j));
            //we have to flip the hints around. 
            if(h == 0){
                //dont increase j.
            }else if(h < 10){//single digit draws
                
                //drawBox(d,hintbox,c111,false,1);//for debugging. we will draw nothing when proper.

                char letter[4];
                sprintf(letter, "%d", h);//this function appears to modify hintbox.x0
                //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
                drawChar(d,hintColor, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+hintbox.x1-p->drawScale), (hintbox.y1-p->drawScale+vHintShift));
                j++;//the index position, but only for where to draw. shifts the clues to the right.
            }else{//double digit draws

                char letter[4];
                sprintf(letter, "%d", h);
                //as a "temporary" workaround, we will use x1 and y1 and subtract the drawscale.
                drawChar(d,hintColor, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+(hintbox.x1-p->drawScale)), (hintbox.y1-p->drawScale+vHintShift));
                sprintf(letter, "%d", hint.hints[PICROSS_MAX_HINTCOUNT-1-i]%10);
                drawChar(d,hintColor, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+(hintbox.x1-p->drawScale+p->drawScale/2)), (hintbox.y1-p->drawScale+vHintShift));
                j++;
            }
        }
    }else{
        int j = 0;
         //if current col, draw background square
        if(p->input->showGuides && hint.index == p->input->x)
        {
            hintbox = boxFromCoord(hint.index,-1);
            hintbox.y0 = 0;
            drawBox(d,hintbox,hintShadeColor,true,0);
        }

        //draw hints
        for(int i = 0;i<PICROSS_MAX_HINTCOUNT;i++)
        {
            h = hint.hints[PICROSS_MAX_HINTCOUNT-1-i];
            hintbox = boxFromCoord(hint.index,-j-1);          

             if(h == 0){      
                //         
            }else if(h < 10){//single digit numbers
                //drawBox(d,hintbox,c111,false,1);
                char letter[4];
                sprintf(letter, "%d", h);
                //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
                drawChar(d,hintColor, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+hintbox.x1-p->drawScale), (hintbox.y1-p->drawScale+vHintShift));
                j++;//the index position, but only for where to draw. shifts the clues to the right.
            }else{
                //drawBox(d,hintbox,c111,false,1);//same as above
                char letter[4];
                sprintf(letter, "%d", h);
                //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
                drawChar(d,hintColor, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+hintbox.x1-p->drawScale), (hintbox.y1-p->drawScale+vHintShift));
                sprintf(letter, "%d", hint.hints[PICROSS_MAX_HINTCOUNT-1-i]%10);
                drawChar(d,hintColor, font->h, &font->chars[(*letter) - ' '], (getHintShift(h)+hintbox.x1-p->drawScale+p->drawScale/2), (hintbox.y1-p->drawScale+vHintShift));
                j++;
            }
        }
    }
}

int8_t getHintShift(uint8_t hint)
{
    
    switch(hint)
    {
        case 0:
            return 0;
        case 1:
            return p->vFontPad + 2;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            return p->vFontPad + 2;
        case 10:
            return p->vFontPad - 4;
    }

    return 0;
}

void drawPicrossInput(display_t* d)
{
    //todo: rewrite this

    //Draw player input box
    box_t inputBox = boxFromCoord(p->input->x,p->input->y);
    inputBox.x1 = inputBox.x1+1;
    inputBox.y1 = inputBox.y1+1;//cover up both marking lines
    drawBox(d, inputBox, p->input->inputBoxColor, false, 0);
    //draw it twice as thicc 
    inputBox.x0 = inputBox.x0-1; 
    inputBox.y0 = inputBox.y0-1; 
    inputBox.x1 = inputBox.x1+1; 
    inputBox.y1 = inputBox.y1+1;
    drawBox(d, inputBox, p->input->inputBoxColor, false, 0);
}

/**
 * @brief Draws the background. 
 * 
 * @param d display to draw to.
 */
void drawBackground(display_t* d)
{
    if(p->animateBG)
    {
        if(p->bgScrollTimer >= p->bgScrollSpeed)
        {
            p->bgScrollTimer = 0;
            p->bgScrollXFrame++;
            p->bgScrollYFrame += p->bgScrollXFrame%2;//scroll y twice as slowly as x
            //loop frames
            p->bgScrollXFrame = p->bgScrollXFrame%20;
            p->bgScrollYFrame = p->bgScrollYFrame%20;
        }
        
        for(int16_t y = 0; y < d->h; y++)
        {
            for(int16_t x = 0; x < d->w; x++)
            {
                if(((x % 20) == 19-p->bgScrollXFrame) && ((y % 20) == p->bgScrollYFrame))
                {
                    d->setPx(x, y, c111); // Grid
                }
                else
                {
                   // d->setPx(x, y, c111); // Background
                }
            }
        }
    }else
    {
        //dont animate bg
        for(int16_t y = 0; y < d->h; y++)
        {
            for(int16_t x = 0; x < d->w; x++)
            {
                //d->setPx(x, y, c111); // Background. todo: save color values somewhere.
            }
        }
    }
}

/**
 * @brief Called by the swadge system. We cache inputs and process them in the game loop.
 * 
 * @param evt 
 */
void picrossGameButtonCb(buttonEvt_t* evt)
{
    p->input->btnState = evt->state;
}

/**
 * @brief Turn of LEDS, Free up memory, and exit.
 * I think this gets called when the swadge gets reset, so maybe I could put a savePicrossProgress() call here, but I want to double-check how this function is actually being used.
 * 
 */
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

/**
 * @brief Saves the current level to permastore. 
 * Each row gets its own int32. Each square needs 2 bits (enum is 4 options, 3 used by actual level and an outofbounds = 2 bits).
 * We save the data in the last 2 bits, then move the bank over 2 bits. repeat.
 * a 32 bit integer means we can support up to 16 values per row.
 * One integer per row means we don't have to re-write this code to deal with levels of different size.
 * We save the current level index elsewhere, and re-create the entire puzzle on loading, so we dont have to store any clues or w/h info.
 * 
 * Currently the puzzle only saves when we hit select to exit. Once I test performance on device, I would like to add saving it when you also hit start,
 * or as frequently as every time you make a change.
 */
void savePicrossProgress()
{
    //should we zero out unused rows? I Don't think we need to, but it kinda feels wrong not to lol.
    for(int y = 0;y<p->puzzle->height;y++)
    {
        for(int x = 0;x<p->puzzle->width;x++)
        {
            //because level enum is <4, it will only write last 2 bits in the OR
            p->saveBanks[y] =  p->saveBanks[y] << 2;
            p->saveBanks[y] = (p->saveBanks[y] | p->puzzle->level[x][y]);
        }
        writeNvs32(getBankName(y), p->saveBanks[y]);
    }    
}
void loadPicrossProgress()
{
    for(int y = 0;y<p->puzzle->height;y++)
    {
        readNvs32(getBankName(y), &p->saveBanks[y]);
        for(int x = 0;x<p->puzzle->width;x++)
        {
            //x is flipped because of the bit shifting direction.
            p->puzzle->level[p->puzzle->width-1-x][y] = p->saveBanks[y] & 3;//0x...00000011, we only care about last two bits
            p->saveBanks[y] = p->saveBanks[y] >> 2;//shift over twice for next load. Wait are we mangling it here on the load? Does it matter?
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
            
            readNvs32(picrossCompletedLevelData1, &victories);
            
            //shift 1 (0x00...001) over levelIndex times, then OR it with victories.
            if(completed)
            {
                victories = victories | (1 << (p->selectedLevel->index));
            }else{
                victories = victories & ~(1 << (p->selectedLevel->index));
            }
            //Save new number
            writeNvs32(picrossCompletedLevelData1, victories);
        }else if(p->selectedLevel->index < 64)//levels 32-64
        {
            victories = 0;
            readNvs32(picrossCompletedLevelData2, &victories);
            if(completed){
                victories = victories | (1 << (p->selectedLevel->index - 32));
            }else{
                victories = victories & ~(1 << (p->selectedLevel->index - 32));
            }
            writeNvs32(picrossCompletedLevelData2, victories);
        }else if(p->selectedLevel->index < 96)//levels 65-95
        {
            victories = 0;
            readNvs32(picrossCompletedLevelData3, &victories);
            if(completed){
                victories = victories | (1 << (p->selectedLevel->index - 64));
            }else{
                victories = victories & ~(1 << (p->selectedLevel->index - 64));
            }
            writeNvs32(picrossCompletedLevelData3, victories);
        }
}
//I promise I tried to do a proper index->row/col mapping here, since we only need 100*2 bits of data. (<320 used here) but i kept messing the math up.
//I know that 120 bits is not a lot, but on principle, it bothers me and I would like to optimize this in the future.
//update: now that I am storing up to 15x15 puzzles (30 bits of data in 32 bits of space, its only 15rows*2 bits of wasted bits. nice.)

char * getBankName(int i)
{
    //this is what it is.
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
        case 10:
            return "pic_b_10";
        case 11:
            return "pic_b_11";
        case 12:
            return "pic_b_12";
        case 13:
            return "pic_b_13";
        case 14:
            return "pic_b_14";
        case 15:
            return "pic_b_15";
    }
    return "pic_b_x";
}