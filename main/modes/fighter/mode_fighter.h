#ifndef _MODE_FIGHTER_H_
#define _MODE_FIGHTER_H_

//==============================================================================
// Includes
//==============================================================================

#include "swadgeMode.h"
#include "aabb_utils.h"

//==============================================================================
// Enums
//==============================================================================

typedef enum
{
    FREE_FLOATING,
    ABOVE_PLATFORM,
    BELOW_PLATFORM,
    RIGHT_OF_PLATFORM,
    LEFT_OF_PLATFORM
} platformPos_t;

typedef enum
{
    FS_IDLE,
    FS_RUNNING,
    FS_DUCKING,
    FS_JUMP_1,
    FS_JUMP_2,
    FS_FALLING,
    FS_FREEFALL, // After up special
    FS_GROUND_STARTUP,
    FS_GROUND_ATTACK,
    FS_GROUND_COOLDOWN,
    FS_AIR_STARTUP,
    FS_AIR_ATTACK,
    FS_AIR_COOLDOWN,
    FS_HITSTUN,
    FS_HITSTOP,
    FS_INVINCIBLE
} fighterState_t;

typedef enum
{
    UP_GROUND,
    DOWN_GROUND,
    DASH_GROUND,
    NEUTRAL_GROUND,
    NEUTRAL_AIR,
    FRONT_AIR,
    BACK_AIR,
    UP_AIR,
    DOWN_AIR,
    NUM_ATTACKS
} attackOrder_t;

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    uint16_t duration;
    vector_t hitboxPos;
    vector_t hitboxSize;
    uint16_t damage;
    uint16_t knockback;
    vector_t knockbackAng;
    uint16_t hitstun;
    char* sprite;
} attackFrame_t;

typedef struct
{
    uint16_t startupLag;
    char* startupLagSpr;
    uint8_t numAttackFrames;
    attackFrame_t* attackFrames;
    uint16_t endLag;
    char* endLagSpr;
} attack_t;

typedef struct
{
    /* Position too! */
    box_t hurtbox;
    vector_t size;
    vector_t velocity;
    platformPos_t relativePos;
    uint8_t numJumps;
    /* how floaty a jump is */
    int32_t gravity;
    /* A negative velocity applied when jumping.
     * The more negative, the higher the jump
     */
    int32_t jump_velo;
    /* Acceleration to run */
    int32_t run_accel;
    /* Acceleration to run */
    int32_t run_decel;
    /* Velocity maximum running velocity */
    int32_t run_max_velo;
    int32_t prevState;
    int32_t btnState;
    fighterState_t state;
    attackOrder_t cAttack;
    uint8_t attackFrame;
    int32_t timer;
    attack_t attacks[NUM_ATTACKS];
    char* idleSprite0;
    char* idleSprite1;
    char* jumpSprite;
    char* duckSprite;
} fighter_t;

//==============================================================================
// Extern variables
//==============================================================================

extern swadgeMode modeFighter;

#endif