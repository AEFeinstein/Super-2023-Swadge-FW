//==============================================================================
// Notes
//==============================================================================

/* Addition, subtraction, and multiplication of 8 and 16 bit ints is 5 instructions
 * Addition, subtraction, and multiplication of 32 bit ints is 4 instructions
 *
 * Division of 8 and 16 bit ints is 6 instructions
 * Division of 32 bit ints is 4 instructions
 */

//==============================================================================
// Includes
//==============================================================================

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "bresenham.h"
#include "linked_list.h"
#include "led_util.h"

#include "mode_fighter.h"
#include "fighter_json.h"
#include "fighter_menu.h"

//==============================================================================
// Constants
//==============================================================================

#define IFRAMES_AFTER_SPAWN ((3 * 1000) / FRAME_TIME_MS) // 3 seconds
#define DRAW_DEBUG_BOXES

#define NUM_FIGHTERS 2
#define NUM_STOCKS 3

typedef enum
{
    COUNTING_IN,
    HR_BARRIER_UP,
    HR_BARRIER_DOWN,
    MP_GAME
} fighterGamePhase_t;

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    int64_t frameElapsed;
    fighter_t fighters[NUM_FIGHTERS];
    list_t projectiles;
    namedSprite_t* loadedSprites;
    display_t* d;
    font_t* mm_font;
    fightingStage_t stageIdx;
    fightingGameType_t type;
    uint8_t playerIdx;
    bool shouldSendButtonInput;
    bool buttonInputReceived;
    bool shouldSendScene;
    fighterScene_t* composedScene;
    uint8_t composedSceneLen;
    bool shouldDrawScene;
    int32_t gameTimerUs;
    int32_t printGoTimerUs;
    fighterGamePhase_t gamePhase;
    int32_t packetRetryUs;
} fightingGame_t;

//==============================================================================
// Function Prototypes
//==============================================================================

void getHurtbox(fighter_t* ftr, box_t* hurtbox);
#define setFighterState(f, st, sp, tm, kb) _setFighterState(f, st, sp, tm, kb, __LINE__);
void _setFighterState(fighter_t* ftr, fighterState_t newState, uint8_t newSprite, int32_t timer, vector_t* knockback,
                      uint32_t line);
void setFighterRelPos(fighter_t* ftr, platformPos_t relPos, const platform_t* touchingPlatform,
                      const platform_t* passingThroughPlatform, bool isInAir);
void checkFighterButtonInput(fighter_t* ftr);
bool updateFighterPosition(fighter_t* f, const platform_t* platforms, uint8_t numPlatforms);
void checkFighterTimer(fighter_t* ftr, bool hitstopActive);
void checkFighterHitboxCollisions(fighter_t* ftr, fighter_t* otherFtr);
void checkFighterProjectileCollisions(list_t* projectiles);

void checkProjectileTimer(list_t* projectiles, const platform_t* platforms,
                          uint8_t numPlatforms);
uint32_t getHitstop(uint16_t damage);

void getSpritePos(fighter_t* ftr, vector_t* spritePos);
fighterScene_t* composeFighterScene(uint8_t stageIdx, fighter_t* f1, fighter_t* f2, list_t* projectiles,
                                    uint8_t* outLen);
void drawFighter(display_t* d, fighter_t* ftr);
void drawFighterHud(display_t* d, font_t* font, int16_t f1_dmg, int16_t f1_stock, int16_t f2_dmg, int16_t f2_stock);
#ifdef DRAW_DEBUG_BOXES
    void drawFighterDebugBox(display_t* d, fighter_t* ftr);
    void drawProjectileDebugBox(display_t* d, list_t* projectiles);
#endif

// void fighterAccelerometerCb(accel_t* accel);
// void fighterAudioCb(uint16_t * samples, uint32_t sampleCnt);
// void fighterTemperatureCb(float tmp_c);
// void fighterTouchCb(touch_event_t* evt);
// void fighterEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
// void fighterEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

// void fighterConCbFn(p2pInfo* p2p, connectionEvt_t evt);
// void fighterMsgRxCbFn(p2pInfo* p2p, const char* msg, const char* payload, uint8_t len);
// void fighterMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);

//==============================================================================
// Variables
//==============================================================================

static fightingGame_t* f = NULL;

static const stage_t battlefield =
{
    .numPlatforms = 4,
    .platforms =
    {
        {
            .area =
            {
                .x0 = (14) << SF,
                           .y0 = (170) << SF,
                           .x1 = (14 + 212 - 1) << SF,
                           .y1 = (170 + 4) << SF,
            },
            .canFallThrough = false
        },
        {
            .area =
            {
                .x0 = (30) << SF,
                           .y0 = (130) << SF,
                           .x1 = (30 + 54 - 1) << SF,
                           .y1 = (130 + 4) << SF,
            },
            .canFallThrough = true
        },
        {
            .area =
            {
                .x0 = (156) << SF,
                            .y0 = (130) << SF,
                            .x1 = (156 + 54 - 1) << SF,
                            .y1 = (130 + 4) << SF,
            },
            .canFallThrough = true
        },
        {
            .area =
            {
                .x0 = (93) << SF,
                           .y0 = (90) << SF,
                           .x1 = (93 + 54 - 1) << SF,
                           .y1 = (90 + 4) << SF,
            },
            .canFallThrough = true
        }
    }
};


static const stage_t finalDest =
{
    .numPlatforms = 1,
    .platforms =
    {
        {
            .area =
            {
                .x0 = (14) << SF,
                           .y0 = (170) << SF,
                           .x1 = (14 + 212 - 1) << SF,
                           .y1 = (170 + 4) << SF,
            },
            .canFallThrough = false
        }
    }
};

static const stage_t hrStadium =
{
    .numPlatforms = 1,
    .platforms =
    {
        {
            .area =
            {
                .x0 = (50) << SF,
                           .y0 = (200) << SF,
                           .x1 = (280 - 50) << SF,
                           .y1 = (200 + 4) << SF,
            },
            .canFallThrough = false
        }
    }
};

// Keep in sync with fightingStage_t
static const stage_t* stages[] =
{
    &battlefield,
    &finalDest,
    &hrStadium
};

//==============================================================================
// Functions
//==============================================================================

/**
 * Initialize all data needed for the fighter game
 *
 * @param disp The display to draw to
 * @param mmFont The font to use for the HUD, already loaded
 * @param type The type of game to play
 * @param fightingCharacter Two characters to load
 * @param stage The stage to fight on
 * @param isPlayerOne true if this is player one, false if player two
 */
void fighterStartGame(display_t* disp, font_t* mmFont, fightingGameType_t type,
                      fightingCharacter_t* fightingCharacter, fightingStage_t stage,
                      bool isPlayerOne)
{
    // Allocate base memory for the mode
    f = calloc(1, sizeof(fightingGame_t));

    // Save the display
    f->d = disp;

    // Load a font
    f->mm_font = mmFont;

    // Keep track of the game type
    f->type = type;

    // Keep track of the stage
    f->stageIdx = stage;

    // Keep track of player order
    f->playerIdx = isPlayerOne ? 0 : 1;

    // Load fighter data
    f->loadedSprites = calloc(MAX_LOADED_SPRITES, sizeof(namedSprite_t));
    for(int i = 0; i < NUM_FIGHTERS; i++)
    {
        f->fighters[i].character = fightingCharacter[i];
        switch (f->fighters[i].character)
        {
            case KING_DONUT:
            {
                loadJsonFighterData(&f->fighters[i], "kd.json", f->loadedSprites);
                break;
            }
            case SUNNY:
            {
                loadJsonFighterData(&f->fighters[i], "sn.json", f->loadedSprites);
                break;
            }
            case BIG_FUNKUS:
            {
                loadJsonFighterData(&f->fighters[i], "bf.json", f->loadedSprites);
                break;
            }
            case SANDBAG:
            case NO_CHARACTER:
            {
                loadJsonFighterData(&f->fighters[i], "sb.json", f->loadedSprites);
                break;
            }
        }

        setFighterRelPos(&(f->fighters[i]), NOT_TOUCHING_PLATFORM, NULL, NULL, true);
        f->fighters[i].cAttack = NO_ATTACK;

        // Set the initial sprites
        setFighterState((&f->fighters[i]), FS_IDLE, f->fighters[i].idleSprite0, 0, NULL);

        // Spawn fighters in respective positions
        f->fighters[i].pos.x = ((((1 + i) * f->d->w) / 3) - ((f->fighters[i].size.x >> SF) / 2)) << SF;

        switch(type)
        {
            case MULTIPLAYER:
            {
                // Start with three stocks
                f->fighters[i].stocks = NUM_STOCKS;
                // Invincible after spawning
                f->fighters[i].iFrameTimer = IFRAMES_AFTER_SPAWN;
                f->fighters[i].isInvincible = true;
                break;
            }
            case HR_CONTEST:
            {
                // No stocks for HR Contest
                f->fighters[i].stocks = 0;
                // No iframes
                f->fighters[i].iFrameTimer = 0;
                f->fighters[i].isInvincible = false;
                break;
            }
        }

        // Start counting in the match
        f->gamePhase = COUNTING_IN;
        f->gameTimerUs = 3000000;
    }

    // Set some LEDs, just because
    // static led_t leds[NUM_LEDS] =
    // {
    //     {.r = 0x00, .g = 0x00, .b = 0x00},
    //     {.r = 0xFF, .g = 0x00, .b = 0xFF},
    //     {.r = 0x0C, .g = 0x19, .b = 0x60},
    //     {.r = 0xFD, .g = 0x08, .b = 0x07},
    //     {.r = 0x70, .g = 0x81, .b = 0xFF},
    //     {.r = 0xFF, .g = 0xCC, .b = 0x00},
    // };
    static led_t leds[NUM_LEDS] =
    {
        {.r = 0x00, .g = 0x00, .b = 0x00},
        {.r = 0x00, .g = 0x00, .b = 0x00},
        {.r = 0x00, .g = 0x00, .b = 0x00},
        {.r = 0x00, .g = 0x00, .b = 0x00},
        {.r = 0x00, .g = 0x00, .b = 0x00},
        {.r = 0x00, .g = 0x00, .b = 0x00},
    };
    setLeds(leds, NUM_LEDS);
}

/**
 * Free all data needed for the fighter game
 */
