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
#include "picross_consts.h"
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
    picrossLevelDef_t levels[PICROSS_LEVEL_COUNT];
    picrossScreen_t screen;
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

//==============================================================================
// Variables
//==============================================================================

//Key values for persistent save data.
const char picrossCurrentPuzzleIndexKey[] = "pic_cur_ind";
const char picrossSavedOptionsKey[] = "picross_options"; 
const char picrossCompletedLevelData1[] = "picross_Solves1";
const char picrossCompletedLevelData2[] = "picross_Solves2";
const char picrossCompletedLevelData3[] = "picross_Solves3";

//Main menu strings
static const char str_picrossTitle[] = "pi-cross";//set game name here for top in-picross-menu name. Should match below.
static const char str_continue[] = "Continue";
static const char str_levelSelect[] = "Puzzle Select";
static const char str_howtoplay[] = "How To Play";
static const char str_exit[] = "Exit";

//Options Menu
static const char str_options[] = "Options";

static const char str_back[] = "Back";
static const char str_HintsOn[] = "Hints: On";
static const char str_HintsOff[] = "Hints: Off";
static const char str_GuidesOn[] = "Guides: On";
static const char str_GuidesOff[] = "Guides: Off";
static const char str_AnimateBGOn[] = "BG Animate: On";
static const char str_AnimateBGOff[] = "BG Animate: Off";
static const char str_eraseProgress[] = "Erase Progress";

swadgeMode modePicross =
{
    .modeName = "pi-cross",//set name here for how name appears in "games" menu. Should match above.
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

    pm->screen = PICROSS_MENU;
    
    setPicrossMainMenu(true);


    loadLevels();
    
    //set default options
    if(false == readNvs32(picrossSavedOptionsKey, &pm->options))
    {
        writeNvs32(picrossSavedOptionsKey, 4);//100 = bg on, guide off, options off
    }
}

void picrossExitMode(void)
{
    picrossExitLevelSelect();//this doesnt actually get called as we go in and out of levelselect (because it breaks everything), so lets call it now
    // picrossExitGame();//this is already getting called! hooray.
    deinitMeleeMenu(pm->menu);
    //p2pDeinit(&jm->p2p);
    freeFont(&(pm->mmFont));
    free(pm);
}

/**
 * @brief Loads in all of the level files. Each level is made up of a black/white data file, and a completed color version.
 * The black.white version can be white or transparent for empty spaces, and any other pixel value for filled spaces.
 * Clues and level size is determined by the images, but only 10x10 is currently supported. It will completely break if it's not 10x10 right now.
 * I would like to support any size that is <=15, including non-square puzzles. First is getting them to render and not write out of bounds, second is scaling and placing the puzzle on screen correctly.
 * 
 */
