//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include "gameData.h"
#include "entityManager.h"

//==============================================================================
// Functions
//==============================================================================
 void initializeGameData(gameData_t * gameData){
    gameData->gameState = 0;
    gameData->btnState = 0;
    gameData->score = 0;
    gameData->lives = 3;
    gameData->countdown = 000;
    gameData->world = 1;
    gameData->level = 1;
    gameData->frameCount = 0;
    gameData->coins = 0;
    gameData->combo = 0;
    gameData->comboTimer = 0;
    gameData->bgColor = c000;
    gameData->initials[0] = 'A';
    gameData->initials[1] = 'A';
    gameData->initials[2] = 'A';
}

 void initializeGameDataFromTitleScreen(gameData_t * gameData){
    gameData->gameState = 0;
    gameData->btnState = 0;
    gameData->score = 0;
    gameData->lives = 3;
    gameData->countdown = 000;
    gameData->frameCount = 0;
    gameData->coins = 0;
    gameData->combo = 0;
    gameData->comboTimer = 0;
    gameData->bgColor = c000;
}

void updateLedsHpMeter(entityManager_t *entityManager, gameData_t *gameData){
    if(entityManager->playerEntity == NULL){
        return;
    }

    uint8_t hp = entityManager->playerEntity->hp;
    if(hp > 3){
        hp = 3;
    }

    //HP meter led pairs:
    //3 4
    //2 5
    //1 6
    for (int32_t i = 1; i < 7; i++)
    {
        gameData->leds[i].r = 0x80;
        gameData->leds[i].g = 0x00;
        gameData->leds[i].b = 0x00;
    }

    for (int32_t i = 1; i < 1+hp; i++)
    {
        gameData->leds[i].r = 0x00;
        gameData->leds[i].g = 0x80;

        gameData->leds[7-i].r = 0x00;
        gameData->leds[7-i].g = 0x80;
    }

    setLeds(gameData->leds, NUM_LEDS);
}

void scorePoints(gameData_t * gameData, uint16_t points){
    gameData->combo++;
    uint16_t comboPoints = points * gameData->combo;

    gameData->score += comboPoints;
    gameData->comboScore = comboPoints;
    
    gameData->comboTimer = 80;
}

void updateComboTimer(gameData_t * gameData){
    gameData->comboTimer--;

    if(gameData->comboTimer < 0){
        gameData->comboTimer = 0;
        gameData->combo = 0;
    }
};