void fighterExitGame(void)
{
    if(NULL != f)
    {
        // Free any stray projectiles
        projectile_t* toFree;
        while (NULL != (toFree = pop(&f->projectiles)))
        {
            free(toFree);
        }

        // Free fighter data
        freeFighterData(f->fighters, NUM_FIGHTERS);

        // Free sprites
        freeFighterSprites(f->loadedSprites);
        free(f->loadedSprites);

        // Free the composed scene if it exists
        if(NULL != f->composedScene)
        {
            free(f->composedScene);
            f->composedScene = NULL;
        }

        // Free game data
        free(f);
        f = NULL;
    }
}

/**
 * Fill a fighter's hurtbox, accounting for the direction it's facing
 *
 * @param ftr The fighter to fill a hurtbox for
 * @param hurtbox The hurtbox to fill
 */
void getHurtbox(fighter_t* ftr, box_t* hurtbox)
{
    if(FACING_RIGHT == ftr->dir)
    {
        hurtbox->x0 = ftr->pos.x + ftr->hurtbox_offset.x;
        hurtbox->x1 = hurtbox->x0 + ftr->size.x;
    }
    else
    {
        hurtbox->x1 = ftr->pos.x + ftr->originalSize.x - ftr->hurtbox_offset.x;
        hurtbox->x0 = hurtbox->x1 - ftr->size.x;
    }
    hurtbox->y0 = ftr->pos.y + ftr->hurtbox_offset.y;
    hurtbox->y1 = hurtbox->y0 + ftr->size.y;
}

/**
 * Set this fighter's current state and adjust any related variables
 *
 * @param ftr       The fighter to set state for
 * @param newState  The new state
 * @param newSprite The new sprite for that state
 * @param timer     The time to stay in this state before transitioning (0 == forever)
 * @param knockback The knockback vector, may be NULL
 * @param line      The line number this was called from, for debugging
 */
void _setFighterState(fighter_t* ftr, fighterState_t newState, uint8_t newSprite, int32_t timer, vector_t* knockback,
                      uint32_t line)
{
    // Clean up variables when leaving a state
    if((FS_ATTACK == ftr->state) && (FS_ATTACK != newState) && (ftr->cAttack < NUM_ATTACKS))
    {
        // When leaving attack state, clear all 'attackConnected'
        ftr->attacks[ftr->cAttack].attackConnected = false;
        attackFrame_t* atk = ftr->attacks[ftr->cAttack].attackFrames;
        uint8_t numAttackFrames = ftr->attacks[ftr->cAttack].numAttackFrames;
        for(uint8_t atkIdx = 0; atkIdx < numAttackFrames; atkIdx++)
        {
            atk[atkIdx].attackConnected = false;
        }
    }
    else if((FS_DUCKING == ftr->state) && (FS_DUCKING != newState))
    {
        // Restore original hitbox
        ftr->size.y = ftr->originalSize.y;
        ftr->hurtbox_offset.y = 0;
    }

    // Set the new state
    ftr->state = newState;

    // Set the new sprite
    ftr->currentSprite = newSprite;

    // Set the timer
    ftr->stateTimer = timer;

    // Manage any other variables
    switch(newState)
    {
        case FS_STARTUP:
        case FS_ATTACK:
        case FS_COOLDOWN:
        {
            break;
        }
        case FS_HITSTUN:
        {
            int32_t absVelX = (knockback->x) > 0 ? (knockback->x) : -(knockback->x);
            int32_t absVelY = (knockback->y) > 0 ? (knockback->y) : -(knockback->y);
            if(absVelX > absVelY)
            {
                if(knockback->x > 0)
                {
                    // Traveling right, bounce to the left
                    ftr->bounceNextCollision = BOUNCE_LEFT;
                }
                else
                {
                    // Traveling left, bounce to the right
                    ftr->bounceNextCollision = BOUNCE_RIGHT;
                }
            }
            else
            {
                if(knockback->y > 0)
                {
                    // Traveling down, bounce up
                    ftr->bounceNextCollision = BOUNCE_UP;
                }
                else
                {
                    // Traveling up, bounce down
                    ftr->bounceNextCollision = BOUNCE_DOWN;
                }
            }
            break;
        }
        case FS_DUCKING:
        {
            // Half the height of the hurbox when ducking
            ftr->size.y = (ftr->size.y / 2);
            ftr->hurtbox_offset.y = ftr->size.y;
            ftr->cAttack = NO_ATTACK;
            break;
        }
        case FS_IDLE:
        case FS_RUNNING:
        case FS_JUMPING:
        {
            ftr->cAttack = NO_ATTACK;
            break;
        }
    }
}

/**
 * @brief Set a group of variables which define positions relative to platforms
 *
 * @param ftr The fighter to update
 * @param relPos A position relative to a platform
 * @param touchingPlatform The platform the fighter is touching, may be NULL
 * @param passingThroughPlatform The platform the fighter is passing through, may be NULL
 * @param isInAir true if the fighter is in the air, false if it is not
 */
void setFighterRelPos(fighter_t* ftr, platformPos_t relPos, const platform_t* touchingPlatform,
                      const platform_t* passingThroughPlatform, bool isInAir)
{
    ftr->relativePos = relPos;
    ftr->touchingPlatform = touchingPlatform;
    ftr->passingThroughPlatform = passingThroughPlatform;
    ftr->isInAir = isInAir;
}

/**
 * Run the main loop for the fighter game. When the time is ready, this will
 * handle button input synchronously, move fighters, check collisions, manage
 * projectiles, and pretty much everything else
 *
 * @param elapsedUs The time elapsed since the last time this was called
 */
void fighterGameLoop(int64_t elapsedUs)
{
    switch(f->gamePhase)
    {
        case COUNTING_IN:
        {
            f->gameTimerUs -= elapsedUs;
            if(f->gameTimerUs <= 0)
            {
                // After count-in, transition to the appropriate state
                if(MULTIPLAYER == f->type)
                {
                    f->gameTimerUs = 0; // Count up after this
                    f->gamePhase = MP_GAME;
                }
                else if (HR_CONTEST == f->type)
                {
                    f->gameTimerUs = 15000000; // 15s total
                    f->gamePhase = HR_BARRIER_UP;
                }
                // Print GO!!! for 1s after COUNTING_IN elapses
                f->printGoTimerUs = 1000000;
            }
            break;
        }
        case HR_BARRIER_UP:
        {
            f->gameTimerUs -= elapsedUs;
            if(f->gameTimerUs <= 5000000) // last 5s
            {
                f->gamePhase = HR_BARRIER_DOWN;
            }
            break;
        }
        case HR_BARRIER_DOWN:
        {
            f->gameTimerUs -= elapsedUs;
            if(f->gameTimerUs <= 0)
            {
                f->gameTimerUs = 0;
                // Initialize the result
                fighterShowHrResult(f->fighters[0].character, f->fighters[1].pos, f->fighters[1].velocity, f->fighters[1].gravity,
                                    hrStadium.platforms[0].area.x1);
                // Deinit the game
                fighterExitGame();
                // Return after deinit
                return;
            }
            break;
        }
        case MP_GAME:
        {
            // Timer counts up in this state
            f->gameTimerUs += elapsedUs;
            break;
        }
    }

    // Don't print GO!!! forever
    if(f->printGoTimerUs > 0)
    {
        f->printGoTimerUs -= elapsedUs;
    }

    // Keep track of time and only calculate frames every FRAME_TIME_MS
    f->frameElapsed += elapsedUs;
    if (f->frameElapsed > (FRAME_TIME_MS * 1000))
    {
        f->frameElapsed -= (FRAME_TIME_MS * 1000);

        switch(f->type)
        {
            case MULTIPLAYER:
            {
                // Start by sending your buttons to the other swadge
                if(1 == f->playerIdx)
                {
                    // Raise flag to send button input to player 0
                    f->shouldSendButtonInput = true;
                    f->packetRetryUs = 1;
                }
                break;
            }
            case HR_CONTEST:
            {
                // All button input is local
                f->buttonInputReceived = true;
                break;
            }
        }
    }

    bool hitstopActive = (f->fighters[0].hitstopTimer || f->fighters[1].hitstopTimer);

    if(f->buttonInputReceived)
    {
        f->buttonInputReceived = false;

        if(!hitstopActive)
        {
            // Check fighter button inputs
            checkFighterButtonInput(&f->fighters[0]);
            checkFighterButtonInput(&f->fighters[1]);

            // Move fighters, and if one KO'd and the round ended, return
            if(updateFighterPosition(&f->fighters[0], stages[f->stageIdx]->platforms, stages[f->stageIdx]->numPlatforms))
            {
                return;
            }
            if(updateFighterPosition(&f->fighters[1], stages[f->stageIdx]->platforms, stages[f->stageIdx]->numPlatforms))
            {
                return;
            }
        }

        // If the home run contest ended early, this will be NULL
        if(NULL == f)
        {
            return;
        }

        // Update timers. This transitions between states and spawns projectiles
        checkFighterTimer(&f->fighters[0], hitstopActive);
        checkFighterTimer(&f->fighters[1], hitstopActive);

        if(!hitstopActive)
        {
            // Update projectile timers. This moves projectiles and despawns if necessary
            checkProjectileTimer(&f->projectiles, stages[f->stageIdx]->platforms, stages[f->stageIdx]->numPlatforms);

            // Check for collisions between hitboxes and hurtboxes
            checkFighterHitboxCollisions(&f->fighters[0], &f->fighters[1]);
            checkFighterHitboxCollisions(&f->fighters[1], &f->fighters[0]);
            // Check for collisions between projectiles and hurtboxes
            checkFighterProjectileCollisions(&f->projectiles);
        }

        // Free scene before composing another
        if(NULL != f->composedScene)
        {
            free(f->composedScene);
        }
        f->composedSceneLen = 0;
        f->composedScene = composeFighterScene(f->stageIdx, &f->fighters[0], &f->fighters[1],
                                               &f->projectiles, &(f->composedSceneLen));

        // Raise flag to send to other if this is multiplayer
        if(MULTIPLAYER == f->type)
        {
            f->shouldSendScene = true;
            f->packetRetryUs = 1;
        }


        // char dbgStr[256];
        // box_t hb;
        // getHurtbox(&f->fighters[0], &hb);
        // sprintf(dbgStr, "{[%d,%d],[%d,%d]},%d",
        //     f->fighters[0].pos.x >> SF,
        //     f->fighters[0].pos.y >> SF,
        //     f->fighters[0].velocity.x,
        //     f->fighters[0].velocity.y,
        //     boxesCollide(battlefield[2].area, hb, SF));
        // drawText(f->d, &(f->mm_font), c444, dbgStr, 0, 0);

        // ESP_LOGI("FTR", "{[%d, %d], [%d, %d], %d}",
        //     f->fighters[0].hurtbox.x0 >> SF,
        //     f->fighters[0].hurtbox.y0 >> SF,
        //     f->fighters[0].velocity.x,
        //     f->fighters[0].velocity.y,
        //     f->fighters[0].relativePos);
    }

    // If we should draw a scene after the background was drawn
    if(f->shouldDrawScene && NULL != f->composedScene)
    {
        // Lower the flag
        f->shouldDrawScene = false;
        // Draw the scene
        drawFighterScene(f->d, (const fighterScene_t*)f->composedScene);
    }

    // If this is multiplayer, manage transmissions
    if(MULTIPLAYER == f->type)
    {
        // If the retry timer is active
        if(f->packetRetryUs > 0)
        {
            // Decrement it
            f->packetRetryUs -= elapsedUs;
            // If it expired
            if(f->packetRetryUs <= 0)
            {
                // Blast the buttons until the scene is received
                if(f->shouldSendButtonInput)
                {
                    fighterSendButtonsToOther(f->fighters[f->playerIdx].btnState);
                }

                // Blast the scene until the bottons are received
                if(f->shouldSendScene)
                {
                    fighterSendSceneToOther(f->composedScene, f->composedSceneLen);
                }
            }
        }
    }
}

