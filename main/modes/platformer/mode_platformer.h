#ifndef _MODE_PLATFORMER_H_
#define _MODE_PLATFORMER_H_
//==============================================================================
// Includes
//==============================================================================

#include "common_typedef.h"
#include "musical_buzzer.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define NUM_PLATFORMER_HIGH_SCORES 5

//==============================================================================
// Constants
//==============================================================================

extern const song_t sndHit;
extern const song_t sndSquish;
extern const song_t sndBreak;
extern const song_t sndCoin;
extern const song_t sndPowerUp;
extern const song_t sndJump1;
extern const song_t sndJump2;
extern const song_t sndJump3;
extern const song_t sndWarp;
extern const song_t sndHurt;
extern const song_t sndWaveBall;
extern const song_t sndGameStart;
extern const song_t sndDie;
extern const song_t sndMenuSelect;
extern const song_t sndMenuConfirm;
extern const song_t sndMenuDeny;
extern const song_t bgmDemagio;
extern const song_t bgmIntro;
extern const song_t bgmSmooth;

//==============================================================================
// Structs
//==============================================================================

typedef struct {
    uint32_t scores[NUM_PLATFORMER_HIGH_SCORES];
    char initials[NUM_PLATFORMER_HIGH_SCORES][3];
} platformerHighScores_t;

//==============================================================================
// Prototypes
//==============================================================================

void updateGame(platformer_t *platformer);
void updateTitleScreen(platformer_t *platformer);

extern swadgeMode modePlatformer;

#endif
