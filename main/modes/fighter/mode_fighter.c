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

#include "mode_fighter.h"
#include "fighter_json.h"
#include "bresenham.h"
#include "linked_list.h"
#include "led_util.h"

//==============================================================================
// Constants
//==============================================================================

#define FRAME_TIME_MS 25 // 20fps

#define DRAW_DEBUG_BOXES

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    int64_t frameElapsed;
    fighter_t* fighters;
    uint8_t numFighters;
    list_t projectiles;
    list_t loadedSprites;
    display_t* d;
    font_t mm_font;
} fightingGame_t;

//==============================================================================
// Function Prototypes
//==============================================================================

void fighterEnterMode(display_t* disp);
void fighterExitMode(void);
void fighterMainLoop(int64_t elapsedUs);
void fighterButtonCb(buttonEvt_t* evt);

void getHurtbox(fighter_t* ftr, box_t* hurtbox);
#define setFighterState(f, st, sp, tm) _setFighterState(f, st, sp, tm, __LINE__);
void _setFighterState(fighter_t* ftr, fighterState_t newState, wsg_t* newSprite, int32_t timer, uint32_t line);
void setFighterRelPos(fighter_t * ftr, platformPos_t relPos, const platform_t * touchingPlatform,
    const platform_t * passingThroughPlatform, bool isInAir);
void checkFighterButtonInput(fighter_t* ftr);
void updateFighterPosition(fighter_t* f, const platform_t* platforms, uint8_t numPlatforms);
void checkFighterTimer(fighter_t* ftr);
void checkFighterHitboxCollisions(fighter_t* ftr, fighter_t* otherFtr);
void checkFighterProjectileCollisions(list_t* projectiles);
void drawFighter(display_t* d, fighter_t* ftr);

void checkProjectileTimer(list_t* projectiles, const platform_t* platforms,
                          uint8_t numPlatforms);

void drawFighterFrame(display_t* d, const platform_t* platforms,
                      uint8_t numPlatforms);
void drawFighterHud(display_t* d, font_t* font, fighter_t* ftr1, fighter_t* ftr2);

// void fighterAccelerometerCb(accel_t* accel);
// void fighterAudioCb(uint16_t * samples, uint32_t sampleCnt);
// void fighterTemperatureCb(float tmp_c);
// void fighterButtonCb(buttonEvt_t* evt);
// void fighterTouchCb(touch_event_t* evt);
// void fighterEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
// void fighterEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

// void fighterConCbFn(p2pInfo* p2p, connectionEvt_t evt);
// void fighterMsgRxCbFn(p2pInfo* p2p, const char* msg, const char* payload, uint8_t len);
// void fighterMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);

//==============================================================================
// Variables
//==============================================================================

static fightingGame_t* f;

swadgeMode modeFighter =
{
    .modeName = "Fighter",
    .fnEnterMode = fighterEnterMode,
    .fnExitMode = fighterExitMode,
    .fnMainLoop = fighterMainLoop,
    .fnButtonCallback = fighterButtonCb,
    .fnTouchCallback = NULL, // fighterTouchCb,
    .wifiMode = NO_WIFI, // ESP_NOW,
    .fnEspNowRecvCb = NULL, // fighterEspNowRecvCb,
    .fnEspNowSendCb = NULL, // fighterEspNowSendCb,
    .fnAccelerometerCallback = NULL, // fighterAccelerometerCb,
    .fnAudioCallback = NULL, // fighterAudioCb,
    .fnTemperatureCallback = NULL, // fighterTemperatureCb
};

static const platform_t battlefield[] =
{
    {
        .area =
        {
            .x0 = (14) * SF,
            .y0 = (170) * SF,
            .x1 = (14 + 212 - 1) * SF,
            .y1 = (170 + 4) * SF,
        },
        .canFallThrough = false
    },
    {
        .area =
        {
            .x0 = (30) * SF,
            .y0 = (130) * SF,
            .x1 = (30 + 54 - 1) * SF,
            .y1 = (130 + 4) * SF,
        },
        .canFallThrough = true
    },
    {
        .area =
        {
            .x0 = (156) * SF,
            .y0 = (130) * SF,
            .x1 = (156 + 54 - 1) * SF,
            .y1 = (130 + 4) * SF,
        },
        .canFallThrough = true
    },
    {
        .area =
        {
            .x0 = (93) * SF,
            .y0 = (90) * SF,
            .x1 = (93 + 54 - 1) * SF,
            .y1 = (90 + 4) * SF,
        },
        .canFallThrough = true
    }
};

