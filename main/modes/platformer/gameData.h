#ifndef _GAMEDATA_H_
#define _GAMEDATA_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include "led_util.h"
#include "common_typedef.h"
#include "swadgeMode.h"
#include "palette.h"
#include "musical_buzzer.h"

//==============================================================================
// Constants
//==============================================================================
/*static const song_t snd1up =
{
    .notes = 
    {
        {G_7, 40},{D_6, 40},{B_5, 80}
    },
    .numNotes = 3,
    .shouldLoop = false
};*/

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
    uint32_t comboScore;

    bool extraLifeCollected;
    uint8_t checkpoint;
    uint8_t levelDeaths;
    uint8_t initialHp;
    
    led_t leds[NUM_LEDS];

    paletteColor_t bgColor;

    char initials[3];
    uint8_t rank;

    uint8_t changeBgm;
    uint8_t currentBgm;
} gameData_t;

//==============================================================================
// Functions
//==============================================================================
void initializeGameData(gameData_t * gameData);
void initializeGameDataFromTitleScreen(gameData_t * gameData);
void updateLedsHpMeter(entityManager_t *entityManager, gameData_t *gameData);
void scorePoints(gameData_t * gameData, uint16_t points);
void addCoins(gameData_t * gameData, uint8_t coins);
void updateComboTimer(gameData_t * gameData);

#endif