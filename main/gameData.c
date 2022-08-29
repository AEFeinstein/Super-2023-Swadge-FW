//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include "gameData.h"

//==============================================================================
// Constants
//==============================================================================

//==============================================================================
// Functions
//==============================================================================
 void initializeGameData(gameData_t * gameData){
    gameData->gameState = 0;
    gameData->btnState = 0;
    gameData->score = 0;
    gameData->lives = 3;
    gameData->countdown = 300;
    gameData->world = 1;
    gameData->level = 1;
    gameData->frameCount = 0;
    gameData->coins = 0;
}