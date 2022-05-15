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

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    int64_t frameElapsed;
    fighter_t* fighters;
    uint8_t numFighters;
    list_t projectiles;
    display_t* d;
} fightingGame_t;

//==============================================================================
// Function Prototypes
//==============================================================================

void fighterEnterMode(display_t* disp);
void fighterExitMode(void);
void fighterMainLoop(int64_t elapsedUs);
void fighterButtonCb(buttonEvt_t* evt);

void checkFighterButtonInput(fighter_t* ftr);
void updateFighterPosition(fighter_t* f, const platform_t* platforms, uint8_t numPlatforms);
void checkFighterTimer(fighter_t* ftr);
void drawFighter(display_t* d, fighter_t* ftr);

void checkProjectileTimer(list_t* projectiles, const platform_t* platforms,
                          uint8_t numPlatforms);

void drawFighterFrame(display_t* d, const platform_t* platforms,
                      uint8_t numPlatforms);

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

static const platform_t finalDest[] =
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

    // Load fighter data
    f->fighters = loadJsonFighterData(&(f->numFighters));
    f->fighters[0].relativePos = FREE_FLOATING;
    f->fighters[1].relativePos = FREE_FLOATING;

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
    node_t* currentNode = f->projectiles.first;
    while (currentNode != NULL)
    {
        free(currentNode->val);
        node_t* toRemove = currentNode;
        currentNode = currentNode->next;
        removeEntry(&(f->projectiles), toRemove);
    }

    // Free fighter data
    freeFighterData(f->fighters, f->numFighters);

    // Free game data
    free(f);
}

/**
 * Run the main loop for the fighter game. When the time is ready, this will
 * handle button input synchronously, move fighters, check collisions, manage
 * projectiles, and pretty much everything else
 *
 * TODO
 *  - Hitbox collisions
 *  - Damage tracking
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
        updateFighterPosition(&f->fighters[0], finalDest, sizeof(finalDest) / sizeof(finalDest[0]));
        updateFighterPosition(&f->fighters[1], finalDest, sizeof(finalDest) / sizeof(finalDest[0]));

        // Update timers. This transitions between states and spawns projectiles
        checkFighterTimer(&f->fighters[0]);
        checkFighterTimer(&f->fighters[1]);

        // Update projectile timers. This moves projectiles and despawns if necessary
        checkProjectileTimer(&f->projectiles, finalDest, sizeof(finalDest) / sizeof(finalDest[0]));

        // Render the scene
        drawFighterFrame(f->d, finalDest, sizeof(finalDest) / sizeof(finalDest[0]));

        // ESP_LOGI("FGT", "{[%d, %d], [%d, %d], %d}",
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
 * TODO
 *  - Draw sprites instead of (or addition to) debug boxes
 *
 * @param d   The display to draw to
 * @param ftr The fighter to draw
 */
