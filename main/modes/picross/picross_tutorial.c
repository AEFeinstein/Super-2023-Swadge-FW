#include <string.h>
#include <stdlib.h>

#include "swadgeMode.h"
#include "aabb_utils.h"

#include "picross_menu.h"
#include "picross_tutorial.h"

//function primitives
void drawTutorial(display_t* disp);

//variables
picrossTutorial_t* tut;

void picrossStartTutorial(display_t* disp, font_t* font)
{
    tut = calloc(1, sizeof(picrossTutorial_t));
    tut->d = disp;
    tut->font = font;
    
    tut->pageIndex = 0;
    tut->totalPages = 1;

    tut->prevBtn = 0;
}
void picrossTutorialLoop(int64_t elapsedUs)
{
    //user input

    //exit on hitting select
    if((tut->btn & SELECT) && !(tut->prevBtn & SELECT))
    {
        //by convention of the rest of the code base, freeing memory and going back to menu in different functions
        //(maybe that shouldnt be how it is *cough*)
        picrossExitTutorial();
        exitTutorial();
        return;
    }else if((tut->btn & LEFT) && !(tut->prevBtn & LEFT))
    {
        //by convention of the rest of the code base, freeing memory and going back to menu in different functions
        //(maybe that shouldnt be how it is *cough*)
        if(tut->pageIndex == 0)
        {
            tut->pageIndex = tut->pageIndex -1;
        }else{
            tut->pageIndex--;
        }
    }else if((tut->btn & RIGHT) && !(tut->prevBtn & RIGHT))
    {
        //by convention of the rest of the code base, freeing memory and going back to menu in different functions
        //(maybe that shouldnt be how it is *cough*)
        if(tut->pageIndex == tut->pageIndex -1)
        {
            tut->pageIndex = 0;
        }else{
            tut->pageIndex++;
        }
    }

    //draw screen
    drawTutorial(tut->d);

    tut->prevBtn = tut->btn;
}

void drawTutorial(display_t* d)
{
    d->clearPx();
    //draw page tut->pageIndex of tutorial
}



void picrossTutorialButtonCb(buttonEvt_t* evt)
{
    tut->btn = evt->button;
}
void picrossExitTutorial(void)
{
    free(tut);
}