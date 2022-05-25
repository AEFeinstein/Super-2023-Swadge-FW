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

// Division by a power of 2 has slightly more instructions than rshift, but handles negative numbers properly!
#define SF (1 << 8) // Scaling factor, a nice power of 2

typedef enum
{
    ABOVE_PLATFORM,
    BELOW_PLATFORM,
    RIGHT_OF_PLATFORM,
    LEFT_OF_PLATFORM,
    FREE_FLOATING,
    PASSING_THROUGH_PLATFORM
} platformPos_t;

typedef enum
{
    FACING_LEFT,
    FACING_RIGHT
} fighterDirection_t;

typedef enum
{
    FS_IDLE,
    FS_RUNNING,
    FS_DUCKING,
    FS_JUMP,
    FS_FALLING,
    FS_FREEFALL, // After up special
    FS_STARTUP,
    FS_ATTACK,
    FS_COOLDOWN,
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
    NUM_ATTACKS,
    NO_ATTACK
} attackOrder_t;

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    wsg_t* sprite;

    vector_t size;
    vector_t pos;
    vector_t velo;
    vector_t accel;

    vector_t knockbackAng;
    uint16_t knockback;
    uint16_t duration;
    uint16_t damage;
    uint16_t hitstun;

    bool removeNextFrame;
    fighterDirection_t dir;
} projectile_t;

typedef struct
{
    uint16_t duration;
    vector_t hitboxPos;
    vector_t hitboxSize;
    uint16_t damage;
    uint16_t knockback;
    vector_t knockbackAng;
    uint16_t hitstun;
    wsg_t* sprite;

    bool isProjectile;
    wsg_t* projSprite;
    uint16_t projDuration;
    vector_t projVelo;
    vector_t projAccel;
} attackFrame_t;

typedef struct
{
    uint16_t startupLag;
    wsg_t* startupLagSpr;
    uint8_t numAttackFrames;
    attackFrame_t* attackFrames;
    uint16_t endLag;
    wsg_t* endLagSpr;
} attack_t;

typedef struct
{
    box_t area;
    bool canFallThrough;
} platform_t;

typedef struct
{
    /* Position too! */
    box_t hurtbox;
    vector_t size;
    vector_t velocity;
    platformPos_t relativePos;
    const platform_t* touchingPlatform;
    const platform_t* passingThroughPlatform;
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
    /* Attack data */
    attack_t attacks[NUM_ATTACKS];
    /* Sprite names */
    wsg_t* idleSprite0;
    wsg_t* idleSprite1;
    wsg_t* runSprite0;
    wsg_t* runSprite1;
    wsg_t* jumpSprite;
    wsg_t* duckSprite;
    /* Input Tracking */
    int32_t prevBtnState;
    int32_t btnState;
    /* Current state tracking */
    fighterState_t state;
    attackOrder_t cAttack;
    uint8_t attackFrame;
    int32_t stateTimer;
    int32_t fallThroughTimer;
    fighterDirection_t dir;
    int32_t shortHopTimer;
    bool isShortHop;
    /* Animation timer */
    int32_t animTimer;
    wsg_t* currentSprite;
} fighter_t;

//==============================================================================
// Extern variables
//==============================================================================

extern swadgeMode modeFighter;

#endif