#ifndef _MODE_FIGHTER_H_
#define _MODE_FIGHTER_H_

//==============================================================================
// Includes
//==============================================================================

#include "swadgeMode.h"
#include "aabb_utils.h"

//==============================================================================
// Defines
//==============================================================================

#define FRAME_TIME_MS 33 // 30fps

//==============================================================================
// Enums
//==============================================================================

// Division by a power of 2 has slightly more instructions than rshift, but handles negative numbers properly!
#define SF 8 // Scaling factor, number of decimal bits

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
    FACING_RIGHT = 0,
    FACING_LEFT
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

typedef enum
{
    KING_DONUT,
    SUNNY,
    BIG_FUNKUS,
    SANDBAG,
    NO_CHARACTER
} fightingCharacter_t;

typedef enum
{
    BATTLEFIELD,
    FINAL_DESTINATION,
    HR_STADIUM,
    NO_STAGE,
} fightingStage_t;

typedef enum
{
    HR_CONTEST,
    MULTIPLAYER
} fightingGameType_t;

typedef enum
{
    NO_BOUNCE,
    BOUNCE_UP,
    BOUNCE_DOWN,
    BOUNCE_LEFT,
    BOUNCE_RIGHT
} bounceDir_t;

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
    uint8_t projSprite;
    uint16_t projDuration;
    vector_t projVelo;
    vector_t projAccel;
} attackHitbox_t;

typedef struct
{
    uint8_t sprite;
    attackHitbox_t* hitboxes;
    vector_t sprite_offset;
    vector_t hurtbox_offset;
    vector_t hurtbox_size;
    vector_t velocity;
    uint16_t duration;
    uint16_t iFrames;
    uint8_t numHitboxes;
    bool attackConnected;
} attackFrame_t;

typedef struct
{
    uint8_t startupLagSprite;
    uint8_t endLagSprite;
    attackFrame_t* attackFrames;
    uint16_t startupLag;
    uint16_t endLag;
    uint16_t landingLag;
    uint16_t iFrames;
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
    uint8_t numPlatforms;
    platform_t platforms[];
} stage_t;

typedef struct
{
    fightingCharacter_t character;
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
    bool isInFreefall;
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
    uint8_t idleSprite0;
    uint8_t idleSprite1;
    uint8_t runSprite0;
    uint8_t runSprite1;
    uint8_t jumpSprite;
    uint8_t duckSprite;
    uint8_t landingLagSprite;
    uint8_t hitstunGroundSprite;
    uint8_t hitstunAirSprite;
    vector_t sprite_offset;
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
    bounceDir_t bounceNextCollision;
    uint32_t damageGiven;
    /* Animation timer */
    int32_t animTimer;
    uint8_t currentSprite;
    uint32_t hitstopTimer;
    uint8_t hitstopShake;
} fighter_t;

typedef struct
{
    fighter_t* owner;
    uint8_t sprite;

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


// Structs to store data to draw a scene

typedef struct
{
    int16_t spritePosX;
    int16_t spritePosY;
    int16_t spriteDir;
    int16_t spriteIdx;
    int16_t damage;
    int16_t stocks;
} fighterSceneFighter_t;

typedef struct
{
    int16_t spritePosX;
    int16_t spritePosY;
    int16_t spriteDir;
    int16_t spriteIdx;
} fighterSceneProjectile_t;

typedef struct
{
    uint8_t msgType;
    uint32_t gameTimerUs;
    bool drawGo;
    uint16_t stageIdx;
    fighterSceneFighter_t f1;
    fighterSceneFighter_t f2;
    int16_t numProjectiles;
    fighterSceneProjectile_t projs[];
} fighterScene_t;

//==============================================================================
// Functions
//==============================================================================

void fighterStartGame(display_t* disp, font_t* mmFont, fightingGameType_t type,
                      fightingCharacter_t* fightingCharacter, fightingStage_t stage,
                      bool isPlayerOne);
void fighterExitGame(void);
void fighterGameLoop(int64_t elapsedUs);
void fighterGameButtonCb(buttonEvt_t* evt);
int32_t fighterGetButtonState(void);

void fighterRxButtonInput(int32_t btnState);
void fighterRxScene(const fighterScene_t* scene, uint8_t len);

void drawFighterScene(display_t* d, const fighterScene_t* sceneData);

#endif