void loadLevels()
{
    //LEVEL SETUP AREA
    //DACVAK JUST SEARCH FOR "DACVAK" TO FIND THIS

    //Todo: we can cut our memory use down by about 2/3 if we use a naming convention and the titles to pull the wsg names.
    //snprint into loadWsg, i think. Make sure it all works with, say, apostophes and spaces.

    //any entry with lowercase names is testing data. CamelCase names are good to go. This is not convention, just nature of dac sending me files vs. my testing ones.
    pm->levels[0].title = "Fifteen";
    loadWsg("fifteen.wsg", &pm->levels[0].levelWSG);//15x15 testing
    loadWsg("fifteen_c.wsg", &pm->levels[0].completedWSG);

    pm->levels[1].title = "Bliss";
    loadWsg("Bliss.wsg", &pm->levels[1].levelWSG);//5x10
    loadWsg("Bliss_c.wsg", &pm->levels[1].completedWSG);

    pm->levels[2].title = "Snare Drum";
    loadWsg("Snare_Drum_PZL.wsg", &pm->levels[2].levelWSG);//10x10
    loadWsg("Snare_Drum_SLV.wsg", &pm->levels[2].completedWSG);

    pm->levels[3].title = "Danny";
    loadWsg("Danny_PZL.wsg", &pm->levels[3].levelWSG);//10x10
    loadWsg("Danny_SLV.wsg", &pm->levels[3].completedWSG);

    pm->levels[4].title = "Controller";
    loadWsg("Controller_PZL.wsg", &pm->levels[4].levelWSG);//10x10
    loadWsg("Controller_SLV.wsg", &pm->levels[4].completedWSG);

    pm->levels[5].title = "Pear";
    loadWsg("Pear_PZL.wsg", &pm->levels[5].levelWSG);//10x10
    loadWsg("Pear_SLV.wsg", &pm->levels[5].completedWSG);
    
    pm->levels[6].title = "Cherry";
    loadWsg("Cherry_PZL.wsg", &pm->levels[6].levelWSG);//10x10
    loadWsg("Cherry_SLV.wsg", &pm->levels[6].completedWSG);

    pm->levels[7].title = "Strawberry";
    loadWsg("Strawberry_PZL.wsg", &pm->levels[7].levelWSG);//10x10
    loadWsg("Strawberry_SLV.wsg", &pm->levels[7].completedWSG);

    pm->levels[8].title = "Coffee Bean";
    loadWsg("CoffeeBean_PZL.wsg", &pm->levels[8].levelWSG);//coffeBean is pretty hard for a 10x10
    loadWsg("CoffeeBean_SLV.wsg", &pm->levels[8].completedWSG);

    pm->levels[9].title = "Boat";   
    loadWsg("3_boat.wsg", &pm->levels[9].levelWSG);//10x10
    loadWsg("3_boat_c.wsg", &pm->levels[9].completedWSG);

    pm->levels[11].title = "Fountain Pen";
    loadWsg("Fountain_Pen_PZL.wsg", &pm->levels[10].levelWSG);//15x15
    loadWsg("Fountain_Pen_SLV.wsg", &pm->levels[10].completedWSG);

    pm->levels[10].title = "Note";
    loadWsg("Note_PZL.wsg", &pm->levels[11].levelWSG);//15x15
    loadWsg("Note_SLV.wsg", &pm->levels[11].completedWSG);

    pm->levels[11].title = "Banana";
    loadWsg("Banana_PZL.wsg", &pm->levels[12].levelWSG);//15x15
    loadWsg("Banana_SLV.wsg", &pm->levels[12].completedWSG);

    pm->levels[12].title = "Power Plug";
    loadWsg("Plug_PZL.wsg", &pm->levels[13].levelWSG);//15x15 - This one is on the harder side of things.
    loadWsg("Plug_SLV.wsg", &pm->levels[13].completedWSG);

    //dont forget to update PICROSS_LEVEL_COUNT (in #define in picross_consts.h) when adding levels.

    //set indices. Used to correctly set save data. levels are loaded without context of the levels array, so they carry the index info with them so we can save victories.
    for(int i = 0;i<PICROSS_LEVEL_COUNT;i++)//8 should = number of levels and that should = levelCount.
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
            drawMeleeMenu(pm->disp, pm->menu);
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
    uint8_t rowPos = pm->menu->selectedRow;
    // Set the position of the hovered row
    if(resetPos)
    {
        rowPos = 0;
    }

    //override the main menu with the options menu instead.
    
    if(pm->screen == PICROSS_OPTIONS)
    {
        //Draw the options menu
        resetMeleeMenu(pm->menu, str_options, picrossMainMenuCb);
        pm->menu->selectedRow = rowPos;//reset sets this to 0, so lets ... set it back.
        if(picrossGetSaveFlag(0))//are hints on?
        {
            addRowToMeleeMenu(pm->menu, str_HintsOn);
        }else{
            addRowToMeleeMenu(pm->menu, str_HintsOff);
        }

        if(picrossGetLoadedSaveFlag(1))//are guides on? (getHints loaded from perma, we can skip doing that again and just read local)
        {
            addRowToMeleeMenu(pm->menu, str_GuidesOn);
        }else{
            addRowToMeleeMenu(pm->menu, str_GuidesOff);
        }

        if(picrossGetLoadedSaveFlag(2))//Bg animate on?
        {
            addRowToMeleeMenu(pm->menu, str_AnimateBGOn);
        }else{
            addRowToMeleeMenu(pm->menu, str_AnimateBGOff);
        }
        addRowToMeleeMenu(pm->menu, str_eraseProgress);
        addRowToMeleeMenu(pm->menu, str_back);
        return;
    }
    
    //otherwise, if we are in ANY game state, this should default (and set) to picross_menu

    resetMeleeMenu(pm->menu, str_picrossTitle, picrossMainMenuCb);
    pm->menu->selectedRow = rowPos;

    //only display continue button if we have a game in progress.

    //attempt to read the saved index. If it doesnt exist, set it to -1.
    if((false == readNvs32(picrossCurrentPuzzleIndexKey, &pm->savedIndex)))
    {
        writeNvs32(picrossCurrentPuzzleIndexKey, -1);//if we enter this screen and instantly exist, also set it to -1 and we can just load that.
        pm->savedIndex = -1;//there is no current index.
    }

    //only show continue button if we have saved data.
    if(pm->savedIndex >= 0)
    {
        addRowToMeleeMenu(pm->menu, str_continue);
    }

    addRowToMeleeMenu(pm->menu, str_levelSelect);
    addRowToMeleeMenu(pm->menu, str_howtoplay);
    addRowToMeleeMenu(pm->menu, str_options);
    addRowToMeleeMenu(pm->menu, str_exit);
    pm->screen = PICROSS_MENU;
}

