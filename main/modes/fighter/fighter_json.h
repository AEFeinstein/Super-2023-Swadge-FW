#ifndef _FIGHTER_JSON_H_
#define _FIGHTER_JSON_H_

#include "mode_fighter.h"
#include "linked_list.h"

fighter_t* loadJsonFighterData(uint8_t* numFighters, list_t* loadedSprites);
void freeFighterData(fighter_t* fighter, uint8_t numFighters);

void freeFighterSprites(list_t* loadedSprites);
wsg_t* loadFighterSprite(char* name, list_t* loadedSprites);

#endif