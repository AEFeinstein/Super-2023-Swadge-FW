#ifndef _FIGHTER_JSON_H_
#define _FIGHTER_JSON_H_

#include "mode_fighter.h"

fighter_t* loadJsonFighterData(uint8_t* numFighters);
void freeFighterData(fighter_t* fighter, uint8_t numFighters);

#endif