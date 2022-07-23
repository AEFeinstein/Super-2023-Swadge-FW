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
    NOT_TOUCHING_PLATFORM,
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
    FS_JUMPING,
    FS_STARTUP,
    FS_ATTACK,
    FS_COOLDOWN,
    FS_HITSTUN
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
    vector_t hitboxPos;
    vector_t hitboxSize;
    uint16_t damage;
    vector_t knockback;
    uint16_t hitstun;

    bool isProjectile;
    wsg_t* projSprite;
    uint16_t projDuration;
    vector_t projVelo;
    vector_t projAccel;
} attackHitbox_t;

typedef struct
{
    wsg_t* sprite;
    attackHitbox_t* hitboxes;
    vector_t sprite_offset;
    vector_t hurtbox_offset;
    vector_t hurtbox_size;
    vector_t velocity;
    uint16_t duration;
    uint8_t numHitboxes;
    bool attackConnected;
} attackFrame_t;

typedef struct
{
    wsg_t* startupLagSpr;
    wsg_t* endLagSpr;
    attackFrame_t* attackFrames;
    uint16_t startupLag;
    uint16_t endLag;
    uint16_t landingLag;
    uint8_t numAttackFrames;
    bool onlyFirstHit;
    bool attackConnected;
} attack_t;

typedef struct
{
    box_t area;
    bool canFallThrough;
} platform_t;

typedef struct
{
    vector_t pos;
    vector_t hurtbox_offset;
    vector_t size;
    vector_t originalSize;
    vector_t velocity;
    bool isInAir;
    bool ledgeJumped;
    bool isInvincible;
    uint16_t iFrameTimer;
    platformPos_t relativePos;
    const platform_t* touchingPlatform;
    const platform_t* passingThroughPlatform;
    uint8_t numJumps;
    uint8_t numJumpsLeft;
    uint16_t landingLag;
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
    wsg_t* landingLagSprite;
    /* Input Tracking */
    int32_t prevBtnState;
    int32_t btnState;
    /* Current state tracking */
    fighterState_t state;
    bool isAerialAttack;
    attackOrder_t cAttack;
    uint8_t attackFrame;
    int32_t stateTimer;
    int32_t fallThroughTimer;
    fighterDirection_t dir;
    int32_t shortHopTimer;
    bool isShortHop;
    int32_t damage;
    uint8_t stocks;
    /* Animation timer */
    int32_t animTimer;
    wsg_t* currentSprite;
} fighter_t;

typedef struct
{
    fighter_t* owner;
    wsg_t* sprite;

    vector_t size;
    vector_t pos;
    vector_t velo;
    vector_t accel;

    vector_t knockback;
    uint16_t duration;
    uint16_t damage;
    uint16_t hitstun;

    bool removeNextFrame;
    fighterDirection_t dir;
} projectile_t;

//==============================================================================
// Extern variables
//==============================================================================

extern swadgeMode modeFighter;

#endif