//==============================================================================
// Functions
//==============================================================================

/**
 * Initialize all data needed for the fighter game
 *
 * @param disp The display to draw to
 */
void fighterEnterMode(display_t* disp)
{
    // Allocate base memory for the mode
    f = malloc(sizeof(fightingGame_t));
    memset(f, 0, sizeof(fightingGame_t));

    // Save the display
    f->d = disp;

    // Load a font
    loadFont("mm.font", &f->mm_font);

    // Load fighter data
    f->fighters = loadJsonFighterData(&(f->numFighters), &(f->loadedSprites));
    setFighterRelPos(&(f->fighters[0]), NOT_TOUCHING_PLATFORM, NULL, NULL, true);
    setFighterRelPos(&(f->fighters[1]), NOT_TOUCHING_PLATFORM, NULL, NULL, true);

    f->fighters[0].cAttack = NO_ATTACK;
    f->fighters[1].cAttack = NO_ATTACK;

    // three seconds @ 20fps
    f->fighters[0].iFrameTimer = 60;
    f->fighters[0].isInvincible = true;
    f->fighters[1].iFrameTimer = 60;
    f->fighters[1].isInvincible = true;

    // Set the initial sprites
    setFighterState((&f->fighters[0]), FS_IDLE, f->fighters[0].idleSprite0, 0);
    setFighterState((&f->fighters[1]), FS_IDLE, f->fighters[1].idleSprite0, 0);

    // Start both fighters in the middle of the stage
    f->fighters[0].pos.x = (f->d->w / 2) * SF;
    f->fighters[1].pos.x = (f->d->w / 2) * SF;

    // Start with three stocks
    f->fighters[0].stocks = 3;
    f->fighters[1].stocks = 3;

    // Set some LEDs, just because
    static led_t leds[NUM_LEDS] =
    {
        {.r = 0x00, .g = 0x00, .b = 0x00},
        {.r = 0xFF, .g = 0x00, .b = 0xFF},
        {.r = 0x0C, .g = 0x19, .b = 0x60},
        {.r = 0xFD, .g = 0x08, .b = 0x07},
        {.r = 0x70, .g = 0x81, .b = 0xFF},
        {.r = 0xFF, .g = 0xCC, .b = 0x00},
    };
    setLeds(leds, NUM_LEDS);
}

/**
 * Free all data needed for the fighter game
 */
