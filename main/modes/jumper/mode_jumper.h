#ifndef _MODE_JUMPER_H_
#define _MODE_JUMPER_H_

#include "swadgeMode.h"
#include "aabb_utils.h"

typedef enum{
    BLOCK_STANDARD = 0,
    BLOCK_STANDARDLANDED = 1,
    BLOCK_PLAYERLANDED = 2,
    BLOCK_COMPLETE = 3,
    BLOCK_WIN = 4,
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
    CHARACTER_DEAD,
    CHARACTER_NONEXISTING
}jumperCharacterState_t;


typedef struct
{
    float resetTime;
    uint64_t decideTime;
} jumperAI_t;

typedef struct 
{
    wsg_t frames[8];
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

    bool flipped;

    uint32_t btnState;    
    bool jumpReady;
    bool jumping;
    uint64_t jumpTime;
    int32_t respawnTime;
    jumperCharacterState_t state;
    jumperAI_t intelligence;
}jumperCharacter_t;


typedef struct
{
    uint8_t numTiles;
    uint8_t lives;
    int32_t level;
    int32_t time;
    int32_t seconds;
    int8_t blockOffset_x;
    int8_t blockOffset_y;
    wsg_t livesIcon;
    jumperBlockType_t blocks[30];
} jumperStage_t;

typedef struct
{
    wsg_t block[9];
    jumperGamePhase_t currentPhase;
    int64_t frameElapsed;
    display_t* d;
    font_t game_font;
    font_t* promptFont;
    jumperStage_t* scene;
    bool controlsEnabled;

} jumperGame_t;



void jumperStartGame(display_t* disp, font_t* mmFont);
void jumperGameLoop(int64_t elapsedUs);
void jumperGameButtonCb(buttonEvt_t* evt);
void jumperExitGame(void);

#endif