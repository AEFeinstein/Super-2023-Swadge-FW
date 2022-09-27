#ifndef _FIGHTER_MP_RESULT_H_
#define _FIGHTER_MP_RESULT_H_

#include "display.h"
#include "mode_fighter.h"
#include "aabb_utils.h"

void initFighterMpResult(display_t* disp, font_t* font, uint32_t roundTime,
                         fightingCharacter_t self,  int8_t selfKOs, int16_t selfDmg,
                         fightingCharacter_t other, int8_t otherKOs, int16_t otherDmg);
void deinitFighterMpResult(void);
void fighterMpResultLoop(int64_t elapsedUs);

#endif