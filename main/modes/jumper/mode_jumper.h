#ifndef _MODE_JUMPER_H_
#define _MODE_JUMPER_H_

#include "swadgeMode.h"

typedef enum{
    BLOCK_STANDARD = 0,
    BLOCK_STANDARDLANDED = 1,
    BLOCK_PLAYERLANDED = 2,
    BLOCK_COMPLETE = 3,
} jumperBlockType_t;

typedef enum{
    JUMPER_COUNTDOWN,
    JUMPER_GAMING,
    JUMPER_DEATH,
    JUMPER_WINSTAGE,
    JUMPER_GAME_OVER
} jumperGamePhase_t;

typedef enum
{
    CHARACTER_IDLE,
    CHARACTER_JUMPCROUCH,
    CHARACTER_JUMPING,
    CHARACTER_LANDING,
    CHARACTER_DYING,
    CHARACTER_DEAD
}jumperCharacterState_t;


typedef struct 
{
    wsg_t frames[7];
    uint8_t frameIndex;
    uint64_t frameTime;
    uint16_t x;
    uint16_t sx;
    uint16_t y;
    uint16_t sy;
    uint16_t dx;
    uint16_t dy;
    uint8_t row;
    uint8_t column;
    uint8_t block;
    uint8_t dBlock;


    uint32_t btnState;
    bool jumpReady;
    bool jumping;
    uint64_t jumpTime;
    jumperCharacterState_t state;
}jumperCharacter_t;


void jumperStartGame(display_t* disp, font_t* mmFont);
void jumperGameLoop(int64_t elapsedUs);
void jumperGameButtonCb(buttonEvt_t* evt);
void jumperExitGame(void);

#endif