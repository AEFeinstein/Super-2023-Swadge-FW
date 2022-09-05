//==============================================================================
// Functions
//==============================================================================

#include <stdlib.h>
#include <esp_log.h>

#include "swadgeMode.h"
#include "meleeMenu.h"
#include "nvs_manager.h"

#include "picross_menu.h"
#include "mode_picross.h"
#include "picross_select.h"
#include "mode_main_menu.h"

//==============================================================================
// Enums & Structs
//==============================================================================

typedef enum
{
    PICROSS_MENU,
    PICROSS_LEVELSELECT,
    PICROSS_GAME
} picrossScreen_t;

typedef struct
{
    font_t mmFont;
    meleeMenu_t* menu;
    display_t* disp;
    picrossLevelDef_t levels[8];
    picrossScreen_t screen;
    bool menuChanged;
    int32_t savedIndex;
} picrossMenu_t;
//==============================================================================
// Function Prototypes
//==============================================================================

void picrossEnterMode(display_t* disp);
void picrossExitMode(void);
void picrossMainLoop(int64_t elapsedUs);
void picrossButtonCb(buttonEvt_t* evt);
void loadLevels(void);
void picrossMainMenuCb(const char* opt);

//==============================================================================
// Variables
//==============================================================================

swadgeMode modePicross =
{
    .modeName = "NonogramFest",
    .fnEnterMode = picrossEnterMode,
    .fnExitMode = picrossExitMode,
    .fnMainLoop = picrossMainLoop,
    .fnButtonCallback = picrossButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
};

//Todo: these maybe dont need to be static?
static const char str_picrossTitle[] = "NonogramFest";
static const char str_continue[] = "Continue";
static const char str_howtoplay[] = "How To Play";
static const char str_levelSelect[] = "Puzzle Select";
static const char str_exit[] = "Exit";

picrossMenu_t* pm;

//==============================================================================
// Functions
//==============================================================================
/**
 * @brief Enter the picross mode by displaying the top level menu
 *
 * @param disp The display to draw to
 *
 */
void picrossEnterMode(display_t* disp)
{
    // Allocate and zero memory
    pm = calloc(1, sizeof(picrossMenu_t));

    pm->disp = disp;

    //Set continue data to -1 if it does not already exit.
    if((false == readNvs32("pic_cur_ind", &pm->savedIndex)))
    {
        writeNvs32("pic_cur_ind", -1);//if we enter this screen and instantly exist, also set it to -1 and we can just load that.
        pm->savedIndex = -1;//there is no current index.
    }

    loadFont("mm.font", &(pm->mmFont));

    pm->menu = initMeleeMenu(str_picrossTitle, &(pm->mmFont), picrossMainMenuCb);

    setPicrossMainMenu();

    pm->screen = PICROSS_MENU;

    loadLevels();
}

void picrossExitMode(void)
{
    picrossExitLevelSelect();//this doesnt actually get called as we go in and out of levelselect (because it breaks everything), so lets call it now

    picrossExitGame();
    deinitMeleeMenu(pm->menu);
    //p2pDeinit(&jm->p2p);
    freeFont(&(pm->mmFont));
    free(pm);
}

void loadLevels()
{
    //LEVEL SETUP AREA
    //DACVAK JUST SEARCH FOR "DACVAK" TO FIND THIS
    loadWsg("test1.wsg", &pm->levels[0].levelWSG);
    loadWsg("test1_complete.wsg", &pm->levels[0].completedWSG);

    loadWsg("test2.wsg", &pm->levels[1].levelWSG);
    loadWsg("test2_c.wsg", &pm->levels[1].completedWSG);

    loadWsg("3_boat.wsg", &pm->levels[2].levelWSG);
    loadWsg("3_boat_c.wsg", &pm->levels[2].completedWSG);

    loadWsg("stairs.wsg", &pm->levels[3].levelWSG);
    loadWsg("stairs_c.wsg", &pm->levels[3].completedWSG);
    //the current levelCOunt is set to 8, but 0-9 are not loaded. if you select them, it will just break.

    loadWsg("grid.wsg", &pm->levels[4].levelWSG);
    loadWsg("grid_c.wsg", &pm->levels[4].completedWSG);

    //set indices. Used to correctly set save data. levels are loaded without context of their array, so they carry the index data with them.
    for(int i = 0;i<8;i++)
    {
        pm->levels[i].index = i;
    }
}

