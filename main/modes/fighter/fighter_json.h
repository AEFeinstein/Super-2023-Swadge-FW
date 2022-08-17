#ifndef _FIGHTER_JSON_H_
#define _FIGHTER_JSON_H_

#include "mode_fighter.h"
#include "linked_list.h"

void loadJsonFighterData(fighter_t* fighter, const char* jsonFile, list_t* loadedSprites);
void freeFighterData(fighter_t* fighter, uint8_t numFighters);

void freeFighterSprites(list_t* loadedSprites);
uint8_t loadFighterSprite(char* name, list_t* loadedSprites);
wsg_t* getFighterSprite(uint8_t spriteIdx, list_t* loadedSprites);

#endif