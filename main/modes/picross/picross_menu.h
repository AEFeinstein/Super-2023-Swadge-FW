#ifndef PICROSS_MENU_H_
#define PICROSS_MENU_H_

#include "mode_picross.h"
#include "picross_select.h"

extern swadgeMode modePicross;
extern const char picrossCurrentPuzzleIndexKey[];
extern const char picrossSavedOptionsKey[];
extern const char picrossCompletedLevelData1[];
extern const char picrossCompletedLevelData2[];
extern const char picrossCompletedLevelData3[];

void setPicrossMainMenu(bool resetPos);
void returnToPicrossMenu(void);
void returnToLevelSelect(void);
void selectPicrossLevel(picrossLevelDef_t* selectedLevel);
void exitTutorial(void);
void picrossSetSaveFlag(int pos, bool on);
bool picrossGetSaveFlag(int pos);
bool picrossGetLoadedSaveFlag(int pos);

#endif