/**
 * @brief Set the time to retry sending a packet
 *
 * @param retryTime The time in microseconds to wait before retrying a packet
 */
void setFighterRetryTimeUs(int32_t retryTime)
{
    if(NULL != f)
    {
        f->packetRetryUs = retryTime;
    }
}

/**
 * Check all timers for the given fighter.
 * Manage transitons between fighter attacks.
 * Create a projectile if the attack state transitioned to is one.
 *
 * @param ftr The fighter to to check timers for
 * @param hitstopActive true if hitstop is active and some timers should be ignored
 */
void checkFighterTimer(fighter_t* ftr, bool hitstopActive)
{
    // Tick down the hitstop timer
    if(ftr->hitstopTimer)
    {
        ftr->hitstopTimer--;
        ftr->hitstopShake = (ftr->hitstopShake + 1) % 2;
    }
    else
    {
        ftr->hitstopShake = 0;
    }

    // Don't check other timers if hitstop is active
    if(hitstopActive)
    {
        return;
    }

    // Tick down the iframe timer
    if(ftr->iFrameTimer > 0)
    {
        ftr->iFrameTimer--;
        // When it hits zero
        if(0 == ftr->iFrameTimer)
        {
            // Become vulnerable
            ftr->isInvincible = false;
        }
    }

    // If the fighter is idle
    if(FS_IDLE == ftr->state)
    {
        // tick down the idle timer
        if(ftr->animTimer > 0)
        {
            ftr->animTimer--;
        }
        else
        {
            // When it elapses, reset it to 0.5s
            ftr->animTimer = 500 / FRAME_TIME_MS;
            // And switch the idle sprite
            if(ftr->currentSprite == ftr->idleSprite1)
            {
                ftr->currentSprite = ftr->idleSprite0;
            }
            else
            {
                ftr->currentSprite = ftr->idleSprite1;
            }
        }
    }
    else if (FS_RUNNING == ftr->state)
    {
        // tick down the idle timer
        if(ftr->animTimer > 0)
        {
            ftr->animTimer--;
        }
        else
        {
            // When it elapses, reset it to 0.2s
            ftr->animTimer = 200 / FRAME_TIME_MS;
            // And switch the running sprite
            if(ftr->currentSprite == ftr->runSprite1)
            {
                ftr->currentSprite = ftr->runSprite0;
            }
            else
            {
                ftr->currentSprite = ftr->runSprite1;
            }
        }
    }

    // Decrement the short hop timer
    if(ftr->shortHopTimer > 0)
    {
        ftr->shortHopTimer--;
        // If the timer expires and the button has already been released
        if((0 == ftr->shortHopTimer) && (true == ftr->isShortHop))
        {
            // Short hop by killing velocity
            ftr->velocity.y = 0;
        }
    }

    // Decrement the timer checking double-down-presses to fall through platforms
    if(ftr->fallThroughTimer > 0)
    {
        ftr->fallThroughTimer--;
    }

    // Decrement the timer for state transitions
    bool shouldTransition = false;
    if(ftr->stateTimer > 0)
    {
        ftr->stateTimer--;
        if(0 == ftr->stateTimer)
        {
            shouldTransition = true;
        }
    }

    // If the timer expired and there is a state transition
    if(shouldTransition)
    {
        // Keep a pointer to the current attack frame
        attackFrame_t* atk = NULL;
        // Switch on where we're transitioning from
        switch(ftr->state)
        {
            case FS_STARTUP:
            case FS_ATTACK:
            {
                if(FS_STARTUP == ftr->state)
                {
                    // Transition from ground startup to ground attack
                    ftr->attackFrame = 0;
                }
                else
                {
                    // Transition from one attack frame to the next
                    ftr->attackFrame++;
                }

                // Make sure the attack frame exists
                if(ftr->attackFrame < ftr->attacks[ftr->cAttack].numAttackFrames)
                {
                    // Transition from one attack frame to the next attack frame
                    atk = &ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame];
                    // Set the sprite
                    setFighterState(ftr, FS_ATTACK, ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].sprite, atk->duration, NULL);

                    // Always copy the iframe value, may be 0
                    ftr->iFrameTimer = atk->iFrames;
                    // Set invincibility if the timer is active
                    ftr->isInvincible = (ftr->iFrameTimer > 0);
                }
                else
                {
                    // Transition from attacking to cooldown
                    atk = NULL;
                    setFighterState(ftr, FS_COOLDOWN, ftr->attacks[ftr->cAttack].endLagSprite, ftr->attacks[ftr->cAttack].endLag, NULL);
                }
                break;
            }
            case FS_COOLDOWN:
            case FS_HITSTUN:
            {
                // Transition from cooldown to idle
                if(ftr->isInAir)
                {
                    // In air, go to jump
                    setFighterState(ftr, FS_JUMPING, ftr->jumpSprite, 0, NULL);
                }
                else
                {
                    // On ground, go to idle
                    setFighterState(ftr, FS_IDLE, ftr->idleSprite0, 0, NULL);
                }
                break;
            }
            case FS_IDLE:
            case FS_RUNNING:
            case FS_DUCKING:
            case FS_JUMPING:
            {
                // These states have no transitions, yet
                break;
            }
        }

        // If this is an attack, check for velocity changes and projectiles
        if(NULL != atk)
        {
            // Apply any velocity from this attack to the fighter
            if(0 != atk->velocity.x)
            {
                if(FACING_RIGHT == ftr->dir)
                {
                    ftr->velocity.x = atk->velocity.x;
                }
                else
                {
                    ftr->velocity.x = -atk->velocity.x;
                }
            }
            if(0 != atk->velocity.y)
            {
                ftr->velocity.y = atk->velocity.y;
            }

            // Resize the hurtbox if there's a custom one
            if(0 != atk->hurtbox_size.x && 0 != atk->hurtbox_size.y)
            {
                ftr->size.x = atk->hurtbox_size.x << SF;
                ftr->size.y = atk->hurtbox_size.y << SF;
            }
            else
            {
                ftr->size = ftr->originalSize;
            }

            // Offset the hurtbox. Does nothing if the offset is 0
            ftr->hurtbox_offset.x = atk->hurtbox_offset.x << SF;
            ftr->hurtbox_offset.y = atk->hurtbox_offset.y << SF;

            // Check hitboxes for projectiles. Allocate and link any that are
            for(uint8_t hbIdx = 0; hbIdx < atk->numHitboxes; hbIdx++)
            {
                attackHitbox_t* hbx = &atk->hitboxes[hbIdx];

                if(hbx->isProjectile)
                {
                    // Allocate the projectile
                    projectile_t* proj = malloc(sizeof(projectile_t));

                    // Copy data from the attack frame to the projectile
                    proj->sprite = hbx->projSprite;
                    proj->size   = hbx->hitboxSize;
                    proj->velo   = hbx->projVelo;
                    proj->accel  = hbx->projAccel;
                    // Adjust position, velocity, and acceleration depending on direction
                    if(FACING_RIGHT == ftr->dir)
                    {
                        proj->pos.x = ftr->pos.x + hbx->hitboxPos.x;
                    }
                    else
                    {
                        // Reverse
                        proj->pos.x = ftr->pos.x + ftr->size.x - hbx->hitboxPos.x - proj->size.x;
                        proj->velo.x = -proj->velo.x;
                        proj->accel.x = -proj->accel.x;
                    }
                    proj->dir             = ftr->dir;
                    proj->pos.y           = ftr->pos.y + hbx->hitboxPos.y;
                    proj->duration        = hbx->projDuration;
                    proj->knockback       = hbx->knockback;
                    proj->damage          = hbx->damage;
                    proj->hitstun         = hbx->hitstun;
                    proj->removeNextFrame = false;
                    proj->owner           = ftr;

                    // Add the projectile to the list
                    push(&f->projectiles, proj);
                }
            }
        }
        else
        {
            // No attack, so use the normal size
            ftr->size = ftr->originalSize;
            ftr->hurtbox_offset.x = 0;
            ftr->hurtbox_offset.y = 0;
        }
    }
}

/**
 * Check button inputs asynchronously for the given fighter
 *
 * @param ftr The fighter to check buttons for
 */
