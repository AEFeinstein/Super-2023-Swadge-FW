#ifndef _FIGHTER_RECORDS_H_
#define _FIGHTER_RECORDS_H_

#include "display.h"

void initFighterRecords(display_t* disp, font_t* font);
void deinitFighterRecords(void);
void fighterRecordsLoop(int64_t elapsedUs);
bool checkHomerunRecord(fightingCharacter_t character, int32_t distance);

#endif