/**
 * @brief Frees level select menu and returns to the picross menu. Should not be called by not-the-level-select-menu.
 * 
 */
void returnToPicrossMenu(void)
{
    picrossExitLevelSelect();//free data
    setPicrossMainMenu(false);
}

/**
 * @brief Exits the tutorial mode.
 * 
 */
void exitTutorial(void)
{
    pm->screen = PICROSS_MENU;
    setPicrossMainMenu(false);
}

//menu button & options menu callbacks. Set the screen and call the appropriate start functions
void picrossMainMenuCb(const char* opt)
{
    if (opt == str_continue)
    {
        //get the current level index
        int currentIndex = 0;//just load 0 if its 0. 
        readNvs32(picrossCurrentPuzzleIndexKey, &currentIndex);

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
        pm->screen = PICROSS_MENU;
        setPicrossMainMenu(true);
    }else if(opt == str_options)
    {
        pm->screen = PICROSS_OPTIONS;
        setPicrossMainMenu(true);
    }else if(opt == str_HintsOff)
    {
        //turn hints back on
        picrossSetSaveFlag(0,true);
        setPicrossMainMenu(false);//re-setup menu with new text, but dont change cursor position
    }
    else if(opt == str_HintsOn)
    {
        //turn hints off
        picrossSetSaveFlag(0,false);
        setPicrossMainMenu(false);
    }else if(opt == str_GuidesOff)
    {
        //turn guides back on
        picrossSetSaveFlag(1,true);
        setPicrossMainMenu(false);//re-setup menu with new text, but dont change cursor position
    }
    else if(opt == str_GuidesOn)
    {
        //turn guides off
        picrossSetSaveFlag(1,false);
        setPicrossMainMenu(false);
    }else if(opt == str_AnimateBGOff)
    {
        //turn bgAnimate back on
        picrossSetSaveFlag(2,true);
        setPicrossMainMenu(false);//re-setup menu with new text, but dont change cursor position
    }
    else if(opt == str_AnimateBGOn)
    {
        //turn bgAnimate off
        picrossSetSaveFlag(2,false);
        setPicrossMainMenu(false);
    }else if(opt == str_eraseProgress)
    {
        //Erase all gameplay data
        //doesnt erase settings.
        writeNvs32(picrossCompletedLevelData1, 0);
        writeNvs32(picrossCompletedLevelData2, 0);
        writeNvs32(picrossCompletedLevelData3, 0);
        for(int i = 0;i<10;i++)
        {
            writeNvs32(getBankName(0), 0);
        }
    }
}

/**
 * @brief Selects a picross level then starts the game. Called by level select, and sends us into mode_picross.c Doesn't free memory.
 * 
 * @param selectedLevel Pointer to the a level struct.
 */
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

/**
 * @brief Save data is stored in a single integer, using the register as 32 flags. 
 * Hints on/off is pos 0.
 * Guide on/off is pos 1
 * BG Animation is pos 2
 * 
 * @param pos Pos (<32) which flag to get.
 * @return true 
 * @return false 
 */
bool picrossGetSaveFlag(int pos)
{
    if(false == readNvs32(picrossSavedOptionsKey, &pm->options))
    {
        writeNvs32(picrossSavedOptionsKey, 0);
    }

    return picrossGetLoadedSaveFlag(pos);
}

/**
 * @brief once you have called picrossGetSaveFlag for any position, you don't need to call it again until an option changes.
 * As we often read a bunch of values in a row, this function can be used after the first one to save on uneccesary read calls.
 * 
 * @param pos 
 * @return true 
 * @return false 
 */
bool picrossGetLoadedSaveFlag(int pos)
{
    int val = (pm->options & ( 1 << pos )) >> pos;
    return val == 1;
}

//also sets pm->options
/**
 * @brief Sets a flag. Sets pm->options, which is actually used by the game without necessarily having to (repeatedly) call picrossGetSaveFlag.
 * Of course, it also also saves it to perma.
 * 
 * @param pos index to get. 0<pos<32.
 * @param on save vale.
 */
void picrossSetSaveFlag(int pos, bool on)
{
    if(on)
    {
        pm->options = pm->options | (1 << (pos));
    }else{
        pm->options = pm->options & ~(1 << (pos));
    }
    writeNvs32(picrossSavedOptionsKey, pm->options);
}