void checkFighterButtonInput(fighter_t* ftr)
{
    // Manage ducking, only happens on the ground
    if(!ftr->isInAir)
    {
        if ((FS_IDLE == ftr->state) && (ftr->btnState & DOWN))
        {
            setFighterState(ftr, FS_DUCKING, ftr->duckSprite, 0, NULL);
        }
        else if((FS_DUCKING == ftr->state) && !(ftr->btnState & DOWN))
        {
            setFighterState(ftr, FS_IDLE, ftr->idleSprite0, 0, NULL);
        }
    }

    // Manage the A button
    if (!(ftr->prevBtnState & BTN_A) && (ftr->btnState & BTN_A))
    {
        switch(ftr->state)
        {
            case FS_IDLE:
            case FS_RUNNING:
            case FS_JUMPING:
            {
                // Pressing A in these states means jump
                if(ftr->numJumpsLeft > 0)
                {
                    // Only set short hop timer on the first jump
                    if(ftr->numJumps == ftr->numJumpsLeft)
                    {
                        ftr->shortHopTimer = 125 / FRAME_TIME_MS;
                        ftr->isShortHop = false;
                    }
                    ftr->numJumpsLeft--;
                    ftr->velocity.y = ftr->jump_velo;
                    // Only update relative pos if not already passing through a platform
                    if(PASSING_THROUGH_PLATFORM != ftr->relativePos)
                    {
                        setFighterRelPos(ftr, NOT_TOUCHING_PLATFORM, NULL, NULL, true);
                    }
                    setFighterState(ftr, FS_JUMPING, ftr->jumpSprite, 0, NULL);
                }
                break;
            }
            case FS_DUCKING:
            {
                // Pressing A in this state means fall through platform
                if((!ftr->isInAir) && ftr->touchingPlatform->canFallThrough)
                {
                    // Fall through a platform
                    setFighterRelPos(ftr, PASSING_THROUGH_PLATFORM, NULL, ftr->touchingPlatform, true);
                    setFighterState(ftr, FS_JUMPING, ftr->jumpSprite, 0, NULL);
                    ftr->fallThroughTimer = 0;
                }
                break;
            }
            case FS_STARTUP:
            case FS_ATTACK:
            case FS_COOLDOWN:
            case FS_HITSTUN:
            {
                // Do nothing in these states
                break;
            }
        }
    }

    // Releasing A when the short hop timer is active will do a short hop
    if((ftr->shortHopTimer > 0) &&
            (ftr->prevBtnState & BTN_A) && !(ftr->btnState & BTN_A))
    {
        // Set this boolean, but don't stop the timer! The short hop will peak
        // when the timer expires, at a nice consistent height
        ftr->isShortHop = true;
    }

    // Pressing B means attack, when not in the freefall state
    if (!ftr->isInFreefall && !(ftr->prevBtnState & BTN_B) && (ftr->btnState & BTN_B))
    {
        switch(ftr->state)
        {
            case FS_IDLE:
            case FS_RUNNING:
            case FS_DUCKING:
            case FS_JUMPING:
            {
                // Save last state
                attackOrder_t prevAttack = ftr->cAttack;

                // Check if it's a ground or air attack
                if(!ftr->isInAir)
                {
                    // Attack on ground
                    ftr->isAerialAttack = false;

                    if(ftr->btnState & UP)
                    {
                        // Up tilt attack
                        ftr->cAttack = UP_GROUND;
                    }
                    else if(ftr->btnState & DOWN)
                    {
                        // Down tilt attack
                        ftr->cAttack = DOWN_GROUND;
                    }
                    else if((ftr->btnState & LEFT) || (ftr->btnState & RIGHT))
                    {
                        // Side attack
                        ftr->cAttack = DASH_GROUND;
                    }
                    else
                    {
                        // Neutral attack
                        ftr->cAttack = NEUTRAL_GROUND;
                    }
                }
                else
                {
                    // Attack in air
                    ftr->isAerialAttack = true;

                    if(ftr->btnState & UP)
                    {
                        // Up air attack
                        ftr->cAttack = UP_AIR;
                        // No more jumps after up air!
                        ftr->numJumpsLeft = 0;
                        ftr->isInFreefall = true;
                    }
                    else if(ftr->btnState & DOWN)
                    {
                        // Down air
                        ftr->cAttack = DOWN_AIR;
                    }
                    else if(((ftr->btnState & LEFT ) && (FACING_RIGHT == ftr->dir)) ||
                            ((ftr->btnState & RIGHT) && (FACING_LEFT  == ftr->dir)))
                    {
                        // Back air
                        ftr->cAttack = BACK_AIR;
                    }
                    else if(((ftr->btnState & RIGHT) && (FACING_RIGHT == ftr->dir)) ||
                            ((ftr->btnState & LEFT ) && (FACING_LEFT  == ftr->dir)))
                    {
                        // Front air
                        ftr->cAttack = FRONT_AIR;
                    }
                    else
                    {
                        // Neutral air
                        ftr->cAttack = NEUTRAL_AIR;
                    }
                }

                // If an attack is starting up
                if(prevAttack != ftr->cAttack)
                {
                    // Set the state, sprite, and timer
                    setFighterState(ftr, FS_STARTUP, ftr->attacks[ftr->cAttack].startupLagSprite, ftr->attacks[ftr->cAttack].startupLag,
                                    NULL);

                    // Always copy the iframe value, may be 0
                    ftr->iFrameTimer = ftr->attacks[ftr->cAttack].iFrames;
                    // Set invincibility if the timer is active
                    ftr->isInvincible = (ftr->iFrameTimer > 0);
                }
                break;
            }
            case FS_STARTUP:
            case FS_ATTACK:
            case FS_COOLDOWN:
            case FS_HITSTUN:
            {
                // Don't allow attacks in these states
                break;
            }
        }
    }

    switch(ftr->state)
    {
        case FS_IDLE:
        case FS_RUNNING:
        case FS_DUCKING:
        {
            // Double tapping down will fall through platforms, if the platform allows it
            if((!ftr->isInAir) && ftr->touchingPlatform->canFallThrough)
            {
                // Check if down button was released
                if ((ftr->prevBtnState & DOWN) && !(ftr->btnState & DOWN))
                {
                    // Start timer to check for second press
                    ftr->fallThroughTimer = 250 / FRAME_TIME_MS; // 250ms
                }
                // Check if a second down press was detected while the timer is active
                else if (ftr->fallThroughTimer > 0 && !(ftr->prevBtnState & DOWN) && (ftr->btnState & DOWN))
                {
                    // Second press detected fast enough
                    ftr->fallThroughTimer = 0;
                    // Fall through a platform
                    setFighterRelPos(ftr, PASSING_THROUGH_PLATFORM, NULL, ftr->touchingPlatform, true);
                    setFighterState(ftr, FS_JUMPING, ftr->jumpSprite, 0, NULL);
                }
            }
            else
            {
                // Not on a platform, kill the timer
                ftr->fallThroughTimer = 0;
            }
            break;
        }
        case FS_JUMPING:
        case FS_STARTUP:
        case FS_ATTACK:
        case FS_COOLDOWN:
        case FS_HITSTUN:
        {
            // Falling through platforms not allowed in these states
            ftr->fallThroughTimer = 0;
            break;
        }
    }

    // Save the button state for the next frame comparison
    ftr->prevBtnState = ftr->btnState;
}

/**
 * Move the fighter based on current button input and position. The algorithm is
 * as follows:
 *  - Find where the fighter should move to in this frame
 *  - Check for collisions with platforms. If none are found, move there
 *  - If a collision is found, do a binary search on a line between where the
 *    fighter is and where they're trying to move and move as far as it can
 *
 * @param f            The fighter to move
 * @param platforms    A pointer to platforms to check for collisions
 * @param numPlatforms The number of platforms
 * @return true if the round is over, false if it is not
 */
