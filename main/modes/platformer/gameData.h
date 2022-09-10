#ifndef _GAMEDATA_H_
#define _GAMEDATA_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include "led_util.h"
#include "common_typedef.h"

//==============================================================================
// Constants
//==============================================================================
#define NUM_LEDS 8 //Also defined in swadgeMode.h

//==============================================================================
// Structs
//==============================================================================

typedef struct 
{
    int16_t btnState;
    int16_t prevBtnState;
    uint8_t gameState;
    uint8_t changeState;

    uint32_t score;
    uint8_t lives;
    uint8_t coins;
    int16_t countdown;
    uint16_t frameCount;

    uint8_t world;
    uint8_t level;

    uint16_t combo;
    int16_t comboTimer;
    uint16_t comboScore;
    
    led_t leds[NUM_LEDS];
} gameData_t;

//==============================================================================
// Functions
//==============================================================================
void initializeGameData(gameData_t * gameData);
void updateLedsHpMeter(entityManager_t *entityManager, gameData_t *gameData);
void scorePoints(gameData_t * gameData, uint16_t points);
void updateComboTimer(gameData_t * gameData);

#endif