void drawFighter(display_t* d, fighter_t* ftr)
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
        default:
        {
            break;
        }
    }
    // Draw an outline
    drawBox(d, ftr->hurtbox, hitboxColor, false, SF);

    // Fill in half the box to indicate which way the figher is facing
    switch(ftr->dir)
    {
        case FACING_LEFT:
        {
            fillDisplayArea(d,
                            ftr->hurtbox.x0 / SF, ftr->hurtbox.y0 / SF,
                            (ftr->hurtbox.x1 + ftr->hurtbox.x0) / (2 * SF), ftr->hurtbox.y1 / SF,
                            hitboxColor);
            break;
        }
        case FACING_RIGHT:
        {
            fillDisplayArea(d,
                            (ftr->hurtbox.x1 + ftr->hurtbox.x0) / (2 * SF), ftr->hurtbox.y0 / SF,
                            ftr->hurtbox.x1 / SF, ftr->hurtbox.y1 / SF,
                            hitboxColor);
            break;
        }
        default:
        {
            break;
        }
    }

    // Draw the hitbox if attacking
    if(FS_ATTACK == ftr->state)
    {
        // Get a reference to the attack frame
        attackFrame_t* atk = &ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame];

        // If this isn't a projectile attack, draw it
        if(!atk->isProjectile)
        {
            // Figure out where the hitbox is relative to the fighter
            box_t relativeHitbox = ftr->hurtbox;
            if((ftr->dir == FACING_LEFT))
            {
                // reverse the hitbox if dashing and facing left
                relativeHitbox.x1 = relativeHitbox.x0 + ftr->size.x - atk->hitboxPos.x;
                relativeHitbox.x0 = relativeHitbox.x1 - atk->hitboxSize.x;
            }
            else
            {
                relativeHitbox.x0 += atk->hitboxPos.x;
                relativeHitbox.x1 = relativeHitbox.x0 + atk->hitboxSize.x;
            }
            relativeHitbox.y0 += atk->hitboxPos.y;
            relativeHitbox.y1 = relativeHitbox.y0 + atk->hitboxSize.y;
            // Draw the hitbox
            drawBox(d, relativeHitbox, c440, false, SF);
        }
    }
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
            {
                // Transition from ground startup to ground attack
                ftr->state = FS_ATTACK;
                ftr->attackFrame = 0;
                atk = &ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame];
                ftr->stateTimer = atk->duration;
                break;
            }
            case FS_ATTACK:
            {
                ftr->attackFrame++;
                if(ftr->attackFrame < ftr->attacks[ftr->cAttack].numAttackFrames)
                {
                    // Transition from one attack frame to the next attack frame
                    atk = &ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame];
                    ftr->stateTimer = atk->duration;
                }
                else
                {
                    // Transition from attacking to cooldown
                    atk = NULL;
                    ftr->state = FS_COOLDOWN;
                    ftr->stateTimer = ftr->attacks[ftr->cAttack].endLag;
                }
                break;
            }
            case FS_COOLDOWN:
            {
                // Transition from cooldown to idle
                ftr->state = FS_IDLE;
                break;
            }
            case FS_IDLE:
            case FS_RUNNING:
            case FS_DUCKING:
            case FS_JUMP_1:
            case FS_JUMP_2:
            case FS_FALLING:
            case FS_FREEFALL:
            case FS_HITSTUN:
            case FS_HITSTOP:
            case FS_INVINCIBLE:
            default:
            {
                // These states have no transitions, yet
                break;
            }
        }

        // If there is a current attack frame, and the attack is a projectile,
        // allocate a projectile and link it to the list
        if((NULL != atk) && (atk->isProjectile))
        {
            // Allocate the projectile
            projectile_t* proj = malloc(sizeof(projectile_t));

            // Copy data from the attack frame to the projectile
            proj->sprite = atk->projSprite;
            proj->size   = atk->hitboxSize;
            proj->velo   = atk->projVelo;
            proj->accel  = atk->projAccel;
            // Adjust position, velocity, and acceleration depending on direction
            if((ftr->dir == FACING_LEFT))
            {
                // Reverse
                proj->pos.x = ftr->hurtbox.x1 - atk->hitboxPos.x - proj->size.x;
                proj->velo.x = -proj->velo.x;
                proj->accel.x = -proj->accel.x;
            }
            else
            {
                proj->pos.x = ftr->hurtbox.x0 + atk->hitboxPos.x;
            }
            proj->pos.y           = ftr->hurtbox.y0 + atk->hitboxPos.y;
            proj->duration        = atk->projDuration;
            proj->knockbackAng    = atk->knockbackAng;
            proj->knockback       = atk->knockback;
            proj->damage          = atk->damage;
            proj->hitstun         = atk->hitstun;
            proj->removeNextFrame = false;

            // Add the projectile to the list
            push(&f->projectiles, proj);
        }
    }
}

/**
 * Check button inputs asynchronously for the given fighter
 *
 * TODO
 *  - Don't allow transitions from all states
 *  - Short hops
 *
 * @param ftr The fighter to check buttons for
 */