bool updateFighterPosition(fighter_t* ftr, const platform_t* platforms,
                           uint8_t numPlatforms)
{
    // Initial velocity before this frame's calculations
    vector_t v0 = ftr->velocity;

    // Save initial state for potential rollback
    vector_t origPos = ftr->pos;
    platformPos_t origRelPos = ftr->relativePos;
    const platform_t* origTouchingPlatform = ftr->touchingPlatform;
    const platform_t* origPassingThroughPlatform = ftr->passingThroughPlatform;
    bool origIsInair = ftr->isInAir;

    // Only allow movement in these states
    bool movementInput = false;
    switch(ftr->state)
    {
        case FS_IDLE:
        case FS_JUMPING:
        case FS_RUNNING:
        {
            // Update X kinematics based on button state and fighter position
            if(ftr->btnState & LEFT)
            {
                movementInput = true;
                // Left button is currently held
                // Change direction if standing on a platform
                if(!ftr->isInAir)
                {
                    ftr->dir = FACING_LEFT;
                    if(FS_RUNNING != ftr->state)
                    {
                        setFighterState(ftr, FS_RUNNING, ftr->runSprite0, 0, NULL);
                        ftr->animTimer = 200 / FRAME_TIME_MS;
                    }
                }

                // Move left if not up against a wall
                if(RIGHT_OF_PLATFORM != ftr->relativePos)
                {
                    int32_t accel = ftr->run_accel;
                    // Half the acceleration in the air
                    if(ftr->isInAir)
                    {
                        accel >>= 1;
                    }
                    // Accelerate towards the left
                    ftr->velocity.x = v0.x - ((accel * FRAME_TIME_MS) >> SF);
                    // Cap the velocity
                    if (ftr->velocity.x < -ftr->run_max_velo)
                    {
                        ftr->velocity.x = -ftr->run_max_velo;
                    }
                }
                else
                {
                    // Up against a a wall, so stop
                    ftr->velocity.x = 0;
                }
            }
            else if(ftr->btnState & RIGHT)
            {
                movementInput = true;
                // Right button is currently held
                // Change direction if standing on a platform
                if(!ftr->isInAir)
                {
                    ftr->dir = FACING_RIGHT;
                    if(FS_RUNNING != ftr->state)
                    {
                        setFighterState(ftr, FS_RUNNING, ftr->runSprite0, 0, NULL);
                        ftr->animTimer = 200 / FRAME_TIME_MS;
                    }
                }

                // Move right if not up against a wall
                if(LEFT_OF_PLATFORM != ftr->relativePos)
                {
                    int32_t accel = ftr->run_accel;
                    // Half the acceleration in the air
                    if(ftr->isInAir)
                    {
                        accel >>= 1;
                    }

                    // Accelerate towards the right
                    ftr->velocity.x = v0.x + ((accel * FRAME_TIME_MS) >> SF);
                    // Cap the velocity
                    if(ftr->velocity.x > ftr->run_max_velo)
                    {
                        ftr->velocity.x = ftr->run_max_velo;
                    }
                }
                else
                {
                    // Up against a a wall, so stop
                    ftr->velocity.x = 0;
                }
            }
            break;
        }
        case FS_STARTUP:
        case FS_ATTACK:
        case FS_COOLDOWN:
        case FS_HITSTUN:
        {
            // DI in the air in these states
            if(ftr->isInAir)
            {
                if(ftr->btnState & LEFT)
                {
                    movementInput = true;
                    // Accelerate towards the left
                    ftr->velocity.x = v0.x - (((ftr->run_accel / 4) * FRAME_TIME_MS) >> SF);
                    // Cap the velocity
                    if (ftr->velocity.x < -ftr->run_max_velo)
                    {
                        ftr->velocity.x = -ftr->run_max_velo;
                    }
                }
                else if (ftr->btnState & RIGHT)
                {
                    movementInput = true;
                    // Accelerate towards the right
                    ftr->velocity.x = v0.x + (((ftr->run_accel / 4) * FRAME_TIME_MS) >> SF);
                    // Cap the velocity
                    if(ftr->velocity.x > ftr->run_max_velo)
                    {
                        ftr->velocity.x = ftr->run_max_velo;
                    }
                }
            }
            break;
        }
        case FS_DUCKING:
        {
            // Left & right button input ignored in these states
            break;
        }
    }

    // If there was no movement input
    if(false == movementInput)
    {
        // If standing running on a platform, and not moving anymore
        if((FS_RUNNING == ftr->state) && (!ftr->isInAir))
        {
            // Return to idle
            setFighterState(ftr, FS_IDLE, ftr->idleSprite0, 0, NULL);
        }

        // Decelerate less in the air
        int32_t decel = ftr->run_decel;
        if(ftr->isInAir)
        {
            if(SANDBAG == ftr->character)
            {
                // Sandbag doesn't decel in the air, only on the ground
                decel = 0;
            }
            else
            {
                decel >>= 1;
            }
        }

        // Neither left nor right buttons are being pressed. Slow down!
        if(ftr->velocity.x > 0)
        {
            // Decelrate towards the left
            ftr->velocity.x = v0.x - ((decel * FRAME_TIME_MS) >> SF);
            // Check if stopped
            if (ftr->velocity.x < 0)
            {
                ftr->velocity.x = 0;
            }
        }
        else if(ftr->velocity.x < 0)
        {
            // Decelerate towards the right
            ftr->velocity.x = v0.x + ((decel * FRAME_TIME_MS) >> SF);
            // Check if stopped
            if(ftr->velocity.x > 0)
            {
                ftr->velocity.x = 0;
            }
        }
    }

    // The hitbox where the fighter will travel to
    box_t dest_hurtbox;
    getHurtbox(ftr, &dest_hurtbox);

    // If this is the home run contest
    if(HR_CONTEST == f->type && (HR_BARRIER_UP == f->gamePhase || COUNTING_IN == f->gamePhase))
    {
        // Before calculating the destination hitbox
        // If the destination is past a magic bouncy barrier
        if((dest_hurtbox.x0 < hrStadium.platforms[0].area.x0 && ftr->velocity.x < 0) ||
                (dest_hurtbox.x1 > hrStadium.platforms[0].area.x1 && ftr->velocity.x > 0))
        {
            // Reverse direction at 3/4 speed
            ftr->velocity.x = -((3 * ftr->velocity.x) >> 2);
        }
    }

    // Now that we have X velocity, find the new X position
    int32_t deltaX = (((ftr->velocity.x + v0.x) * FRAME_TIME_MS) >> (SF + 1));
    dest_hurtbox.x0 += deltaX;
    dest_hurtbox.x1 += deltaX;

    // Update Y kinematics based on fighter position
    if(ftr->isInAir)
    {
        // Fighter is in the air, so there will be a new Y
        ftr->velocity.y = v0.y + ((ftr->gravity * FRAME_TIME_MS) >> SF);
        // Terminal velocity, arbitrarily chosen. Maybe make this a character attribute?
        if(ftr->velocity.y > 60 << SF)
        {
            ftr->velocity.y = 60 << SF;
        }
        // Now that we have Y velocity, find the new Y position
        int32_t deltaY = (((ftr->velocity.y + v0.y) * FRAME_TIME_MS) >> (SF + 1));
        dest_hurtbox.y0 += deltaY;
        dest_hurtbox.y1 += deltaY;
    }
    else
    {
        // If the fighter is on the ground, check if they get bounced up
        if(BOUNCE_UP == ftr->bounceNextCollision)
        {
            ftr->bounceNextCollision = NO_BOUNCE;
            // Bounce up
            ftr->velocity.y = (-ftr->velocity.y);
        }
        else
        {
            ftr->velocity.y = 0;
        }
    }

    // Do a quick check to see if the binary search can be avoided altogether
    bool collisionDetected = false;
    bool intersectionDetected = false;
    for (uint8_t idx = 0; idx < numPlatforms; idx++)
    {
        if(boxesCollide(dest_hurtbox, platforms[idx].area, SF))
        {
            // dest_hurtbox intersects with a platform, but it may be passing
            // through, like jumping up through one
            intersectionDetected = true;
            if(ftr->passingThroughPlatform != &platforms[idx])
            {
                // dest_hurtbox collides with a platform that it shouldn't
                collisionDetected = true;
                break;
            }
        }
    }

    // If there are no intersections, then the fighter isn't passing through
    // a platform anymore
    if(!intersectionDetected && (PASSING_THROUGH_PLATFORM == ftr->relativePos))
    {
        // Clear the reference to the platform being passed through
        setFighterRelPos(ftr, NOT_TOUCHING_PLATFORM, NULL, NULL, true);
    }

    // If no collisions were detected at the final position
    if(false == collisionDetected)
    {
        // Just move it and be done
        if(FACING_RIGHT == ftr->dir)
        {
            ftr->pos.x = dest_hurtbox.x0 - ftr->hurtbox_offset.x;
        }
        else
        {
            ftr->pos.x = dest_hurtbox.x1 - ftr->originalSize.x + ftr->hurtbox_offset.x;
        }
        ftr->pos.y = dest_hurtbox.y0 - ftr->hurtbox_offset.y;
    }
    else
    {
        // A collision at the destination was detected. Do a binary search to
        // find how close the fighter can get to the destination as possible.
        // Save the starting position
        box_t src_hurtbox;
        getHurtbox(ftr, &src_hurtbox);

        // Start the binary search at the midpoint between src and dest
        box_t test_hurtbox;
        test_hurtbox.x0 = (src_hurtbox.x0 + dest_hurtbox.x0) / 2;
        test_hurtbox.y0 = (src_hurtbox.y0 + dest_hurtbox.y0) / 2;
        test_hurtbox.x1 = test_hurtbox.x0 + ftr->size.x;
        test_hurtbox.y1 = test_hurtbox.y0 + ftr->size.y;

        // Binary search between where the fighter is and where the fighter
        // wants to be until it converges
        while(true)
        {
            // Check if there are any collisions at this position
            collisionDetected = false;
            for (uint8_t idx = 0; idx < numPlatforms; idx++)
            {
                if(ftr->passingThroughPlatform != &platforms[idx] &&
                        boxesCollide(test_hurtbox, platforms[idx].area, SF))
                {
                    // Collision is dettected, stop looping!
                    collisionDetected = true;
                    break;
                }
            }

            if(collisionDetected)
            {
                // If there was a collision, move towards the src by setting
                // the new dest to be what the test point was
                memcpy(&dest_hurtbox, &test_hurtbox, sizeof(box_t));
            }
            else
            {
                // If there wasn't a collision, move towards the dest by setting
                // the new src to be what the test point was
                memcpy(&src_hurtbox, &test_hurtbox, sizeof(box_t));

                // No collision here, so move the fighter here for now
                if(FACING_RIGHT == ftr->dir)
                {
                    ftr->pos.x = test_hurtbox.x0 - ftr->hurtbox_offset.x;
                }
                else
                {
                    ftr->pos.x = test_hurtbox.x1 - ftr->originalSize.x + ftr->hurtbox_offset.x;
                }
                ftr->pos.y = test_hurtbox.y0 - ftr->hurtbox_offset.y;
            }

            // Recalculate the new test point
            test_hurtbox.x0 = (src_hurtbox.x0 + dest_hurtbox.x0) / 2;
            test_hurtbox.y0 = (src_hurtbox.y0 + dest_hurtbox.y0) / 2;
            test_hurtbox.x1 = test_hurtbox.x0 + ftr->size.x;
            test_hurtbox.y1 = test_hurtbox.y0 + ftr->size.y;

            // Check for convergence
            if(((test_hurtbox.x0 == src_hurtbox.x0) || (test_hurtbox.x0 == dest_hurtbox.x0)) &&
                    ((test_hurtbox.y0 == src_hurtbox.y0) || (test_hurtbox.y0 == dest_hurtbox.y0)))
            {
                break;
            }
        }
    }

    // After the final location was found, check what the fighter is up against
    // If the fighter is touching something to begin with, clear that
    if(ftr->relativePos < NOT_TOUCHING_PLATFORM)
    {
        setFighterRelPos(ftr, NOT_TOUCHING_PLATFORM, NULL, NULL, true);
        // If in the air for any reason (like running off a ledge)
        // only have a max of one jump left
        if(ftr->numJumpsLeft > 1)
        {
            ftr->numJumpsLeft = 1;
        }
    }

    // Get the final hurtbox
    box_t hbox;
    getHurtbox(ftr, &hbox);

    // Loop through all platforms to check for touching
    for (uint8_t idx = 0; idx < numPlatforms; idx++)
    {
        // Don't check the platform being passed throughd
        if(ftr->passingThroughPlatform == &platforms[idx])
        {
            continue;
        }

        // If the fighter is above or below a platform
        if(((hbox.x0 >> SF) < (platforms[idx].area.x1 >> SF)) &&
                ((hbox.x1 >> SF) > (platforms[idx].area.x0 >> SF)))
        {
            // If the fighter is moving downward or not at all and hit a platform
            if ((ftr->velocity.y >= 0) &&
                    (((hbox.y1 >> SF)) == (platforms[idx].area.y0 >> SF)))
            {
                // Fighter standing on platform
                ftr->ledgeJumped = false;
                if(BOUNCE_UP == ftr->bounceNextCollision)
                {
                    // Bounce up at half velocity
                    ftr->velocity.y = (-ftr->velocity.y);
                    ftr->bounceNextCollision = NO_BOUNCE;
                }
                else
                {
                    ftr->velocity.y = 0;
                    setFighterRelPos(ftr, ABOVE_PLATFORM, &platforms[idx], NULL, false);
                }
                ftr->numJumpsLeft = ftr->numJumps;
                ftr->isInFreefall = false;
                // If the fighter was jumping, land
                switch(ftr->state)
                {
                    case FS_JUMPING:
                    {
                        // Apply normal landing lag
                        setFighterState(ftr, FS_COOLDOWN, ftr->landingLagSprite, ftr->landingLag, NULL);
                        break;
                    }
                    case FS_STARTUP:
                    case FS_ATTACK:
                    {
                        // If this is an aerial attack while landing
                        if(ftr->isAerialAttack)
                        {
                            // Apply attack landing lag
                            setFighterState(ftr, FS_COOLDOWN, ftr->landingLagSprite, ftr->attacks[ftr->cAttack].landingLag, NULL);
                        }
                        break;
                    }
                    case FS_IDLE:
                    case FS_RUNNING:
                    case FS_DUCKING:
                    case FS_COOLDOWN:
                    case FS_HITSTUN:
                    {
                        // Do nothing (stick with the normal attack cooldown)
                        break;
                    }
                }
                break;
            }
            // If the fighter is moving upward and hit a platform
            else if ((ftr->velocity.y <= 0) &&
                     ((hbox.y0 >> SF) == ((platforms[idx].area.y1 >> SF))))
            {
                if((true == platforms[idx].canFallThrough))
                {
                    // Jump up through the platform
                    setFighterRelPos(ftr, PASSING_THROUGH_PLATFORM, NULL, &platforms[idx], true);
                }
                else
                {
                    // Fighter jumped into the bottom of a solid platform
                    ftr->velocity.y = 0;
                    setFighterRelPos(ftr, BELOW_PLATFORM, &platforms[idx], NULL, true);
                }
                break;
            }
        }

        // If the fighter is to the left or right of a platform
        if(((hbox.y0 >> SF) < (platforms[idx].area.y1 >> SF)) &&
                ((hbox.y1 >> SF) > (platforms[idx].area.y0 >> SF)))
        {
            // If the fighter is moving rightward and hit a wall
            if ((ftr->velocity.x >= 0) &&
                    (((hbox.x1 >> SF)) == (platforms[idx].area.x0 >> SF)))
            {
                if((true == platforms[idx].canFallThrough))
                {
                    // Pass through this platform from the side
                    setFighterRelPos(ftr, PASSING_THROUGH_PLATFORM, NULL, &platforms[idx], true);
                }
                else
                {
                    // Fighter to left of a solid platform
                    if(BOUNCE_RIGHT == ftr->bounceNextCollision)
                    {
                        // Bounce right at half velocity
                        ftr->velocity.x = (-ftr->velocity.x);
                        ftr->bounceNextCollision = NO_BOUNCE;
                    }
                    else
                    {
                        ftr->velocity.x = 0;
                        setFighterRelPos(ftr, LEFT_OF_PLATFORM, &platforms[idx], NULL, true);
                    }

                    // Moving downward
                    if((false == ftr->ledgeJumped) && (ftr->velocity.y > 0))
                    {
                        // Give a bonus 2/3 'jump' to get back on the platform
                        ftr->ledgeJumped = true;
                        ftr->velocity.y = 2 * (ftr->jump_velo / 3);
                    }
                }
                break;
            }
            // If the fighter is moving leftward and hit a wall
            else if ((ftr->velocity.x <= 0) &&
                     ((hbox.x0 >> SF) == ((platforms[idx].area.x1 >> SF))))
            {
                if((true == platforms[idx].canFallThrough))
                {
                    // Pass through this platform from the side
                    setFighterRelPos(ftr, PASSING_THROUGH_PLATFORM, NULL, &platforms[idx], true);
                }
                else
                {
                    // Fighter to right of a solid platform
                    if(BOUNCE_LEFT == ftr->bounceNextCollision)
                    {
                        // Bounce left at half velocity
                        ftr->velocity.x = (-ftr->velocity.x);
                        ftr->bounceNextCollision = NO_BOUNCE;
                    }
                    else
                    {
                        ftr->velocity.x = 0;
                        setFighterRelPos(ftr, RIGHT_OF_PLATFORM, &platforms[idx], NULL, true);
                    }

                    // Moving downward
                    if((false == ftr->ledgeJumped) && (ftr->velocity.y > 0))
                    {
                        // Give a bonus 2/3 'jump' to get back on the platform
                        ftr->ledgeJumped = true;
                        ftr->velocity.y = 2 * (ftr->jump_velo / 3);
                    }
                }
                break;
            }
        }
    }

    // Revert position if dashing and dashed off a platform
    if((DASH_GROUND == ftr->cAttack) && (ftr->relativePos != ABOVE_PLATFORM))
    {
        // Dead stop
        ftr->velocity.x = 0;
        // Revert position
        ftr->pos = origPos;
        setFighterRelPos(ftr, origRelPos, origTouchingPlatform, origPassingThroughPlatform, origIsInair);
    }

    // Check if the sandbag has landed
    if(SANDBAG == ftr->character)
    {
        if(hbox.y1 > (f->d->h << SF))
        {
            // Initialize the result
            fighterShowHrResult(f->fighters[0].character, f->fighters[1].pos, f->fighters[1].velocity,
                                f->fighters[1].gravity, hrStadium.platforms[0].area.x1);
            // Deinit the game
            fighterExitGame();
            // Return after deinit
            return true;
        }
    }
    // Check kill zone for all other characters
    else if(hbox.y0 > (600 << SF))
    {
        // Decrement stocks
        if(ftr->stocks > 1)
        {
            ftr->stocks--;
        }
        else
        {
            ftr->stocks = 0;
            // End game and show result
            fighterShowMpResult(f->gameTimerUs / 1000000,
                                f->fighters[0].character, NUM_STOCKS - f->fighters[1].stocks, f->fighters[0].damageGiven,
                                f->fighters[1].character, NUM_STOCKS - f->fighters[0].stocks, f->fighters[1].damageGiven);
            // Deinit the game
            fighterExitGame();
            // Return after deinit
            return true;
        }

        // Respawn by resetting state
        // TODO probably need to reset more
        setFighterRelPos(ftr, NOT_TOUCHING_PLATFORM, NULL, NULL, true);
        ftr->cAttack = NO_ATTACK;
        setFighterState(ftr, FS_IDLE, ftr->idleSprite0, 0, NULL);
        ftr->pos.x = (f->d->w / 2) << SF;
        ftr->pos.y = 0;
        ftr->velocity.x = 0;
        ftr->velocity.y = 0;
        ftr->damage = 0;
        ftr->iFrameTimer = IFRAMES_AFTER_SPAWN;
        ftr->isInvincible = true;
    }
    return false;
}

