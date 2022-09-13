//==============================================================================
// Includes
//==============================================================================

#include <string.h>
#include <stdlib.h>

#include "swadgeMode.h"
#include "esp_log.h"

#include "aabb_utils.h"
#include "nvs_manager.h"

#include "picross_menu.h"
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
void drawPicrossLevelWSG(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff);
void drawPicrossPreviewWindow(display_t* d, wsg_t* wsg);
//====
// Functions
//====

//Initiation
void picrossStartLevelSelect(display_t* disp, font_t* font, picrossLevelDef_t levels[])
{
    ls = calloc(1, sizeof(picrossLevelSelect_t));
    ls->disp = disp;
    ls->game_font = font;

    loadWsg("unknownPuzzle.wsg",&ls->unknownPuzzle);

    //Load in which levels have been completed.
    int32_t victories0 = 0;
    int32_t victories1 = 0;
    int32_t victories2 = 0;

    //picross_Solves1 would be for levels 33->64
    readNvs32(picrossCompletedLevelData1, &victories0);
    readNvs32(picrossCompletedLevelData2, &victories1);
    readNvs32(picrossCompletedLevelData3, &victories2);

    int32_t v;
    ls->allLevelsComplete = true;
    int j;//bit position of i in appropriate register
    for(int i = 0;i<PICROSS_LEVEL_COUNT;i++)
    {
        //todo: We will switch to blob storage once I get around to it, bypassing this hack with int32s.
        if(i<32){
            v = victories0;
            j = i;
        }else if(i<64)
        {
            v = victories1;
            j = i - 32;
        }else if(i<96)
        {
            v = victories2;
            j = i - 64;
        }
        
        //set completed data from save data.
        if(1 == (v & ( 1 << j )) >> j){
            levels[i].completed = true;
        }else{
            levels[i].completed = false;
            ls->allLevelsComplete = false;
        }

        ls->levels[i] = levels[i];
    }
    
    ls->hoverX = 0;
    ls->hoverY = 0;
    ls->hoverLevelIndex = 0;
    ls->prevBtnState = SELECT | START | BTN_A | BTN_B | UP | DOWN | LEFT | RIGHT;
    
    ls->btnState = 0;

    //visual settings
    ls->cols = 5;
    //rows*cols should = picrossLevelCount. If it doesn't, the UI will break lol. I can add a check for it, but i dont want to unless we need it.
    //The goal is just to make the correct amount of puzzles, and replace this line with a concretely set rows. Or replace rows/cols with a #DEFINE.
    ls->rows = (PICROSS_LEVEL_COUNT+(ls->cols-1)) / ls->cols;
    ls->paddingLeft = 10;
    ls->paddingTop = 20;
    ls->gap = 5;
    ls->gridScale = 30;//PICROSS_MAX_SIZE * 2
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
    //todo: quit with both start+select

    if (ls->btnState & SELECT && !(ls->prevBtnState & SELECT) && !(ls->btnState & BTN_A))
    {
        //exit to main menu
        returnToPicrossMenu();
        return;
    }
    //Choosing a Level
    if (ls->btnState & BTN_A && !(ls->prevBtnState & BTN_A) && !(ls->btnState & SELECT))
    {
        ls->chosenLevel = &ls->levels[ls->hoverLevelIndex];
        writeNvs32(picrossCurrentPuzzleIndexKey, ls->hoverLevelIndex);//save the selected level before we lose context of the index.
        selectPicrossLevel(ls->chosenLevel);
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
        if(ls->hoverY >= ls->rows)//todo: rows?
        {
            ls->hoverY = 0;
        }
    }
    else if (ls->btnState & UP && !(ls->prevBtnState & UP))
    {
        ls->hoverY--;
        if(ls->hoverY < 0)
        {
            ls->hoverY = ls->rows-1;
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
    drawText(d, font, c555, "Puzzle", 190, 30); 
    drawText(d, font, c555, "Select", 190, 60);   
    
    for(int i=0;i<PICROSS_LEVEL_COUNT;i++)
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
            drawPicrossLevelWSG(d,&ls->levels[i].completedWSG,x,y);
        }else
        {
            //Draw ? sprite
            drawPicrossLevelWSG(d,&ls->unknownPuzzle,x,y);
        }
    }
    // //Draw "name" of current level
    // char letter[1];
    // sprintf(letter, "%d", ls->hoverLevelIndex+1);//this function appears to modify hintbox.x0
    // //as a temporary workaround, we will use x1 and y1 and subtract the drawscale.
    // drawChar(d,c555, font->h, &font->chars[(*letter) - ' '], 158, 120);

    if(ls->hoverLevelIndex < PICROSS_LEVEL_COUNT){//jic
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
        //draw preview window
        if(ls->levels[ls->hoverLevelIndex].completed){
            //if completed, show victory image and green hover
            drawPicrossPreviewWindow(d,&ls->levels[ls->hoverLevelIndex].completedWSG);
            drawBox(d,inputBox,c151,false,0);
        }else{
            //if incomplete, show ? image and red hvoer
            drawPicrossPreviewWindow(d,&ls->unknownPuzzle);
            drawBox(d,inputBox,c511,false,0);
        }
    }

    if(ls->allLevelsComplete)
    {
        drawText(d,ls->game_font,c000,"You Are Win!",53,103);
        drawText(d,ls->game_font,c555,"You Are Win!",50,100);
    }
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
void drawPicrossLevelWSG(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff)
{
    if(NULL == wsg->px)
    {
        return;
    }

    //Draw the WSG into a 30 pixel square area. thats PICROSSMAXLEVELSIZE*2. This is also gridScale.
    //gridScale works because 5,10, and 15 will divide into 30 evenly. Those are the only levels we plan on really supporting.
    //if we just pick whatever gridscale, then this function isn't would be ... wonky
    //BUT if we do something else, uh... it'll maybe not be the right size but... should look close to centered.
    uint16_t pixelPerPixel = ls->gridScale/((wsg->w > wsg->h) ? wsg->w : wsg->h);
    
    //level draw size is 2 pixels for each square when drawing 15x15 squares.
    //3 pixels for 10x10
    //5 pixels for 5x5
    
    if(wsg->w != wsg->h)
    {
        if(wsg->h < wsg->w)
        {
            //we need to calculate new y offset to center the image.
            yOff = yOff + (((PICROSS_MAX_LEVELSIZE*2)-wsg->h)/2);
        }
        else
        {
            //calculate new x offset to center
            xOff = xOff + (((PICROSS_MAX_LEVELSIZE*2)-wsg->w)/2);
        }
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
                int16_t dstX = srcX*pixelPerPixel+xOff;
                int16_t dstY = srcY*pixelPerPixel+yOff;
                
                // Check bounds
                if(0 <= dstX && dstX < disp->w && 0 <= dstY && dstY <= disp->h)
                {
                    // //root pixel
                    // disp->setPx(dstX, dstY, wsg->px[(srcY * wsg->w) + srcX]);
                    // // Draw the pixel
                    for(int i = 0;i<pixelPerPixel;i++)
                    {
                        for(int j = 0;j<pixelPerPixel;j++)
                        {
                            disp->setPx(dstX+i, dstY+j, wsg->px[(srcY * wsg->w) + srcX]);
                        }
                        // disp->setPx(dstX+1, dstY, wsg->px[(srcY * wsg->w) + srcX]);
                        
                        // disp->setPx(dstX+1, dstY+1, wsg->px[(srcY * wsg->w) + srcX]);
                    }
                }
            }
        }
    }
}

/// @brief Draws the picross preview of the level. Really, it just draws a wsg at a very specific location using drawBox calls.It can likely be optimized (by pulling the actual drawing code from drawBox into here?)
/// @param d display
/// @param wsg image to draw. on hover, it will be told to draw the image of the level we are hoving over - a '?' image or the completed one, as appopriate. 
void drawPicrossPreviewWindow(display_t* d, wsg_t* wsg)
{
    uint8_t s = 3* ls->gridScale/((wsg->w > wsg->h) ? wsg->w : wsg->h);

    for(int i = 0;i<wsg->w;i++)
    {
        for(int j = 0;j<wsg->h;j++)
        {
            
            box_t box =
                {
                    .x0 = (i * s) + ls->paddingLeft+172,//110=left offet for image
                    .y0 = (j * s) + ls->paddingTop+90,//60=topoffset for image below text
                    .x1 = (i * s) + s + ls->paddingLeft+172,
                    .y1 = (j * s) + s + ls->paddingTop+90,
                };  
            drawBox(d, box, wsg->px[(j * wsg->w) + i], true, 0);
        }
    }
}