void checkFighterButtonInput(fighter_t* ftr)
{
    // Pressing A means jump
    if (!(ftr->prevBtnState & BTN_A) && (ftr->btnState & BTN_A))
    {
        if(ftr->numJumps > 0)
        {
            ftr->numJumps--;
            ftr->velocity.y = ftr->jump_velo;
            ftr->relativePos = FREE_FLOATING;
            ftr->touchingPlatform = NULL;
        }
    }

    // Pressing B means attack
    if (!(ftr->prevBtnState & BTN_B) && (ftr->btnState & BTN_B))
    {
        // Check if it's a ground or air attack
        if(ABOVE_PLATFORM == ftr->relativePos)
        {
            // Attack on ground
            if(ftr->btnState & UP)
            {
                // Up tilt attack
                ftr->cAttack = UP_GROUND;
                ftr->state = FS_STARTUP;
                ftr->stateTimer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else if(ftr->btnState & DOWN)
            {
                // Down tilt attack
                ftr->cAttack = DOWN_GROUND;
                ftr->state = FS_STARTUP;
                ftr->stateTimer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else if((ftr->btnState & LEFT) || (ftr->btnState & RIGHT))
            {
                // Side attack
                ftr->cAttack = DASH_GROUND;
                ftr->state = FS_STARTUP;
                ftr->stateTimer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else
            {
                // Neutral attack
                ftr->cAttack = NEUTRAL_GROUND;
                ftr->state = FS_STARTUP;
                ftr->stateTimer = ftr->attacks[ftr->cAttack].startupLag;
            }
        }
        else
        {
            // Attack in air
            if(ftr->btnState & UP)
            {
                // Up air attack
                ftr->cAttack = UP_AIR;
                ftr->state = FS_STARTUP;
                ftr->stateTimer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else if(ftr->btnState & DOWN)
            {
                // Down air
                ftr->cAttack = DOWN_AIR;
                ftr->state = FS_STARTUP;
                ftr->stateTimer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else if(((ftr->btnState & LEFT ) && (FACING_RIGHT == ftr->dir)) ||
                    ((ftr->btnState & RIGHT) && (FACING_LEFT  == ftr->dir)))
            {
                // Back air
                ftr->cAttack = BACK_AIR;
                ftr->state = FS_STARTUP;
                ftr->stateTimer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else if(((ftr->btnState & RIGHT) && (FACING_RIGHT == ftr->dir)) ||
                    ((ftr->btnState & LEFT ) && (FACING_LEFT  == ftr->dir)))
            {
                // Front air
                ftr->cAttack = FRONT_AIR;
                ftr->state = FS_STARTUP;
                ftr->stateTimer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else
            {
                // Neutral air
                ftr->cAttack = NEUTRAL_AIR;
                ftr->state = FS_STARTUP;
                ftr->stateTimer = ftr->attacks[ftr->cAttack].startupLag;
            }
        }
    }

    // Double tapping down will fall through platforms, if the platform allows it
    if(ftr->relativePos == ABOVE_PLATFORM && ftr->touchingPlatform->canFallThrough)
    {
        // Check if down button was released
        if ((ftr->prevBtnState & DOWN) && !(ftr->btnState & DOWN))
        {
            // Start timer to check for second press
            ftr->fallThroughTimer = 10; // 10 frames @ 25ms == 250ms
        }
        // Check if a second down press was detected while the timer is active
        else if (ftr->fallThroughTimer > 0 && !(ftr->prevBtnState & DOWN) && (ftr->btnState & DOWN))
        {
            // Second press detected fast enough
            ftr->fallThroughTimer = 0;
            // Fall through a platform
            ftr->relativePos = PASSING_THROUGH_PLATFORM;
            ftr->passingThroughPlatform = ftr->touchingPlatform;
            ftr->touchingPlatform = NULL;
        }
    }
    else
    {
        // Not on a platform, kill the timer
        ftr->fallThroughTimer = 0;
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
    // The hitbox where the fighter will travel to
    box_t dest_hurtbox;
    // Initial velocity before this frame's calculations
    vector_t v0 = ftr->velocity;

    // Update X kinematics based on button state and fighter position
    if(ftr->btnState & LEFT)
    {
        // Left button is currently held
        // Change direction if standing on a platform
        if(ABOVE_PLATFORM == ftr->relativePos)
        {
            ftr->dir = FACING_LEFT;
        }

        // Move left if not up against a wall
        if(RIGHT_OF_PLATFORM != ftr->relativePos)
        {
            // Accelerate towards the left
            ftr->velocity.x = v0.x - (ftr->run_accel * FRAME_TIME_MS) / SF;
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
        // Right button is currently held
        // Change direction if standing on a platform
        if(ABOVE_PLATFORM == ftr->relativePos)
        {
            ftr->dir = FACING_RIGHT;
        }

        // Move right if not up against a wall
        if(LEFT_OF_PLATFORM != ftr->relativePos)
        {
            // Accelerate towards the right
            ftr->velocity.x = v0.x + (ftr->run_accel * FRAME_TIME_MS) / SF;
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
    else
    {
        // Neither left nor right buttons are being pressed. Slow down!
        if(ftr->velocity.x > 0)
        {
            // Decelrate towards the left
            ftr->velocity.x = v0.x - (ftr->run_decel * FRAME_TIME_MS) / SF;
            // Check if stopped
            if (ftr->velocity.x < 0)
            {
                ftr->velocity.x = 0;
            }
        }
        else if(ftr->velocity.x < 0)
        {
            // Decelerate towards the right
            ftr->velocity.x = v0.x + (ftr->run_decel * FRAME_TIME_MS) / SF;
            // Check if stopped
            if(ftr->velocity.x > 0)
            {
                ftr->velocity.x = 0;
            }
        }
    }
    // Now that we have X velocity, find the new X position
    dest_hurtbox.x0 = ftr->hurtbox.x0 + (((ftr->velocity.x + v0.x) * FRAME_TIME_MS) / (SF * 2));

    // Update Y kinematics based on fighter position
    if(ftr->relativePos != ABOVE_PLATFORM)
    {
        // Fighter is in the air, so there will be a new Y
        ftr->velocity.y = v0.y + (ftr->gravity * FRAME_TIME_MS) / SF;
        // Terminal velocity, arbitrarily chosen. Maybe make this a character attribute?
        if(ftr->velocity.y > 60 * SF)
        {
            ftr->velocity.y = 60 * SF;
        }
        // Now that we have Y velocity, find the new Y position
        dest_hurtbox.y0 = ftr->hurtbox.y0 + (((ftr->velocity.y + v0.y) * FRAME_TIME_MS) / (SF * 2));
    }
    else
    {
        // If the fighter is on the ground, don't bother doing Y axis math
        ftr->velocity.y = 0;
        dest_hurtbox.y0 = ftr->hurtbox.y0;
    }

    // Finish up the destination hurtbox
    dest_hurtbox.x1 = dest_hurtbox.x0 + ftr->size.x;
    dest_hurtbox.y1 = dest_hurtbox.y0 + ftr->size.y;

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
        ftr->passingThroughPlatform = NULL;
    }

    // If no collisions were detected at the final position
    if(false == collisionDetected)
    {
        // Just move it and be done
        memcpy(&ftr->hurtbox, &dest_hurtbox, sizeof(box_t));
    }
    else
    {
        // A collision at the destination was detected. Do a binary search to
        // find how close the fighter can get to the destination as possible.
        // Save the starting position
        box_t src_hurtbox;
        memcpy(&src_hurtbox, &ftr->hurtbox, sizeof(box_t));

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
                memcpy(&ftr->hurtbox, &test_hurtbox, sizeof(box_t));
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
    if(ftr->relativePos < FREE_FLOATING)
    {
        ftr->relativePos = FREE_FLOATING;
        ftr->touchingPlatform = NULL;
    }

    // Loop through all platforms to check for touching
    for (uint8_t idx = 0; idx < numPlatforms; idx++)
    {
        // Don't check the platform being passed throughd
        if(ftr->passingThroughPlatform == &platforms[idx])
        {
            continue;
        }

        // If the fighter is above or below a platform
        if((ftr->hurtbox.x0 < platforms[idx].area.x1 + SF) &&
                (ftr->hurtbox.x1 + SF > platforms[idx].area.x0))
        {
            // If the fighter is moving downward or not at all and hit a platform
            if ((ftr->velocity.y >= 0) &&
                    (((ftr->hurtbox.y1 / SF)) == (platforms[idx].area.y0 / SF)))
            {
                // Fighter standing on platform
                ftr->velocity.y = 0;
                ftr->relativePos = ABOVE_PLATFORM;
                ftr->touchingPlatform = &platforms[idx];
                ftr->numJumps = 2;
                break;
            }
            // If the fighter is moving upward and hit a platform
            else if ((ftr->velocity.y <= 0) &&
                     ((ftr->hurtbox.y0 / SF) == ((platforms[idx].area.y1 / SF))))
            {
                if((true == platforms[idx].canFallThrough))
                {
                    // Jump up through the platform
                    ftr->passingThroughPlatform = &platforms[idx];
                    ftr->relativePos = PASSING_THROUGH_PLATFORM;
                    ftr->touchingPlatform = NULL;
                }
                else
                {
                    // Fighter jumped into the bottom of a solid platform
                    ftr->velocity.y = 0;
                    ftr->relativePos = BELOW_PLATFORM;
                    ftr->touchingPlatform = &platforms[idx];
                }
                break;
            }
        }

        // If the fighter is to the left or right of a platform
        if((ftr->hurtbox.y0 < platforms[idx].area.y1 + SF) &&
                (ftr->hurtbox.y1 + SF > platforms[idx].area.y0))
        {
            // If the fighter is moving rightward and hit a wall
            if ((ftr->velocity.x >= 0) &&
                    (((ftr->hurtbox.x1 / SF)) == (platforms[idx].area.x0 / SF)))
            {
                if((true == platforms[idx].canFallThrough))
                {
                    // Pass through this platform from the side
                    ftr->passingThroughPlatform = &platforms[idx];
                    ftr->relativePos = PASSING_THROUGH_PLATFORM;
                    ftr->touchingPlatform = NULL;
                }
                else
                {
                    // Fighter to left of a solid platform
                    ftr->velocity.x = 0;
                    ftr->relativePos = LEFT_OF_PLATFORM;
                    ftr->touchingPlatform = &platforms[idx];
                }
                break;
            }
            // If the fighter is moving leftward and hit a wall
            else if ((ftr->velocity.x <= 0) &&
                     ((ftr->hurtbox.x0 / SF) == ((platforms[idx].area.x1 / SF))))
            {
                if((true == platforms[idx].canFallThrough))
                {
                    // Pass through this platform from the side
                    ftr->passingThroughPlatform = &platforms[idx];
                    ftr->relativePos = PASSING_THROUGH_PLATFORM;
                    ftr->touchingPlatform = NULL;
                }
                else
                {
                    // Fighter to right of a solid platform
                    ftr->velocity.x = 0;
                    ftr->relativePos = RIGHT_OF_PLATFORM;
                    ftr->touchingPlatform = &platforms[idx];
                }
                break;
            }
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
 * projectiles
 *
 * TODO
 *  - Draw HUD
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
        box_t projBox =
        {
            .x0 = proj->pos.x,
            .y0 = proj->pos.y,
            .x1 = proj->pos.x + proj->size.x,
            .y1 = proj->pos.y + proj->size.y,
        };

        // Draw the projectile
        drawBox(d, projBox, c050, false, SF);

        // Iterate
        currentNode = currentNode->next;
    }
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