/**
 * Get the number of hitstop frames for the given amount of damage
 *
 * @param damage The amount of damage dealt
 * @return The number of hitstop frames
 */
inline uint32_t getHitstop(uint16_t damage)
{
    // In ultimate, it's
    // frames == floor(damage * 0.65 + 6)
    // but an ultimate frame is 60fps, and this is 20fps, so this works
    return (((damage * 112) / 512) + 2);
}

/**
 * Check if ftr's hitbox collides with otherFtr's hurtbox. If it does, note that
 * the attack connected and add damage and knockback to otherFtr
 *
 * @param ftr      The fighter that is attacking
 * @param otherFtr The fighter that is being attached
 */
void checkFighterHitboxCollisions(fighter_t* ftr, fighter_t* otherFtr)
{
    /* Can't get hurt if you're invincible! */
    if(otherFtr->isInvincible)
    {
        return;
    }

    box_t otherFtrHurtbox;
    getHurtbox(otherFtr, &otherFtrHurtbox);

    // Check hitbox and hurbox
    if(FS_ATTACK == ftr->state)
    {
        // Get a reference to the attack and frames
        attack_t* atk = &ftr->attacks[ftr->cAttack];
        attackFrame_t* afrm = &atk->attackFrames[ftr->attackFrame];

        // Check for collisions if this frame hasn't connected yet
        // Also make sure that the attack allows multi-frame hits
        if ((false == (atk->onlyFirstHit && atk->attackConnected)) &&
                (false == afrm->attackConnected))
        {
            for(uint8_t hbIdx = 0; hbIdx < afrm->numHitboxes; hbIdx++)
            {
                attackHitbox_t* hbx = &afrm->hitboxes[hbIdx];

                // If this isn't a projectile attack, check the hitbox
                if(!hbx->isProjectile)
                {
                    // Figure out where the hitbox is relative to the fighter
                    box_t relativeHitbox;
                    getHurtbox(ftr, &relativeHitbox);

                    if(FACING_RIGHT == ftr->dir)
                    {
                        relativeHitbox.x0 += hbx->hitboxPos.x;
                        relativeHitbox.x1 = relativeHitbox.x0 + hbx->hitboxSize.x;
                    }
                    else
                    {
                        // reverse the hitbox if dashing and facing left
                        relativeHitbox.x1 = relativeHitbox.x0 + ftr->size.x - hbx->hitboxPos.x;
                        relativeHitbox.x0 = relativeHitbox.x1 - hbx->hitboxSize.x;
                    }
                    relativeHitbox.y0 += hbx->hitboxPos.y;
                    relativeHitbox.y1 = relativeHitbox.y0 + hbx->hitboxSize.y;

                    if(boxesCollide(relativeHitbox, otherFtrHurtbox, SF))
                    {
                        // Note the attack connected so it doesnt collide twice
                        atk->attackConnected = true;
                        afrm->attackConnected = true;

                        // Tally the damage
                        otherFtr->damage += hbx->damage;
                        ftr->damageGiven += hbx->damage;

                        // Set the hitstop timer
                        otherFtr->hitstopTimer = getHitstop(hbx->damage);

                        // Apply the knockback, scaled by damage
                        // roughly (1 + (0.02 * dmg))
                        int32_t knockbackScalar = 64 + (otherFtr->damage);
                        vector_t knockback;
                        if(FACING_RIGHT == ftr->dir)
                        {
                            knockback.x = ((hbx->knockback.x * knockbackScalar) / 64);
                        }
                        else
                        {
                            knockback.x = -((hbx->knockback.x * knockbackScalar) / 64);
                        }
                        knockback.y = ((hbx->knockback.y * knockbackScalar) / 64);
                        otherFtr->velocity.x = knockback.x;
                        otherFtr->velocity.y = knockback.y;

                        // Knock the fighter into the air
                        if(!otherFtr->isInAir)
                        {
                            setFighterRelPos(otherFtr, NOT_TOUCHING_PLATFORM, NULL, NULL, true);
                        }

                        // Apply hitstun, scaled by defendant's percentage
                        setFighterState(otherFtr, FS_HITSTUN,
                                        otherFtr->isInAir ? otherFtr->hitstunAirSprite : otherFtr->hitstunGroundSprite,
                                        hbx->hitstun * (1 + (otherFtr->damage / 32)),
                                        &knockback);

                        // Break out of the for loop so that only one hitbox hits
                        break;
                    }
                }
            }
        }
    }
}

