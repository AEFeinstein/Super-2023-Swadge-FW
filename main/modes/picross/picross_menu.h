#ifndef PICROSS_MENU_H_
#define PICROSS_MENU_H_

#include "mode_picross.h"

extern swadgeMode modePicross;

void setPicrossMainMenu(void);
void returnToPicrossMenu(void);
void returnToLevelSelect(void);
void selectPicrossLevel(picrossLevelDef_t* selectedLevel);

#endif