#ifndef _FIGHTER_HR_RESULT_H_
#define _FIGHTER_HR_RESULT_H_

#include "display.h"
#include "mode_fighter.h"
#include "aabb_utils.h"

void initFighterHrResult(display_t* disp, font_t* font, fightingCharacter_t character, vector_t pos, vector_t vel,
                         int32_t gravity);
void deinitFighterHrResult(void);
void fighterHrResultLoop(int64_t elapsedUs);

#endif