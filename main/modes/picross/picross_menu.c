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
#include "picross_tutorial.h"

//==============================================================================
// Enums & Structs
//==============================================================================

typedef enum
{
    PICROSS_MENU,
    PICROSS_LEVELSELECT,
    PICROSS_OPTIONS,
    PICROSS_GAME,
    PICROSS_TUTORIAL
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
    uint8_t settingsPos;
    int32_t options;//bit 0: hints
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
void picrossMenuOptionsCb(const char* opt);
void PicrossSetSaveFlag(int pos, bool on);
bool PicrossGetSaveFlag(int pos);

//==============================================================================
// Variables
//==============================================================================
//Todo: these maybe dont need to be static?
//how are const compiled in c? whats the smart way to do this?

static const char str_picrossTitle[] = "pi-Cross";
static const char str_continue[] = "Continue";
static const char str_levelSelect[] = "Puzzle Select";
static const char str_howtoplay[] = "How To Play";
static const char str_exit[] = "Exit";

//Options Menu
static const char str_options[] = "Options";

static const char str_back[] = "Back";
static const char str_HintsOn[] = "Hints: On";
static const char str_HintsOff[] = "Hints: Off";
static const char str_eraseProgress[] = "Erase Progress";

swadgeMode modePicross =
{
    .modeName = "pi-Cross",//set here and also above
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

    loadFont("mm.font", &(pm->mmFont));

    pm->menu = initMeleeMenu(str_picrossTitle, &(pm->mmFont), picrossMainMenuCb);

    setPicrossMainMenu(true);

    pm->screen = PICROSS_MENU;
    pm->options = 0;//todo: read data from disk.

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

    //any entry with lowercase names is testing data. CamelCase names are good to go.
    loadWsg("Cherry_PZL.wsg", &pm->levels[0].levelWSG);
    loadWsg("Cherry_SLV.wsg", &pm->levels[0].completedWSG);

    loadWsg("Strawberry_PZL.wsg", &pm->levels[1].levelWSG);
    loadWsg("Strawberry_SLV.wsg", &pm->levels[1].completedWSG);

    loadWsg("Snare_Drum_PZL.wsg", &pm->levels[2].levelWSG);
    loadWsg("Snare_Drum_SLV.wsg", &pm->levels[2].completedWSG);

    loadWsg("3_boat.wsg", &pm->levels[3].levelWSG);
    loadWsg("3_boat_c.wsg", &pm->levels[3].completedWSG);

    loadWsg("test2.wsg", &pm->levels[4].levelWSG);
    loadWsg("test2_c.wsg", &pm->levels[4].completedWSG);

    loadWsg("test1.wsg", &pm->levels[5].levelWSG);
    loadWsg("test1_complete.wsg", &pm->levels[5].completedWSG);

    //set indices. Used to correctly set save data. levels are loaded without context of their array, so they carry the index data with them.
    for(int i = 0;i<8;i++)//8 should = number of levels and that should = levelCount.
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
        case PICROSS_OPTIONS:
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
        case PICROSS_TUTORIAL:
        {
            picrossTutorialLoop(elapsedUs);
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
        case PICROSS_OPTIONS:
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
        case PICROSS_TUTORIAL:
        {
            picrossTutorialButtonCb(evt);
            break;
        }
            //No wifi mode stuff
    }
}


/////////////////////////

/**
 * @brief Sets up the top level menu for Picross
 */
void setPicrossMainMenu(bool resetPos)
{
    // Set the position of the hovered row
    if(resetPos)
    {
        pm->menu->selectedRow = 0;
    }

    //override the main menu with the options menu instead.
    
    if(pm->screen == PICROSS_OPTIONS)
    {
        //Draw the options menu
        resetMeleeMenu(pm->menu, str_options, picrossMainMenuCb);
        if(PicrossGetSaveFlag(0))//are hints on?
        {
            addRowToMeleeMenu(pm->menu, str_HintsOn);
        }else{
            addRowToMeleeMenu(pm->menu, str_HintsOff);
        }
        addRowToMeleeMenu(pm->menu, str_eraseProgress);
        addRowToMeleeMenu(pm->menu, str_back);
        return;
    }
    
    //otherwise, if we are in ANY game state, this should default (and set) to picross_menu

    resetMeleeMenu(pm->menu, str_picrossTitle, picrossMainMenuCb);

    //only display continue button if we have a game in progress.

    //attempt to read the value. If it doesnt exist, set it to -1.
    if((false == readNvs32("pic_cur_ind", &pm->savedIndex)))
    {
        writeNvs32("pic_cur_ind", -1);//if we enter this screen and instantly exist, also set it to -1 and we can just load that.
        pm->savedIndex = -1;//there is no current index.
    }

    //TODO: continue button not appearing?
    if(pm->savedIndex >= 0)
    {
        addRowToMeleeMenu(pm->menu, str_continue);
    }

    addRowToMeleeMenu(pm->menu, str_levelSelect);
    addRowToMeleeMenu(pm->menu, str_howtoplay);
    addRowToMeleeMenu(pm->menu, str_options);
    addRowToMeleeMenu(pm->menu, str_exit);
    pm->menuChanged = true;
    pm->screen = PICROSS_MENU;
}

void returnToPicrossMenu(void)
{
    picrossExitLevelSelect();//free data
    setPicrossMainMenu(false);
}

void exitTutorial(void)
{
    pm->screen = PICROSS_MENU;
    setPicrossMainMenu(false);
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
        pm->screen = PICROSS_TUTORIAL;
        picrossStartTutorial(pm->disp,&pm->mmFont);
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
    }else if(opt == str_back)
    {
        pm->menuChanged = true;
        pm->screen = PICROSS_MENU;
        setPicrossMainMenu(true);
    }else if(opt == str_options)
    {
        pm->menuChanged = true;
        pm->screen = PICROSS_OPTIONS;
        setPicrossMainMenu(true);
    }else if(opt == str_HintsOff)
    {
        //turn hints back on
        PicrossSetSaveFlag(0,true);
        pm->menuChanged = true;
        setPicrossMainMenu(false);//re-setup menu with new text, but dont change cursor position
    }
    else if(opt == str_HintsOn)
    {
        //turn hints off
        PicrossSetSaveFlag(0,false);
        pm->menuChanged = true;
        setPicrossMainMenu(false);
    }else if(opt == str_eraseProgress)
    {
        //Erase all gameplay data
        //doesnt erase settings.
        writeNvs32("picross_Solves0", 0);
        writeNvs32("picross_Solves1", 0);
        writeNvs32("picross_Solves2", 0);
        for(int i = 0;i<10;i++)
        {
            writeNvs32(getBankName(0), 0);
        }
        
    }
}

void selectPicrossLevel(picrossLevelDef_t* selectedLevel)
{
    //picrossExitLevelSelect();//we do this BEFORE we enter startGame.
    pm->screen = PICROSS_GAME;
    picrossStartGame(pm->disp, &pm->mmFont, selectedLevel, false);
}

void returnToLevelSelect()//todo: rename
{
    //todo: abstract this to function
    pm->screen = PICROSS_LEVELSELECT;
    //todo: fix below warning
    picrossStartLevelSelect(pm->disp,&pm->mmFont,pm->levels);      
}

bool PicrossGetSaveFlag(int pos)
{
    //read once on loading menu?

    if(false == readNvs32("pic_options", &pm->options))
    {
        writeNvs32("pic_options", 0);
    }

    
    int val = (pm->options & ( 1 << pos )) >> pos;
    return val == 1;//hints will be first bit. IT IS DECIDED
}
//also sets pm->options
void PicrossSetSaveFlag(int pos, bool on)
{
    if(on)
    {
        pm->options = pm->options | (1 << (pos));
    }else{
        pm->options = pm->options & ~(1 << (pos));
    }
    writeNvs32("pic_options", pm->options);
}