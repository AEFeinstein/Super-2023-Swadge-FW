#ifndef _MODE_JUMPER_H_
#define _MODE_JUMPER_H_

#include "swadgeMode.h"
#include "aabb_utils.h"
#include "musical_buzzer.h"

extern const song_t jumpDeathTune;
extern const song_t jumpGameOverTune;
extern const song_t jumpWinTune;
extern const song_t jumpGameLoop;
extern const song_t jumpPerfectTune;
extern const song_t jumpPlayerJump;
extern const song_t jumperPlayerCollect;
extern const song_t jumpPlayerBrokeCombo;
extern const song_t jumpCountdown;
extern const song_t jumpEvilDonutJump;
extern const song_t jumpBlumpJump;

typedef enum
{
    BLOCK_STANDARD = 0,
    BLOCK_STANDARDLANDED = 1,
    BLOCK_PLAYERLANDED = 2,
    BLOCK_COMPLETE = 3,
    BLOCK_WIN = 4,
    BLOCK_EVILSTANDARD = 5,
    BLOCK_EVILLANDED = 6,
    BLOCK_WARBLESTANDARD = 7,
    BLOCK_WARBLELANDEDA = 8,
    BLOCK_WARBLELANDEDB = 9,
} jumperBlockType_t;

typedef enum
{
    POWERUP_JOYSTICK,
} jumperPowerType_t;

typedef enum
{
    JUMPER_COUNTDOWN,
    JUMPER_GAMING,
    JUMPER_DEATH,
    JUMPER_WINSTAGE,
    JUMPER_GAME_OVER,
    JUMPER_PAUSE
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
} jumperCharacterState_t;


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
    uint8_t respawnBlock;
    bool respawnReady;
    bool flipped;

    uint32_t btnState;
    bool jumpReady;
    bool jumping;
    uint64_t jumpTime;
    int32_t respawnTime;
    jumperCharacterState_t state;
    jumperAI_t intelligence;
} jumperCharacter_t;


typedef struct
{
    
    uint32_t powerupTime;
    bool powerupSpawned;
    uint16_t frame;
    uint32_t frameTime;

    uint16_t x;
    uint16_t y;
    bool collected;
} jumperPowerup_t;

typedef struct
{
    float ledSpeed;
    float ledTimer;
    uint8_t numTiles;
    uint8_t lives;
    int32_t level;
    int32_t time;
    int32_t previousSecond;
    int32_t freezeTimer;
    int32_t seconds;
    int8_t blockOffset_x;
    int8_t blockOffset_y;
    uint32_t score;
    uint8_t combo;
    uint8_t perfect;
    bool pauseRelease;
    bool smallerScoreFont;

    jumperPowerup_t* currentPowerup;


    jumperBlockType_t blocks[30];
} jumperStage_t;

typedef struct
{
    uint32_t x;
    uint32_t y;
    uint8_t digits[3];
    uint32_t time;
} jumperMultiplier_t;

typedef struct
{
    wsg_t block[10];
    wsg_t digit[12];
    wsg_t livesIcon;
    wsg_t target;
    wsg_t powerup[4];
    jumperGamePhase_t currentPhase;
    int64_t frameElapsed;
    display_t* d;
    font_t smaller_game_font;
    font_t game_font;
    font_t outline_font;
    font_t fill_font;
    font_t* prompt_font;
    jumperStage_t* scene;
    bool controlsEnabled;
    bool ledEnabled;
    uint8_t respawnBlock;

    uint64_t jumperJumpTime;
    uint32_t highScore;

    jumperCharacter_t* player;
    jumperCharacter_t* evilDonut;
    jumperCharacter_t* blump;

    jumperMultiplier_t* multiplier;

} jumperGame_t;


void jumperStartGame(display_t* disp, font_t* mmFont,  bool ledEnabled);
void jumperGameLoop(int64_t elapsedUs);
void jumperGameButtonCb(buttonEvt_t* evt);
void jumperExitGame(void);

#endif