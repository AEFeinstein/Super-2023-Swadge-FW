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

//==============================================================================
// Constants
//==============================================================================

#define FRAME_TIME_MS 25 // 20fps

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    fighter_t* fighters;
    uint8_t numFighters;
    display_t* d;
} fightingGame_t;

//==============================================================================
// Function Prototypes
//==============================================================================

void fighterEnterMode(display_t* disp);
void fighterExitMode(void);
void fighterMainLoop(int64_t elapsedUs);
void fighterButtonCb(buttonEvt_t* evt);

void updatePosition(fighter_t* f, const platform_t* platforms, uint8_t numPlatforms);
void checkFighterTimer(fighter_t* ftr);
void drawFighter(display_t* d, fighter_t* ftr);

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
 * @brief TODO
 *
 * @param disp
 */
void fighterEnterMode(display_t* disp)
{
    f = malloc(sizeof(fightingGame_t));
    f->d = disp;
    f->fighters = loadJsonFighterData(&(f->numFighters));
    f->fighters[0].relativePos = FREE_FLOATING;
    f->fighters[1].relativePos = FREE_FLOATING;

    ESP_LOGD("FGT", "data loaded");

    // loadWsg("kd0.wsg", &kd[0]);
    // loadWsg("kd1.wsg", &kd[1]);
}

/**
 * @brief TODO
 *
 */
void fighterExitMode(void)
{
    freeFighterData(f->fighters, f->numFighters);
    // freeWsg(&kd[0]);
    // freeWsg(&kd[1]);
}

/**
 * @brief TODO
 *
 * @param elapsedUs
 */
void fighterMainLoop(int64_t elapsedUs)
{
    static uint8_t animIdx = 0;
    static int64_t animElapsed = 0;
    animElapsed += elapsedUs;
    if(animElapsed > 500000)
    {
        animElapsed -= 500000;
        animIdx = animIdx ? 0 : 1;
    }

    static int64_t frameElapsed = 0;
    frameElapsed += elapsedUs;
    if (frameElapsed > (FRAME_TIME_MS * 1000))
    {
        frameElapsed -= (FRAME_TIME_MS * 1000);

        updatePosition(&f->fighters[0], finalDest, sizeof(finalDest) / sizeof(finalDest[0]));
        updatePosition(&f->fighters[1], finalDest, sizeof(finalDest) / sizeof(finalDest[0]));

        checkFighterTimer(&f->fighters[0]);
        checkFighterTimer(&f->fighters[1]);

        // ESP_LOGI("FGT", "{[%d, %d], [%d, %d], %d}",
        //     f->fighters[0].hurtbox.x0 / SF,
        //     f->fighters[0].hurtbox.y0 / SF,
        //     f->fighters[0].velocity.x,
        //     f->fighters[0].velocity.y,
        //     f->fighters[0].relativePos);
    }

    f->d->clearPx();

    for (uint8_t idx = 0; idx < sizeof(finalDest) / sizeof(finalDest[0]); idx++)
    {
        drawBox(f->d, finalDest[idx].area, c555, !finalDest[idx].canFallThrough, SF);
    }

    drawFighter(f->d, &f->fighters[0]);
    drawFighter(f->d, &f->fighters[1]);
}

/**
 * @brief TODO
 *
 * @param d
 * @param ftr
 */
void drawFighter(display_t* d, fighter_t* ftr)
{
    /* Pick the color based on state */
    paletteColor_t hitboxColor = c500;
    switch(ftr->state)
    {
        case FS_GROUND_STARTUP:
        case FS_AIR_STARTUP:
        {
            hitboxColor = c502;
            break;
        }
        case FS_GROUND_ATTACK:
        case FS_AIR_ATTACK:
        {
            hitboxColor = c303;
            break;
        }
        case FS_GROUND_COOLDOWN:
        case FS_AIR_COOLDOWN:
        {
            hitboxColor = c205;
            break;
        }
        default:
        {
            break;
        }
    }
    drawBox(d, ftr->hurtbox, hitboxColor, false, SF);

    /* Draw which way the figher is facing */
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
            // Draw no indicator
            break;
        }
    }

    /* Draw the hitbox if attacking */
    if((FS_GROUND_ATTACK == ftr->state) || (FS_AIR_ATTACK == ftr->state))
    {
        box_t relativeHitbox = ftr->hurtbox;
        if((ftr->dir == FACING_LEFT) &&
                ((DASH_GROUND == ftr->cAttack) ||
                 (FRONT_AIR   == ftr->cAttack) ||
                 (BACK_AIR    == ftr->cAttack)))
        {
            /* reverse the hitbox if dashing and facing left */
            relativeHitbox.x1 = relativeHitbox.x0 + ftr->size.x -
                                ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].hitboxPos.x;
            relativeHitbox.x0 = relativeHitbox.x1 - ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].hitboxSize.x;
        }
        else
        {
            relativeHitbox.x0 += ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].hitboxPos.x;
            relativeHitbox.x1 = relativeHitbox.x0 + ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].hitboxSize.x;
        }
        relativeHitbox.y0 += ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].hitboxPos.y;
        relativeHitbox.y1 = relativeHitbox.y0 + ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].hitboxSize.y;
        drawBox(d, relativeHitbox, c440, false, SF);
    }
}

