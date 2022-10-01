#ifndef _FIGHTER_JSON_H_
#define _FIGHTER_JSON_H_

//==============================================================================
// Includes
//==============================================================================

#include "mode_fighter.h"

//==============================================================================
// Defines
//==============================================================================

// TODO this should be 2x sprites for a char
#define MAX_LOADED_SPRITES 100

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    char* name;
    wsg_t sprite;
} namedSprite_t;

//==============================================================================
// Function declarations
//==============================================================================

void loadJsonFighterData(fighter_t* fighter, const char* jsonFile, namedSprite_t* loadedSprites);
void freeFighterData(fighter_t* fighter, uint8_t numFighters);

void freeFighterSprites(namedSprite_t* loadedSprites);
uint8_t loadFighterSprite(char* name, namedSprite_t* loadedSprites);
wsg_t* getFighterSprite(uint8_t spriteIdx, namedSprite_t* loadedSprites);

#endif