/**
 * Check if any projectile's hitbox collides with either of the fighter's
 * hurtboxes. If it does, despawn the projectile and add damage and knockback to
 * the fighter who was shot.
 *
 * Note, once a projectile is fired, it has no owner and can hit either fighter.
 *
 * @param projectiles A list of projectiles to check
 */
void checkFighterProjectileCollisions(list_t* projectiles)
{
    // Check projectile collisions. Iterate through all projectiles
    node_t* currentNode = projectiles->first;
    while (currentNode != NULL)
    {
        projectile_t* proj = currentNode->val;

        // Don't bother checking if this is gonna be removed
        if(false == proj->removeNextFrame)
        {
            // Create a hurtbox for this projectile to check for collisions with hurtboxes
            box_t projHurtbox =
            {
                .x0 = proj->pos.x,
                .y0 = proj->pos.y,
                .x1 = proj->pos.x + proj->size.x,
                .y1 = proj->pos.y + proj->size.y,
            };

            // For each fighter
            for(uint8_t i = 0; i < NUM_FIGHTERS; i++)
            {
                // Get a convenience pointer
                fighter_t* ftr = &f->fighters[i];

                // Make sure a projectile can't hurt its owner
                if(ftr != proj->owner)
                {
                    box_t ftrHurtbox;
                    getHurtbox(ftr, &ftrHurtbox);
                    // Check if this projectile collided the first fighter
                    if(boxesCollide(projHurtbox, ftrHurtbox, SF))
                    {
                        // Tally the damage
                        ftr->damage += proj->damage;
                        proj->owner->damageGiven += proj->damage;

                        // Apply the knockback, scaled by damage
                        // roughly (1 + (0.02 * dmg))
                        int32_t knockbackScalar = 64 + (ftr->damage);
                        vector_t knockback;
                        if(FACING_RIGHT == proj->dir)
                        {
                            knockback.x = ((proj->knockback.x * knockbackScalar) / 64);
                        }
                        else
                        {
                            knockback.x = -((proj->knockback.x * knockbackScalar) / 64);
                        }
                        knockback.y = ((proj->knockback.y * knockbackScalar) / 64);
                        ftr->velocity.x = knockback.x;
                        ftr->velocity.y = knockback.y;

                        // Knock the fighter into the air
                        if(!ftr->isInAir)
                        {
                            setFighterRelPos(ftr, NOT_TOUCHING_PLATFORM, NULL, NULL, true);
                        }

                        // Apply hitstun, scaled by defendant's percentage
                        setFighterState(ftr, FS_HITSTUN,
                                        ftr->isInAir ? ftr->hitstunAirSprite : ftr->hitstunGroundSprite,
                                        proj->hitstun * (1 + (ftr->damage / 32)),
                                        &knockback);

                        // Mark this projectile for removal
                        proj->removeNextFrame = true;
                    }
                }
            }
        }

        // Iterate to the next projectile
        currentNode = currentNode->next;
    }
}

/**
 * Iterate through all projectiles, checking their timers and collisions.
 * If a projectile collides with a platform or times out, unlink and free it
 *
 * @param projectiles  A linked list of projectiles to process
 * @param platforms    A pointer to platforms to check for collisions
 * @param numPlatforms The number of platforms
 */
void checkProjectileTimer(list_t* projectiles, const platform_t* platforms,
                          uint8_t numPlatforms)
{
    // Iterate through all projectiles
    node_t* currentNode = projectiles->first;
    while (currentNode != NULL)
    {
        projectile_t* proj = currentNode->val;

        // Decrement this projectile's time-to-live
        proj->duration--;

        // If the projectile times out or is marked for removal
        if((0 == proj->duration) || (true == proj->removeNextFrame))
        {
            // Free and remove the projectile, and iterate
            free(proj);
            node_t* toRemove = currentNode;
            currentNode = currentNode->next;
            removeEntry(projectiles, toRemove);
        }
        else
        {
            // Otherwise, update projectile kinematics. Acceleration is constant
            vector_t v0 = proj->velo;

            // Update velocity
            proj->velo.x = proj->velo.x + ((proj->accel.x * FRAME_TIME_MS) >> SF);
            proj->velo.y = proj->velo.y + ((proj->accel.y * FRAME_TIME_MS) >> SF);

            // Update the position
            proj->pos.x = proj->pos.x + (((proj->velo.x + v0.x) * FRAME_TIME_MS) >> (SF + 1));
            proj->pos.y = proj->pos.y + (((proj->velo.y + v0.y) * FRAME_TIME_MS) >> (SF + 1));

            // Create a hurtbox for this projectile to check for collisions with platforms
            box_t projHurtbox =
            {
                .x0 = proj->pos.x,
                .y0 = proj->pos.y,
                .x1 = proj->pos.x + proj->size.x,
                .y1 = proj->pos.y + proj->size.y,
            };

            // Check if this projectile collided with a platform
            for (uint8_t idx = 0; idx < numPlatforms; idx++)
            {
                if(boxesCollide(projHurtbox, platforms[idx].area, SF))
                {
                    // Draw one more frame, then remove the projectile
                    proj->removeNextFrame = true;
                    break;
                }
            }

            // Iterate to the next projectile
            currentNode = currentNode->next;
        }
    }
}

/**
 * @brief Get the position to draw a fighter's sprite at
 *
 * @param ftr The fighter to draw a sprite for
 * @param spritePos The position of the sprite
 */
void getSpritePos(fighter_t* ftr, vector_t* spritePos)
{
    wsg_t* currentSprite = getFighterSprite(ftr->currentSprite, f->loadedSprites);
    if(FACING_RIGHT == ftr->dir)
    {
        spritePos->x = ftr->pos.x >> SF;
    }
    else
    {
        spritePos->x = ((ftr->pos.x + ftr->originalSize.x) >> SF) - currentSprite->w;
    }
    spritePos->x += ftr->hitstopShake;
    spritePos->y = ftr->pos.y >> SF;

    // If this is an attack frame
    if(FS_ATTACK == ftr->state)
    {
        // Get a reference to the attack frame
        attackFrame_t* atk = &ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame];
        // Shift the sprite
        if(FACING_RIGHT == ftr->dir)
        {
            spritePos->x += atk->sprite_offset.x;
        }
        else
        {
            spritePos->x -= atk->sprite_offset.x;
        }
        spritePos->y += atk->sprite_offset.y;
    }
    else
    {
        // Shift the sprite
        if(FACING_RIGHT == ftr->dir)
        {
            spritePos->x += ftr->sprite_offset.x;
        }
        else
        {
            spritePos->x -= ftr->sprite_offset.x;
        }
        spritePos->y += ftr->sprite_offset.y;
    }
}

/**
 * Compose the current scene into a series of instructions to either render or
 * send to another Swadge
 *
 * @param stageIdx The index of the stage being fought on
 * @param f1 One fighter to compose
 * @param f2 The other fighter to compose
 * @param projectiles A list of projectiles to compose
 * @param outLen The length of the composed scene (output)
 * @return int16_t* An array with the composed scene. This memory is allocated and must be freed
 */
fighterScene_t* composeFighterScene(uint8_t stageIdx, fighter_t* f1, fighter_t* f2, list_t* projectiles,
                                    uint8_t* outLen)
{
    // Count number of projectiles
    int16_t numProj = 0;
    node_t* currentNode = projectiles->first;
    while (currentNode != NULL)
    {
        numProj++;
        // Iterate
        currentNode = currentNode->next;
    }

    // Allocate array to store the data to render a scene
    (*outLen) = (sizeof(fighterScene_t)) + (numProj * sizeof(fighterSceneProjectile_t));
    fighterScene_t* scene = malloc((*outLen));

    // Message type will go here later
    scene->p2pMsgType = 0;

    // Save Stage IDX
    scene->stageIdx = stageIdx;

    vector_t spritePos;

    // f1 position
    getSpritePos(f1, &spritePos);
    scene->f1.spritePosX = spritePos.x; // Save the final location for the sprite
    scene->f1.spritePosY = spritePos.y;
    scene->f1.spriteDir = f1->dir;
    // f1 sprite
    scene->f1.spriteIdx = f1->currentSprite;
    // f1 damage and stock
    scene->f1.damage = f1->damage;
    scene->f1.stocks = f1->stocks;

    // f2 position
    getSpritePos(f2, &spritePos);
    scene->f2.spritePosX = spritePos.x; // Save the final location for the sprite
    scene->f2.spritePosY = spritePos.y;
    scene->f2.spriteDir = f2->dir;
    // f2 sprite
    scene->f2.spriteIdx = f2->currentSprite;
    // f2 damage and stock
    scene->f2.damage = f2->damage;
    scene->f2.stocks = f2->stocks;

    scene->numProjectiles = numProj;
    // Iterate through all the projectiles
    int16_t cProj = 0;
    currentNode = projectiles->first;
    while (currentNode != NULL)
    {
        projectile_t* proj = currentNode->val;

        scene->projs[cProj].spritePosX = proj->pos.x >> SF;
        scene->projs[cProj].spritePosY = proj->pos.y >> SF;
        scene->projs[cProj].spriteDir = proj->dir;
        scene->projs[cProj].spriteIdx = proj->sprite;

        // Iterate
        cProj++;
        currentNode = currentNode->next;
    }

    return scene;
}

/**
 * Render the current frame to the display, including fighters, platforms, and
 * projectiles, and HUD
 *
 * @param d The display to draw to
 * @param scene The scene to draw
 */