/**
 * @brief TODO
 *
 * @param ftr
 */
void checkFighterTimer(fighter_t* ftr)
{
    if(ftr->fallThroughTimer > 0)
    {
        ftr->fallThroughTimer--;
    }

    bool shouldTransition = false;
    if(ftr->timer > 0)
    {
        ftr->timer--;
        if(0 == ftr->timer)
        {
            shouldTransition = true;
        }
    }

    if(shouldTransition)
    {
        switch(ftr->state)
        {
            case FS_GROUND_STARTUP:
            {
                ftr->state = FS_GROUND_ATTACK;
                ftr->attackFrame = 0;
                ftr->timer = ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].duration;
                break;
            }
            case FS_GROUND_ATTACK:
            {
                ftr->attackFrame++;
                if(ftr->attackFrame < ftr->attacks[ftr->cAttack].numAttackFrames)
                {
                    // Don't change state
                    ftr->timer = ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].duration;
                }
                else
                {
                    ftr->state = FS_GROUND_COOLDOWN;
                    ftr->timer = ftr->attacks[ftr->cAttack].endLag;
                }
                break;
            }
            case FS_GROUND_COOLDOWN:
            {
                ftr->state = FS_IDLE;
                break;
            }
            case FS_AIR_STARTUP:
            {
                ftr->state = FS_AIR_ATTACK;
                ftr->attackFrame = 0;
                ftr->timer = ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].duration;
                break;
            }
            case FS_AIR_ATTACK:
            {
                ftr->attackFrame++;
                if(ftr->attackFrame < ftr->attacks[ftr->cAttack].numAttackFrames)
                {
                    // Don't change state
                    ftr->timer = ftr->attacks[ftr->cAttack].attackFrames[ftr->attackFrame].duration;
                }
                else
                {
                    ftr->state = FS_AIR_COOLDOWN;
                    ftr->timer = ftr->attacks[ftr->cAttack].endLag;
                }
                break;
            }
            case FS_AIR_COOLDOWN:
            {
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
                // TODO
                break;
            }
        }
    }
}
/**
 * @brief TODO
 *
 * @param f
 * @param platforms
 * @param numPlatforms
 */