/**
 * Call the appropriate main loop function for the screen being displayed
 *
 * @param elapsedUd Time.deltaTime
 */
void picrossMainLoop(int64_t elapsedUs)
{
    switch(pm->screen)
    {
        case PICROSS_MENU:
        {
            if (pm->menuChanged)
            {
                drawMeleeMenu(pm->disp, pm->menu);
                pm->menuChanged = false;
            }
            break;
        }
        case PICROSS_LEVELSELECT:
        {
            picrossLevelSelectLoop(elapsedUs);
            break;
        }
        case PICROSS_GAME:
        {
            picrossGameLoop(elapsedUs);
            break;
        }
    }
}

/**
 * @brief Call the appropriate button functions for the screen being displayed
 *
 * @param evt
 */
void picrossButtonCb(buttonEvt_t* evt)
{
    switch(pm->screen)
    {
        case PICROSS_MENU:
        {
            //Pass button events from the Swadge mode to the menu
            if (evt->down)
            {
                pm->menuChanged = true;
                meleeMenuButton(pm->menu, evt->button);
            }
            break;
        }
        case PICROSS_LEVELSELECT:
        {
            picrossLevelSelectButtonCb(evt);
            break;
        }
        case PICROSS_GAME:
        {
            picrossGameButtonCb(evt);
            break;
        }
            //No wifi mode stuff
    }
}


/////////////////////////

/**
 * @brief Sets up the top level menu for Picross
 */
void setPicrossMainMenu(void)
{
    resetMeleeMenu(pm->menu, str_picrossTitle, picrossMainMenuCb);

    //only display continue button if we have a game in progress.
    if(pm->savedIndex >= 0){
        addRowToMeleeMenu(pm->menu, str_continue);
    }
    addRowToMeleeMenu(pm->menu, str_levelSelect);
    addRowToMeleeMenu(pm->menu, str_howtoplay);
    addRowToMeleeMenu(pm->menu, str_exit);
    pm->menuChanged = true;
    pm->screen = PICROSS_MENU;
}

void returnToPicrossMenu(void)
{
    picrossExitLevelSelect();
    setPicrossMainMenu();
}

//menu button callbacks. Set the screen and call the appropriate start functions
void picrossMainMenuCb(const char* opt)
{
    if (opt == str_continue)
    {
        //get the current level index
        int currentIndex = 0;//just load 0 if its 0. 
        readNvs32("pic_cur_ind", &currentIndex);

        //load in the level we selected.
        //uh. read the currentLevelIndex and get the value from 
        picrossStartGame(pm->disp, &pm->mmFont, &pm->levels[currentIndex], true);
        pm->screen = PICROSS_GAME;
        return;
    }
    if (opt == str_howtoplay)
    {
        //how... do we play?
        return;
    }
    if (opt == str_levelSelect)
    {
        pm->screen = PICROSS_LEVELSELECT;
        picrossStartLevelSelect(pm->disp,&pm->mmFont,pm->levels);
        return;
    }
    if (opt == str_exit)
    {
        // Exit to main menu
        switchToSwadgeMode(&modeMainMenu);
        return;
    }
}

void selectPicrossLevel(picrossLevelDef_t* selectedLevel)
{
    //picrossExitLevelSelect();//we do this BEFORE we enter startGame.
    pm->screen = PICROSS_GAME;
    picrossStartGame(pm->disp, &pm->mmFont, selectedLevel, false);
}

void returnToLevelSelect()
{
    //todo: abstract this to function
    pm->screen = PICROSS_LEVELSELECT;
    //todo: fix below warning
    picrossStartLevelSelect(pm->disp,&pm->mmFont,pm->levels);      
}