void drawFighterScene(display_t* d, const fighterScene_t* scene)
{
    // Read from scene
    uint8_t stageIdx = scene->stageIdx;

    // Draw the specified stage
    const stage_t* stage = stages[stageIdx];
    for (uint8_t idx = 0; idx < stage->numPlatforms; idx++)
    {
        platform_t platform = stage->platforms[idx];
        drawBox(d, platform.area, c555, !platform.canFallThrough, SF);
    }

    // f1 position
    int16_t f1_posX           = scene->f1.spritePosX;
    int16_t f1_posY           = scene->f1.spritePosY;
    fighterDirection_t f1_dir = scene->f1.spriteDir;
    int16_t f1_sprite         = scene->f1.spriteIdx;

    // f1 damage and stock
    int16_t f1_dmg   = scene->f1.damage;
    int16_t f1_stock = scene->f1.stocks;

    // f2 position
    int16_t f2_posX           = scene->f2.spritePosX;
    int16_t f2_posY           = scene->f2.spritePosY;
    fighterDirection_t f2_dir = scene->f2.spriteDir;
    int16_t f2_sprite         = scene->f2.spriteIdx;

    // f2 damage and stock
    int16_t f2_dmg   = scene->f2.damage;
    int16_t f2_stock = scene->f2.stocks;

    // Actually draw fighters
    drawWsg(d, getFighterSprite(f2_sprite, f->loadedSprites), f2_posX, f2_posY, f2_dir, false, 0);
    drawWsg(d, getFighterSprite(f1_sprite, f->loadedSprites), f1_posX, f1_posY, f1_dir, false, 0);

    // Iterate through projectiles
    int16_t numProj = scene->numProjectiles;
    for (int16_t pIdx = 0; pIdx < numProj; pIdx++)
    {
        // Projectile position
        int16_t proj_posX           = scene->projs[pIdx].spritePosX;
        int16_t proj_posY           = scene->projs[pIdx].spritePosY;
        fighterDirection_t proj_dir = scene->projs[pIdx].spriteDir;
        int16_t proj_sprite         = scene->projs[pIdx].spriteIdx;

        // Actually draw projectile
        drawWsg(d, getFighterSprite(proj_sprite, f->loadedSprites), proj_posX, proj_posY, proj_dir, false, 0);
    }

    // If this is the home run contest
    if(HR_CONTEST == f->type && (HR_BARRIER_UP == f->gamePhase || COUNTING_IN == f->gamePhase))
    {
        // Draw some barriers
        plotLine(d,
                 hrStadium.platforms[0].area.x0 >> SF, 0,
                 hrStadium.platforms[0].area.x0 >> SF, (hrStadium.platforms[0].area.y0 >> SF) - 1,
                 c435, 2);

        plotLine(d,
                 (hrStadium.platforms[0].area.x1 >> SF) - 1, 0,
                 (hrStadium.platforms[0].area.x1 >> SF) - 1, (hrStadium.platforms[0].area.y0 >> SF) - 1,
                 c435, 2);
    }

    // Draw the HUD
    drawFighterHud(d, f->mm_font, f1_dmg, f1_stock, f2_dmg, f2_stock);

    // Draw debug boxes, conditionally
#ifdef DRAW_DEBUG_BOXES
    if(f->type != MULTIPLAYER)
    {
        drawFighterDebugBox(d, &(f->fighters[0]));
        drawFighterDebugBox(d, &(f->fighters[1]));
        drawProjectileDebugBox(d, &(f->projectiles));
    }
#endif
}

/**
 * Draw the HUD, which is just the damage percentages and stock circles
 *
 * @param d The display to draw to
 * @param font The font to use for the damage percentages
 * @param f1_dmg Fighter one's damage
 * @param f1_stock Fighter one's stocks
 * @param f2_dmg Fighter two's damage
 * @param f2_stock Fighter two's stocks
 */
void drawFighterHud(display_t* d, font_t* font, int16_t f1_dmg, int16_t f1_stock, int16_t f2_dmg, int16_t f2_stock)
{
    char dmgStr[16];
    uint16_t tWidth;
    uint16_t xPos;

#define SR 5 // Stock radius
    int16_t stockX = (d->w / 3) - (2 * SR) - 3;
    for(uint8_t stockToDraw = 0; stockToDraw < f1_stock; stockToDraw++)
    {
        plotCircleFilled(d, stockX, d->h - font->h - 4 - (SR * 2), SR, c550);
        stockX += ((2 * SR) + 3);
    }

    snprintf(dmgStr, sizeof(dmgStr) - 1, "%d%%", f1_dmg);
    tWidth = textWidth(font, dmgStr);
    xPos = (d->w / 3) - (tWidth / 2);
    drawText(d, font, c555, dmgStr, xPos, d->h - font->h - 2);

    stockX = (2 * (d->w / 3)) - (2 * SR) - 3;
    for(uint8_t stockToDraw = 0; stockToDraw < f2_stock; stockToDraw++)
    {
        plotCircleFilled(d, stockX, d->h - font->h - 4 - (SR * 2), SR, c550);
        stockX += ((2 * SR) + 3);
    }

    snprintf(dmgStr, sizeof(dmgStr) - 1, "%d%%", f2_dmg);
    tWidth = textWidth(font, dmgStr);
    xPos = (2 * (d->w / 3)) - (tWidth / 2);
    drawText(d, font, c555, dmgStr, xPos, d->h - font->h - 2);

    switch(f->gamePhase)
    {
        case COUNTING_IN:
        case HR_BARRIER_UP:
        case HR_BARRIER_DOWN:
        {
            // Draw the timer
            char timeStr[32];
            snprintf(timeStr, sizeof(timeStr) - 1, "%d.%03d", f->gameTimerUs / 1000000, (f->gameTimerUs / 1000) % 1000);
            tWidth = textWidth(font, "9.999"); // If this isn't constant, the time jiggles a lot
            drawText(d, font, c555, timeStr, (d->w - tWidth) / 2, 0);
        }
        case MP_GAME:
        {
            // No timer
            break;
        }
    }

    // Print GO!!! after the COUNTING_IN phase finishes
    if(f->printGoTimerUs > 0)
    {
        char goStr[] = "GO!!!";
        tWidth = textWidth(f->mm_font, goStr);
        drawText(f->d, f->mm_font, c555, goStr, (f->d->w - tWidth) / 2, f->d->h / 3);
    }
}

#ifdef DRAW_DEBUG_BOXES
/**
 * Draw debug hit and hurt boxes around a fighter
 *
 * @param d The display to draw to
 * @param fighter The fighter to draw debug boxes for
 */
void drawFighterDebugBox(display_t* d, fighter_t* ftr)
{
    // Pick the color based on state
    paletteColor_t hitboxColor = c500;
    switch(ftr->state)
    {
        case FS_STARTUP:
        {
            hitboxColor = c502;
            break;
        }
        case FS_ATTACK:
        {
            hitboxColor = c303;
            break;
        }
        case FS_COOLDOWN:
        {
            hitboxColor = c205;
            break;
        }
        case FS_HITSTUN:
        {
            hitboxColor = c040;
            break;
        }
        case FS_IDLE:
        case FS_RUNNING:
        case FS_DUCKING:
        case FS_JUMPING:
        {
            hitboxColor = c500;
            break;
        }
    }

    // Override color if invincible
    if(ftr->isInvincible)
    {
        hitboxColor = c333;
    }

    // Draw an outline
    box_t hurtbox;
    getHurtbox(ftr, &hurtbox);
    drawBox(d, hurtbox, hitboxColor, false, SF);

    // Draw the hitbox if attacking
    if(FS_ATTACK == ftr->state)
    {
        // Get a reference to the attack frame
        attackFrame_t* atk = &ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame];

        // For each hitbox in this frame
        for(uint8_t hbIdx = 0; hbIdx < atk->numHitboxes; hbIdx++)
        {
            attackHitbox_t* hbx = &atk->hitboxes[hbIdx];

            // If this isn't a projectile attack, draw it
            if(!hbx->isProjectile)
            {
                // Figure out where the hitbox is relative to the fighter
                box_t relativeHitbox;
                getHurtbox(ftr, &relativeHitbox);
                if(FACING_RIGHT == ftr->dir)
                {
                    relativeHitbox.x0 += hbx->hitboxPos.x;
                    relativeHitbox.x1 = relativeHitbox.x0 + hbx->hitboxSize.x;
                }
                else
                {
                    // reverse the hitbox if dashing and facing left
                    relativeHitbox.x1 = relativeHitbox.x0 + ftr->size.x - hbx->hitboxPos.x;
                    relativeHitbox.x0 = relativeHitbox.x1 - hbx->hitboxSize.x;
                }
                relativeHitbox.y0 += hbx->hitboxPos.y;
                relativeHitbox.y1 = relativeHitbox.y0 + hbx->hitboxSize.y;
                // Draw the hitbox
                drawBox(d, relativeHitbox, c440, false, SF);
            }
        }
    }
}

/**
 * Draw debug hit and hurt boxes around projectiles
 *
 * @param d The display to draw to
 * @param projectiles a list of projectiles to draw debug boxes around
 */
void drawProjectileDebugBox(display_t* d, list_t* projectiles)
{
    // Iterate through all the projectiles
    node_t* currentNode = projectiles->first;
    while (currentNode != NULL)
    {
        projectile_t* proj = currentNode->val;

        // Draw the projectile box
        box_t projBox =
        {
            .x0 = proj->pos.x,
            .y0 = proj->pos.y,
            .x1 = proj->pos.x + proj->size.x,
            .y1 = proj->pos.y + proj->size.y,
        };
        drawBox(d, projBox, c050, false, SF);

        // Iterate
        currentNode = currentNode->next;
    }
}
#endif /* DRAW_DEBUG_BOXES */

/**
 * Save the button state for processing
 *
 * @param evt The button event to save
 */
void fighterGameButtonCb(buttonEvt_t* evt)
{
    // Ignore button input while counting in
    if(f->gamePhase != COUNTING_IN)
    {
        // Save the state to check synchronously
        f->fighters[f->playerIdx].btnState = evt->state;
    }
}

/**
 * @brief Receive button input from another swadge
 *
 * @param btnState The button state from the other swadge
 */
void fighterRxButtonInput(int32_t btnState)
{
    f->fighters[1].btnState = btnState;
    // Set to true to run main loop with input
    f->buttonInputReceived = true;
    f->shouldSendScene = false;
}

/**
 * @brief Receive a scene to render from another swadge
 *
 * @param scene The scene to render
 */
void fighterRxScene(const fighterScene_t* scene, uint8_t len)
{
    f->shouldSendButtonInput = false;

    // Save scene to be drawn in fighterGameLoop()
    if(NULL != f->composedScene)
    {
        free(f->composedScene);
    }
    f->composedScene = malloc(len);
    f->composedSceneLen = len;
    memcpy(f->composedScene, scene, len);
}

/**
 * This function notifies the game that the backround was drawn
 * and the scene should be drawn over it
 */
void fighterSetDrawScene(void)
{
    f->shouldDrawScene = true;
}