void updatePosition(fighter_t* ftr, const platform_t* platforms, uint8_t numPlatforms)
{
    // The hitbox where the fighter will travel to
    box_t upper_hurtbox;
    // Initial velocity before this frame's calculations
    vector_t v0 = ftr->velocity;

    // Check for button presses, jump first
    if (!(ftr->prevBtnState & BTN_A) && (ftr->btnState & BTN_A))
    {
        if(ftr->numJumps > 0)
        {
            ftr->numJumps--;
            v0.y = ftr->jump_velo;
            ftr->relativePos = FREE_FLOATING;
            ftr->touchingPlatform = NULL;
        }
    }

    // Attack button
    if (!(ftr->prevBtnState & BTN_B) && (ftr->btnState & BTN_B))
    {
        // TODO don't allow attacks in all fighter states
        if(ABOVE_PLATFORM == ftr->relativePos)
        {
            // Attack on ground
            if(ftr->btnState & UP)
            {
                // Up tilt attack
                ftr->cAttack = UP_GROUND;
                ftr->state = FS_GROUND_STARTUP;
                ftr->timer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else if(ftr->btnState & DOWN)
            {
                // Down tilt attack
                ftr->cAttack = DOWN_GROUND;
                ftr->state = FS_GROUND_STARTUP;
                ftr->timer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else if((ftr->btnState & LEFT) || (ftr->btnState & RIGHT))
            {
                // Side attack
                ftr->cAttack = DASH_GROUND;
                ftr->state = FS_GROUND_STARTUP;
                ftr->timer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else
            {
                // Neutral attack
                ftr->cAttack = NEUTRAL_GROUND;
                ftr->state = FS_GROUND_STARTUP;
                ftr->timer = ftr->attacks[ftr->cAttack].startupLag;
            }
        }
        else
        {
            // Attack in air
            if(ftr->btnState & UP)
            {
                // Up air attack
                ftr->cAttack = UP_AIR;
                ftr->state = FS_AIR_STARTUP;
                ftr->timer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else if(ftr->btnState & DOWN)
            {
                // Down air
                ftr->cAttack = DOWN_AIR;
                ftr->state = FS_AIR_STARTUP;
                ftr->timer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else if(((ftr->btnState & LEFT ) && (FACING_RIGHT == ftr->dir)) ||
                    ((ftr->btnState & RIGHT) && (FACING_LEFT  == ftr->dir)))
            {
                // Back air
                ftr->cAttack = BACK_AIR;
                ftr->state = FS_AIR_STARTUP;
                ftr->timer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else if(((ftr->btnState & RIGHT) && (FACING_RIGHT == ftr->dir)) ||
                    ((ftr->btnState & LEFT ) && (FACING_LEFT  == ftr->dir)))
            {
                // Front air
                ftr->cAttack = FRONT_AIR;
                ftr->state = FS_AIR_STARTUP;
                ftr->timer = ftr->attacks[ftr->cAttack].startupLag;
            }
            else
            {
                // Neutral air
                ftr->cAttack = NEUTRAL_AIR;
                ftr->state = FS_AIR_STARTUP;
                ftr->timer = ftr->attacks[ftr->cAttack].startupLag;
            }
        }
    }

    // Double tap to fall through platforms
    if(ftr->relativePos == ABOVE_PLATFORM && ftr->touchingPlatform->canFallThrough)
    {
        // Check if button was released
        if ((ftr->prevBtnState & DOWN) && !(ftr->btnState & DOWN))
        {
            // Start timer to check for second press
            ftr->fallThroughTimer = 10; // 10 frames @ 25ms == 250ms
        }
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

    // Update X kinematics
    if(ftr->btnState & LEFT)
    {
        if(ABOVE_PLATFORM == ftr->relativePos)
        {
            ftr->dir = FACING_LEFT;
        }

        if(ftr->relativePos != RIGHT_OF_PLATFORM)
        {
            // Accelerate towards the left
            ftr->velocity.x = v0.x - (ftr->run_accel * FRAME_TIME_MS) / SF;
            if (ftr->velocity.x < -ftr->run_max_velo)
            {
                ftr->velocity.x = -ftr->run_max_velo;
            }
        }
        else
        {
            ftr->velocity.x = 0;
        }
    }
    else if(ftr->btnState & RIGHT)
    {
        if(ABOVE_PLATFORM == ftr->relativePos)
        {
            ftr->dir = FACING_RIGHT;
        }

        if(ftr->relativePos != LEFT_OF_PLATFORM)
        {
            // Accelerate towards the right
            ftr->velocity.x = v0.x + (ftr->run_accel * FRAME_TIME_MS) / SF;
            if(ftr->velocity.x > ftr->run_max_velo)
            {
                ftr->velocity.x = ftr->run_max_velo;
            }
        }
        else
        {
            ftr->velocity.x = 0;
        }
    }
    else
    {
        if(ftr->velocity.x > 0)
        {
            // Decelrate towards the left
            ftr->velocity.x = v0.x - (ftr->run_decel * FRAME_TIME_MS) / SF;
            if (ftr->velocity.x < 0)
            {
                ftr->velocity.x = 0;
            }
        }
        else if(ftr->velocity.x < 0)
        {
            // Decelerate towards the right
            ftr->velocity.x = v0.x + (ftr->run_decel * FRAME_TIME_MS) / SF;
            if(ftr->velocity.x > 0)
            {
                ftr->velocity.x = 0;
            }
        }
    }
    // Find the new X position
    upper_hurtbox.x0 = ftr->hurtbox.x0 + (((ftr->velocity.x + v0.x) * FRAME_TIME_MS) / (SF * 2));

    // Update Y kinematics
    if(ftr->relativePos != ABOVE_PLATFORM)
    {
        // Fighter is in the air, so there will be a new Y
        ftr->velocity.y = v0.y + (ftr->gravity * FRAME_TIME_MS) / SF;
        // TODO cap velocity?
        upper_hurtbox.y0 = ftr->hurtbox.y0 + (((ftr->velocity.y + v0.y) * FRAME_TIME_MS) / (SF * 2));
    }
    else
    {
        // If the fighter is on the ground, don't bother doing Y axis math
        ftr->velocity.y = 0;
        upper_hurtbox.y0 = ftr->hurtbox.y0;
    }

    // Finish up the upper hurtbox
    upper_hurtbox.x1 = upper_hurtbox.x0 + ftr->size.x;
    upper_hurtbox.y1 = upper_hurtbox.y0 + ftr->size.y;

    // Do a quick check to see if the binary search can be avoided altogether
    bool collisionDetected = false;
    bool intersectionDetected = false;
    for (uint8_t idx = 0; idx < numPlatforms; idx++)
    {
        if(boxesCollide(upper_hurtbox, platforms[idx].area))
        {
            intersectionDetected = true;
            if(ftr->passingThroughPlatform != &platforms[idx])
            {
                collisionDetected = true;
                break;
            }
        }
    }

    // If there are no intersections, then the fighter isn't passing through
    // a platform anymore
    if(!intersectionDetected)
    {
        ftr->passingThroughPlatform = NULL;
    }

    // If no collisions were detected at the final position
    if(!collisionDetected)
    {
        // Just move it and be done
        memcpy(&ftr->hurtbox, &upper_hurtbox, sizeof(box_t));
    }
    else
    {
        // Save the starting position
        box_t lower_hurtbox;
        memcpy(&lower_hurtbox, &ftr->hurtbox, sizeof(box_t));

        // Start the binary search at the midpoint between lower and upper
        box_t test_hurtbox;
        test_hurtbox.x0 = (lower_hurtbox.x0 + upper_hurtbox.x0) / 2;
        test_hurtbox.y0 = (lower_hurtbox.y0 + upper_hurtbox.y0) / 2;
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
                    collisionDetected = true;
                    break;
                }
            }

            if(collisionDetected)
            {
                // If there was a collision, move back by setting the new upper
                // bound as the test point
                memcpy(&upper_hurtbox, &test_hurtbox, sizeof(box_t));
            }
            else
            {
                // If there wasn't a collision, move back by setting the new
                // lower bound as the test point
                memcpy(&lower_hurtbox, &test_hurtbox, sizeof(box_t));

                // And since there wasn't a collision, move the fighter here
                memcpy(&ftr->hurtbox, &test_hurtbox, sizeof(box_t));
            }

            // Recalculate the new test point
            test_hurtbox.x0 = (lower_hurtbox.x0 + upper_hurtbox.x0) / 2;
            test_hurtbox.y0 = (lower_hurtbox.y0 + upper_hurtbox.y0) / 2;
            test_hurtbox.x1 = test_hurtbox.x0 + ftr->size.x;
            test_hurtbox.y1 = test_hurtbox.y0 + ftr->size.y;

            // Check for convergence
            if(((test_hurtbox.x0 == lower_hurtbox.x0) || (test_hurtbox.x0 == upper_hurtbox.x0)) &&
                    ((test_hurtbox.y0 == lower_hurtbox.y0) || (test_hurtbox.y0 == upper_hurtbox.y0)))
            {
                // Binary search has converged
                break;
            }
        }
    }

    // If the fighter is touching something, clear that
    if(ftr->relativePos < FREE_FLOATING)
    {
        ftr->relativePos = FREE_FLOATING;
        ftr->touchingPlatform = NULL;
    }

    // After the final location was found, check what the fighter is up against
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
                // Fighter above platform
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
                    // Fighter below platform
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
                    // Jump up through the platform
                    ftr->passingThroughPlatform = &platforms[idx];
                    ftr->relativePos = PASSING_THROUGH_PLATFORM;
                    ftr->touchingPlatform = NULL;
                }
                else
                {
                    // Fighter to left of platform
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
                    // Jump up through the platform
                    ftr->passingThroughPlatform = &platforms[idx];
                    ftr->relativePos = PASSING_THROUGH_PLATFORM;
                    ftr->touchingPlatform = NULL;
                }
                else
                {
                    // Fighter to right of platform
                    ftr->velocity.x = 0;
                    ftr->relativePos = RIGHT_OF_PLATFORM;
                    ftr->touchingPlatform = &platforms[idx];
                }
                break;
            }
        }
    }

    // Save the button state for the next frame comparison
    ftr->prevBtnState = ftr->btnState;
}

/**
 * @brief TODO
 *
 * @param evt
 */
void fighterButtonCb(buttonEvt_t* evt)
{
    // Save the state to check synchronously
    f->fighters[0].btnState = evt->state;
}
