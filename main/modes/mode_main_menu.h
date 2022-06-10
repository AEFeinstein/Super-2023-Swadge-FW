#ifndef _MODE_MAIN_MENU_H_
#define _MODE_MAIN_MENU_H_

#include "swadgeMode.h"

extern swadgeMode* swadgeModes[];
uint8_t getNumSwadgeModes(void);
void switchToSwadgeMode(uint8_t mode);

extern swadgeMode modemainMenu;

#endif