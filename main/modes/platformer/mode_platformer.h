#ifndef _MODE_PLATFORMER_H_
#define _MODE_PLATFORMER_H_
//==============================================================================
// Includes
//==============================================================================
#include "common_typedef.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define NUM_PLATFORMER_HIGH_SCORES 5

//==============================================================================
// Constants
//==============================================================================

static const song_t bgmDemagio;
static const song_t bgmIntro;
static const song_t bgmSmooth;

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