void fighterExitMode(void)
{
    // Free any stray projectiles
    projectile_t* toFree;
    while (NULL != (toFree = pop(&f->projectiles)))
    {
        free(toFree);
    }

    // Free fighter data
    freeFighterData(f->fighters, f->numFighters);

    // Free sprites
    freeFighterSprites(&(f->loadedSprites));

    // Free font
    freeFont(&f->mm_font);

    // Free game data
    free(f);
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
 * @param line      The line number this was called from, for debugging
 */
void _setFighterState(fighter_t* ftr, fighterState_t newState, wsg_t* newSprite, int32_t timer, uint32_t line)
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
        case FS_HITSTUN:
        {
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

    // debug
    // printf("%d: %d, %p\n", line, newState, newSprite);
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
void setFighterRelPos(fighter_t * ftr, platformPos_t relPos, const platform_t * touchingPlatform,
    const platform_t * passingThroughPlatform, bool isInAir)
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
 * TODO
 *  - Knockback
 *  - Hitstun
 *  - Player stock tracking
 *
 * @param elapsedUs The time elapsed since the last time this was called
 */
void fighterMainLoop(int64_t elapsedUs)
{
    // Keep track of time and only calculate frames every FRAME_TIME_MS
    f->frameElapsed += elapsedUs;
    if (f->frameElapsed > (FRAME_TIME_MS * 1000))
    {
        f->frameElapsed -= (FRAME_TIME_MS * 1000);

        // Check fighter button inputs
        checkFighterButtonInput(&f->fighters[0]);
        checkFighterButtonInput(&f->fighters[1]);

        // Move fighters
        updateFighterPosition(&f->fighters[0], battlefield, sizeof(battlefield) / sizeof(battlefield[0]));
        updateFighterPosition(&f->fighters[1], battlefield, sizeof(battlefield) / sizeof(battlefield[0]));

        // Update timers. This transitions between states and spawns projectiles
        checkFighterTimer(&f->fighters[0]);
        checkFighterTimer(&f->fighters[1]);

        // Update projectile timers. This moves projectiles and despawns if necessary
        checkProjectileTimer(&f->projectiles, battlefield, sizeof(battlefield) / sizeof(battlefield[0]));

        // Check for collisions between hitboxes and hurtboxes
        checkFighterHitboxCollisions(&f->fighters[0], &f->fighters[1]);
        checkFighterHitboxCollisions(&f->fighters[1], &f->fighters[0]);
        // Check for collisions between projectiles and hurtboxes
        checkFighterProjectileCollisions(&f->projectiles);

        // Render the scene
        drawFighterFrame(f->d, battlefield, sizeof(battlefield) / sizeof(battlefield[0]));

        // ESP_LOGI("FTR", "{[%d, %d], [%d, %d], %d}",
        //     f->fighters[0].hurtbox.x0 / SF,
        //     f->fighters[0].hurtbox.y0 / SF,
        //     f->fighters[0].velocity.x,
        //     f->fighters[0].velocity.y,
        //     f->fighters[0].relativePos);
    }
}

/**
 * Draw a fighter to the display. Right now, just draw debugging boxes
 *
 * @param d   The display to draw to
 * @param ftr The fighter to draw
 */
void drawFighter(display_t* d, fighter_t* ftr)
{
#if defined(DRAW_DEBUG_BOXES)
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
#endif

    // Start with the sprite aligned with the hurtbox
    vector_t spritePos;
    if(FACING_RIGHT == ftr->dir)
    {
        spritePos.x = ftr->pos.x / SF;
    }
    else
    {
        spritePos.x = ((ftr->pos.x + ftr->originalSize.x) / SF) - ftr->currentSprite->w;
    }
    spritePos.y = ftr->pos.y / SF;

    // If this is an attack frame
    if(FS_ATTACK == ftr->state)
    {
        // Get a reference to the attack frame
        attackFrame_t* atk = &ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame];
        // Shift the sprite
        spritePos.x += atk->sprite_offset.x;
        spritePos.y += atk->sprite_offset.y;
    }

    // Draw a sprite
    drawWsg(d, ftr->currentSprite,
            spritePos.x, spritePos.y,
            ftr->dir == FACING_LEFT, false, 0);

#if defined(DRAW_DEBUG_BOXES)
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
#endif
}

/**
 * Check all timers for the given fighter.
 * Manage transitons between fighter attacks.
 * Create a projectile if the attack state transitioned to is one.
 *
 * @param ftr The fighter to to check timers for
 */
void checkFighterTimer(fighter_t* ftr)
{
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
                    setFighterState(ftr, FS_ATTACK, ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].sprite, atk->duration);

                    // Always copy the iframe value, may be 0
                    ftr->iFrameTimer = atk->iFrames;
                    // Set invincibility if the timer is active
                    ftr->isInvincible = (ftr->iFrameTimer > 0);
                }
                else
                {
                    // Transition from attacking to cooldown
                    atk = NULL;
                    setFighterState(ftr, FS_COOLDOWN, ftr->attacks[ftr->cAttack].endLagSpr, ftr->attacks[ftr->cAttack].endLag);
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
                    setFighterState(ftr, FS_JUMPING, ftr->jumpSprite, 0);
                }
                else
                {
                    // On ground, go to idle
                    setFighterState(ftr, FS_IDLE, ftr->idleSprite0, 0);
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
                ftr->velocity.x = atk->velocity.x;
            }
            if(0 != atk->velocity.y)
            {
                ftr->velocity.y = atk->velocity.y;
            }

            // Resize the hurtbox if there's a custom one
            if(0 != atk->hurtbox_size.x && 0 != atk->hurtbox_size.y)
            {
                ftr->size.x = atk->hurtbox_size.x * SF;
                ftr->size.y = atk->hurtbox_size.y * SF;
            }
            else
            {
                ftr->size = ftr->originalSize;
            }

            // Offset the hurtbox. Does nothing if the offset is 0
            ftr->hurtbox_offset.x = atk->hurtbox_offset.x * SF;
            ftr->hurtbox_offset.y = atk->hurtbox_offset.y * SF;

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
 * TODO
 *  - Don't allow transitions from all states
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
            setFighterState(ftr, FS_DUCKING, ftr->duckSprite, 0);
        }
        else if((FS_DUCKING == ftr->state) && !(ftr->btnState & DOWN))
        {
            setFighterState(ftr, FS_IDLE, ftr->idleSprite0, 0);
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
                    setFighterState(ftr, FS_JUMPING, ftr->jumpSprite, 0);
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
                    setFighterState(ftr, FS_JUMPING, ftr->jumpSprite, 0);
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

    // Pressing B means attack
    switch(ftr->state)
    {
        case FS_IDLE:
        case FS_RUNNING:
        case FS_DUCKING:
        case FS_JUMPING:
        {
            if (!(ftr->prevBtnState & BTN_B) && (ftr->btnState & BTN_B))
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
                    setFighterState(ftr, FS_STARTUP, ftr->attacks[ftr->cAttack].startupLagSpr, ftr->attacks[ftr->cAttack].startupLag);

                    // Always copy the iframe value, may be 0
                    ftr->iFrameTimer = ftr->attacks[ftr->cAttack].iFrames;
                    // Set invincibility if the timer is active
                    ftr->isInvincible = (ftr->iFrameTimer > 0);
                }
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
                    setFighterState(ftr, FS_JUMPING, ftr->jumpSprite, 0);
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
 */
void updateFighterPosition(fighter_t* ftr, const platform_t* platforms,
                           uint8_t numPlatforms)
{
    // Initial velocity before this frame's calculations
    vector_t v0 = ftr->velocity;

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
                        setFighterState(ftr, FS_RUNNING, ftr->runSprite0, 0);
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
                    ftr->velocity.x = v0.x - (accel * FRAME_TIME_MS) / SF;
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
                        setFighterState(ftr, FS_RUNNING, ftr->runSprite0, 0);
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
                    ftr->velocity.x = v0.x + (accel * FRAME_TIME_MS) / SF;
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
                    ftr->velocity.x = v0.x - ((ftr->run_accel / 4) * FRAME_TIME_MS) / SF;
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
                    ftr->velocity.x = v0.x + ((ftr->run_accel / 4) * FRAME_TIME_MS) / SF;
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
            setFighterState(ftr, FS_IDLE, ftr->idleSprite0, 0);
        }

        // Decelerate less in the air
        int32_t decel = ftr->run_decel;
        if(ftr->isInAir)
        {
            decel >>= 1;
        }

        // Neither left nor right buttons are being pressed. Slow down!
        if(ftr->velocity.x > 0)
        {
            // Decelrate towards the left
            ftr->velocity.x = v0.x - (decel * FRAME_TIME_MS) / SF;
            // Check if stopped
            if (ftr->velocity.x < 0)
            {
                ftr->velocity.x = 0;
            }
        }
        else if(ftr->velocity.x < 0)
        {
            // Decelerate towards the right
            ftr->velocity.x = v0.x + (decel * FRAME_TIME_MS) / SF;
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

    // Now that we have X velocity, find the new X position
    int32_t deltaX = (((ftr->velocity.x + v0.x) * FRAME_TIME_MS) / (SF * 2));
    dest_hurtbox.x0 += deltaX;
    dest_hurtbox.x1 += deltaX;

    // Update Y kinematics based on fighter position
    if(ftr->isInAir)
    {
        // Fighter is in the air, so there will be a new Y
        ftr->velocity.y = v0.y + (ftr->gravity * FRAME_TIME_MS) / SF;
        // Terminal velocity, arbitrarily chosen. Maybe make this a character attribute?
        if(ftr->velocity.y > 60 * SF)
        {
            ftr->velocity.y = 60 * SF;
        }
        // Now that we have Y velocity, find the new Y position
        int32_t deltaY = (((ftr->velocity.y + v0.y) * FRAME_TIME_MS) / (SF * 2));
        dest_hurtbox.y0 += deltaY;
        dest_hurtbox.y1 += deltaY;
    }
    else
    {
        // If the fighter is on the ground, don't bother doing Y axis math
        ftr->velocity.y = 0;
    }

    // Do a quick check to see if the binary search can be avoided altogether
    bool collisionDetected = false;
    bool intersectionDetected = false;
    for (uint8_t idx = 0; idx < numPlatforms; idx++)
    {
        if(boxesCollide(dest_hurtbox, platforms[idx].area))
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
    if(!intersectionDetected)
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
                        boxesCollide(test_hurtbox, platforms[idx].area))
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
        if((hbox.x0 < platforms[idx].area.x1 + SF) &&
                (hbox.x1 + SF > platforms[idx].area.x0))
        {
            // If the fighter is moving downward or not at all and hit a platform
            if ((ftr->velocity.y >= 0) &&
                    (((hbox.y1 / SF)) == (platforms[idx].area.y0 / SF)))
            {
                // Fighter standing on platform
                setFighterRelPos(ftr, ABOVE_PLATFORM, &platforms[idx], NULL, false);
                ftr->ledgeJumped = false;
                ftr->velocity.y = 0;
                ftr->numJumpsLeft = ftr->numJumps;
                // If the fighter was jumping, land
                switch(ftr->state)
                {
                    case FS_JUMPING:
                    {
                        // Apply normal landing lag
                        setFighterState(ftr, FS_COOLDOWN, ftr->landingLagSprite, ftr->landingLag);
                        break;
                    }
                    case FS_STARTUP:
                    case FS_ATTACK:
                    {
                        // If this is an aerial attack while landing
                        if(ftr->isAerialAttack)
                        {
                            // Apply attack landing lag
                            setFighterState(ftr, FS_COOLDOWN, ftr->landingLagSprite, ftr->attacks[ftr->cAttack].landingLag);
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
                     ((hbox.y0 / SF) == ((platforms[idx].area.y1 / SF))))
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
        if((hbox.y0 < platforms[idx].area.y1 + SF) &&
                (hbox.y1 + SF > platforms[idx].area.y0))
        {
            // If the fighter is moving rightward and hit a wall
            if ((ftr->velocity.x >= 0) &&
                    (((hbox.x1 / SF)) == (platforms[idx].area.x0 / SF)))
            {
                if((true == platforms[idx].canFallThrough))
                {
                    // Pass through this platform from the side
                    setFighterRelPos(ftr, PASSING_THROUGH_PLATFORM, NULL, &platforms[idx], true);
                }
                else
                {
                    // Fighter to left of a solid platform
                    ftr->velocity.x = 0;
                    setFighterRelPos(ftr, LEFT_OF_PLATFORM, &platforms[idx], NULL, true);

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
                     ((hbox.x0 / SF) == ((platforms[idx].area.x1 / SF))))
            {
                if((true == platforms[idx].canFallThrough))
                {
                    // Pass through this platform from the side
                    setFighterRelPos(ftr, PASSING_THROUGH_PLATFORM, NULL, &platforms[idx], true);
                }
                else
                {
                    // Fighter to right of a solid platform
                    ftr->velocity.x = 0;
                    setFighterRelPos(ftr, RIGHT_OF_PLATFORM, &platforms[idx], NULL, true);

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

    // Check kill zone
    if(hbox.y0 > SF * 600)
    {
        // Decrement stocks
        if(ftr->stocks > 0)
        {
            ftr->stocks--;
        }
        else
        {
            // TODO end game
        }

        // Respawn by resetting state
        // TODO probably need to reset more
        setFighterRelPos(ftr, NOT_TOUCHING_PLATFORM, NULL, NULL, true);
        ftr->cAttack = NO_ATTACK;
        setFighterState(ftr, FS_IDLE, ftr->idleSprite0, 0);
        ftr->pos.x = (f->d->w / 2) * SF;
        ftr->pos.y = 0;
        ftr->velocity.x = 0;
        ftr->velocity.y = 0;
        ftr->damage = 0;
        ftr->iFrameTimer = 60;
        ftr->isInvincible = true;
    }
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

                    if(boxesCollide(relativeHitbox, otherFtrHurtbox))
                    {
                        // Note the attack connected so it doesnt collide twice
                        atk->attackConnected = true;
                        afrm->attackConnected = true;

                        // Tally the damage
                        otherFtr->damage += hbx->damage;

                        // Apply the knockback, scaled by damage
                        // roughly (1 + (0.02 * dmg))
                        int32_t knockbackScalar = 64 + (otherFtr->damage);
                        if(FACING_RIGHT == ftr->dir)
                        {
                            otherFtr->velocity.x += ((hbx->knockback.x * knockbackScalar) / 64);
                        }
                        else
                        {
                            otherFtr->velocity.x -= ((hbx->knockback.x * knockbackScalar) / 64);
                        }
                        otherFtr->velocity.y += ((hbx->knockback.y * knockbackScalar) / 64);

                        // Apply hitstun, scaled by defendant's percentage
                        setFighterState(otherFtr, FS_HITSTUN, otherFtr->currentSprite, hbx->hitstun * (1 + (otherFtr->damage / 32)));

                        // Knock the fighter into the air
                        if(!otherFtr->isInAir)
                        {
                            setFighterRelPos(otherFtr, NOT_TOUCHING_PLATFORM, NULL, NULL, true);
                        }

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

        // Create a hurtbox for this projectile to check for collisions with hurtboxes
        box_t projHurtbox =
        {
            .x0 = proj->pos.x,
            .y0 = proj->pos.y,
            .x1 = proj->pos.x + proj->size.x,
            .y1 = proj->pos.y + proj->size.y,
        };

        bool removeProjectile = false;

        // For each fighter
        for(uint8_t i = 0; i < f->numFighters; i++)
        {
            // Get a convenience pointer
            fighter_t* ftr = &f->fighters[i];

            // Make sure a projectile can't hurt its owner
            if(ftr != proj->owner)
            {
                box_t ftrHurtbox;
                getHurtbox(ftr, &ftrHurtbox);
                // Check if this projectile collided the first fighter
                if(boxesCollide(projHurtbox, ftrHurtbox))
                {
                    // Tally the damage
                    ftr->damage += proj->damage;

                    // Apply the knockback, scaled by damage
                    // roughly (1 + (0.02 * dmg))
                    int32_t knockbackScalar = 64 + (ftr->damage);
                    if(FACING_RIGHT == proj->dir)
                    {
                        ftr->velocity.x += ((proj->knockback.x * knockbackScalar) / 64);
                    }
                    else
                    {
                        ftr->velocity.x -= ((proj->knockback.x * knockbackScalar) / 64);
                    }
                    ftr->velocity.y += ((proj->knockback.y * knockbackScalar) / 64);

                    // Apply hitstun, scaled by defendant's percentage
                    setFighterState(ftr, FS_HITSTUN, ftr->currentSprite, proj->hitstun * (1 + (ftr->damage / 32)));

                    // Knock the fighter into the air
                    if(!ftr->isInAir)
                    {
                        setFighterRelPos(ftr, NOT_TOUCHING_PLATFORM, NULL, NULL, true);
                    }

                    // Mark this projectile for removal
                    removeProjectile = true;
                }
            }
        }

        // If the projectile collided with a fighter, remove it
        if(removeProjectile)
        {
            // Iterate while removing this projectile
            node_t* nextNode = currentNode->next;
            removeEntry(projectiles, currentNode);
            currentNode = nextNode;
        }
        else
        {
            // Iterate to the next projectile
            currentNode = currentNode->next;
        }
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
            proj->velo.x = proj->velo.x + (proj->accel.x * FRAME_TIME_MS) / SF;
            proj->velo.y = proj->velo.y + (proj->accel.y * FRAME_TIME_MS) / SF;

            // Update the position
            proj->pos.x = proj->pos.x + (((proj->velo.x + v0.x) * FRAME_TIME_MS) / (SF * 2));
            proj->pos.y = proj->pos.y + (((proj->velo.y + v0.y) * FRAME_TIME_MS) / (SF * 2));

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
                if(boxesCollide(projHurtbox, platforms[idx].area))
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
 * Render the current frame to the display, including fighters, platforms, and
 * projectiles, and HUD
 *
 * @param d The display to draw to
 * @param platforms    A pointer to platforms to draw
 * @param numPlatforms The number of platforms
 */
void drawFighterFrame(display_t* d, const platform_t* platforms,
                      uint8_t numPlatforms)
{
    // First clear everything
    d->clearPx();

    // Draw all the platforms
    for (uint8_t idx = 0; idx < numPlatforms; idx++)
    {
        drawBox(d, platforms[idx].area, c555, !platforms[idx].canFallThrough, SF);
    }

    // Draw the fighters
    drawFighter(d, &f->fighters[0]);
    drawFighter(d, &f->fighters[1]);

    // Iterate through all the projectiles
    node_t* currentNode = f->projectiles.first;
    while (currentNode != NULL)
    {
        projectile_t* proj = currentNode->val;

        // Draw the sprite
        drawWsg(d, proj->sprite, proj->pos.x / SF, proj->pos.y / SF,
                FACING_LEFT == proj->dir, false, 0);

#if defined(DRAW_DEBUG_BOXES)
        // Draw the projectile box
        box_t projBox =
        {
            .x0 = proj->pos.x,
            .y0 = proj->pos.y,
            .x1 = proj->pos.x + proj->size.x,
            .y1 = proj->pos.y + proj->size.y,
        };
        drawBox(d, projBox, c050, false, SF);
#endif

        // Iterate
        currentNode = currentNode->next;
    }

    drawFighterHud(d, &f->mm_font, &f->fighters[0], &f->fighters[1]);

    // drawMeleeMenu(d, &f->mm_font);
}

/**
 * Draw the HUD, which is just the damage percentages and stock circles
 *
 * @param d The display to draw to
 * @param font The font to use for the damage percentages
 * @param ftr1 The first fighter to draw damage percent for
 * @param ftr2 The second fighter to draw damage percent for
 */
void drawFighterHud(display_t* d, font_t* font, fighter_t* ftr1, fighter_t* ftr2)
{
    char dmgStr[8];
    uint16_t tWidth;
    uint16_t xPos;

#define SR 5
    int16_t stockX = (d->w / 3) - (2 * SR) - 3;
    for(uint8_t stockToDraw = 0; stockToDraw < ftr1->stocks; stockToDraw++)
    {
        plotCircleFilled(d, stockX, d->h - font->h - 4 - (SR * 2), SR, c550);
        stockX += ((2 * SR) + 3);
    }

    snprintf(dmgStr, sizeof(dmgStr) - 1, "%d%%", ftr1->damage);
    tWidth = textWidth(font, dmgStr);
    xPos = (d->w / 3) - (tWidth / 2);
    drawText(d, font, c555, dmgStr, xPos, d->h - font->h - 2);

    stockX = (2 * (d->w / 3)) - (2 * SR) - 3;
    for(uint8_t stockToDraw = 0; stockToDraw < ftr2->stocks; stockToDraw++)
    {
        plotCircleFilled(d, stockX, d->h - font->h - 4 - (SR * 2), SR, c550);
        stockX += ((2 * SR) + 3);
    }

    snprintf(dmgStr, sizeof(dmgStr) - 1, "%d%%", ftr2->damage);
    tWidth = textWidth(font, dmgStr);
    xPos = (2 * (d->w / 3)) - (tWidth / 2);
    drawText(d, font, c555, dmgStr, xPos, d->h - font->h - 2);
}

/**
 * Save the button state for processing
 *
 * @param evt The button event to save
 */
void fighterButtonCb(buttonEvt_t* evt)
{
    // Save the state to check synchronously
    f->fighters[0].btnState = evt->state;
}
