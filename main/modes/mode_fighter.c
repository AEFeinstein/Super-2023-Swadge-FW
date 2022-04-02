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

#include "esp_log.h"
#include "esp_timer.h"

#include "swadgeMode.h"
#include "mode_fighter.h"
#include "aabb_utils.h"
#include "bresenham.h"

//==============================================================================
// Constants
//==============================================================================

// Division by a power of 2 has slightly more instructions than rshift, but handles negative numbers properly!
#define SF (1 << 8) // Scaling factor, a nice power of 2
#define FIGHTER_SIZE (24 * SF)

#define FRAME_TIME_MS 25 // 20fps

#define DEFAULT_GRAVITY    32

//==============================================================================
// Structs
//==============================================================================

typedef enum
{
    FREE_FLOATING,
    ABOVE_PLATFORM,
    BELOW_PLATFORM,
    RIGHT_OF_PLATFORM,
    LEFT_OF_PLATFORM
} platformPos_t;

typedef struct
{
    /* Position too! */
    box_t hurtbox;
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
    int32_t btnState;
} fighter_t;

typedef struct
{
    box_t area;
    bool canFallThrough;
} platform_t;

//==============================================================================
// Functoon Prototypes
//==============================================================================

void fighterEnterMode(display_t *disp);
void fighterExitMode(void);
void fighterMainLoop(int64_t elapsedUs);
void fighterButtonCb(buttonEvt_t* evt);

void updatePosition(fighter_t *f, const platform_t *platforms, uint8_t numPlatforms);

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

fighter_t fighters[2];
display_t *d;
wsg_t kd[2];

static const platform_t finalDest[] =
{
    {
        .area =
        {
            .x0 = (14) * SF,
            .y0 = (170) * SF,
            .x1 = (14 + 212 - 1) * SF,
            .y1 = (170 + 4 - 1) * SF,
        },
        .canFallThrough = false
    },
    {
        .area =
        {
            .x0 = (30) * SF,
            .y0 = (130) * SF,
            .x1 = (30 + 54 - 1) * SF,
            .y1 = (130 + 4 - 1) * SF,
        },
        .canFallThrough = true
    },
    {
        .area =
        {
            .x0 = (156) * SF,
            .y0 = (130) * SF,
            .x1 = (156 + 54 - 1) * SF,
            .y1 = (130 + 4 - 1) * SF,
        },
        .canFallThrough = true
    },
    {
        .area =
        {
            .x0 = (93) * SF,
            .y0 = (90) * SF,
            .x1 = (93 + 54 - 1) * SF,
            .y1 = (90 + 4 - 1) * SF,
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
void fighterEnterMode(display_t *disp)
{
    d = disp;

    loadWsg("kd0.wsg", &kd[0]);
    loadWsg("kd1.wsg", &kd[1]);

    fighters[0].hurtbox.x0 = (32) * SF;
    fighters[0].hurtbox.y0 = 0 * SF;
    fighters[0].hurtbox.x1 = fighters[0].hurtbox.x0 + FIGHTER_SIZE;
    fighters[0].hurtbox.y1 = fighters[0].hurtbox.y0 + FIGHTER_SIZE;
    fighters[0].velocity.x = 0 * SF;
    fighters[0].velocity.y = 0 * SF;
    fighters[0].relativePos = FREE_FLOATING;
    fighters[0].numJumps = 0;
    fighters[0].gravity = DEFAULT_GRAVITY * SF;
    fighters[0].jump_velo = -60 * SF;
    fighters[0].run_accel = DEFAULT_GRAVITY * SF;
    fighters[0].run_decel = DEFAULT_GRAVITY * SF;
    fighters[0].run_max_velo = 60 * SF;

    fighters[1].hurtbox.x0 = (240-32) * SF - FIGHTER_SIZE;
    fighters[1].hurtbox.y0 = 0 * SF;
    fighters[1].hurtbox.x1 = fighters[1].hurtbox.x0 + FIGHTER_SIZE;
    fighters[1].hurtbox.y1 = fighters[1].hurtbox.y0 + FIGHTER_SIZE;
    fighters[1].velocity.x = 0 * SF;
    fighters[1].velocity.y = 0 * SF;
    fighters[1].relativePos = FREE_FLOATING;
    fighters[1].numJumps = 0;
    fighters[1].gravity = DEFAULT_GRAVITY * SF;
    fighters[1].jump_velo = -60 * SF;
    fighters[1].run_accel = DEFAULT_GRAVITY * SF;
    fighters[1].run_decel = DEFAULT_GRAVITY * SF;
    fighters[1].run_max_velo = 60 * SF;
}

/**
 * @brief TODO
 *
 */
void fighterExitMode(void)
{
    freeWsg(&kd[0]);
    freeWsg(&kd[1]);
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

        updatePosition(&fighters[0], finalDest, sizeof(finalDest) / sizeof(finalDest[0]));
        updatePosition(&fighters[1], finalDest, sizeof(finalDest) / sizeof(finalDest[0]));

        // ESP_LOGI("FGT", "{[%d, %d], [%d, %d], %d}",
        //     fighters[0].hurtbox.x0 / SF,
        //     fighters[0].hurtbox.y0 / SF,
        //     fighters[0].velocity.x,
        //     fighters[0].velocity.y,
        //     fighters[0].relativePos);
    }

    d->clearPx();

    // drawBox(d, fighters[0].hurtbox, c500, SF);
    drawWsg(d, &kd[animIdx], fighters[0].hurtbox.x0 / SF, fighters[0].hurtbox.y0 / SF);

    // drawBox(d, fighters[1].hurtbox, c005, SF);
    drawWsg(d, &kd[(1 + animIdx) % 2], fighters[1].hurtbox.x0 / SF, fighters[1].hurtbox.y0 / SF);

    for (uint8_t idx = 0; idx < sizeof(finalDest) / sizeof(finalDest[0]); idx++)
    {
        drawBox(d, finalDest[idx].area, c555, SF);
    }
}

/**
 * @brief TODO
 *
 * @param f
 * @param platforms
 * @param numPlatforms
 */
void updatePosition(fighter_t *f, const platform_t *platforms, uint8_t numPlatforms)
{
    // The hitbox where the fighter will travel to
    box_t upper_hurtbox;
    // Initial velocity before this frame's calculations
    vector_t v0 = f->velocity;

    // Update X kinematics
    if(f->btnState & LEFT)
    {
        if(f->relativePos != RIGHT_OF_PLATFORM)
        {
            // Accelerate towards the left
            f->velocity.x = v0.x - (f->run_accel * FRAME_TIME_MS) / SF;
            if (f->velocity.x < -f->run_max_velo)
            {
                f->velocity.x = -f->run_max_velo;
            }
        }
        else
        {
            f->velocity.x = 0;
        }
    }
    else if(f->btnState & RIGHT)
    {
        if(f->relativePos != LEFT_OF_PLATFORM)
        {
            // Accelerate towards the right
            f->velocity.x = v0.x + (f->run_accel * FRAME_TIME_MS) / SF;
            if(f->velocity.x > f->run_max_velo)
            {
                f->velocity.x = f->run_max_velo;
            }
        }
        else
        {
            f->velocity.x = 0;
        }
    }
    else
    {
        if(f->velocity.x > 0)
        {
            // Decelrate towards the left
            f->velocity.x = v0.x - (f->run_decel * FRAME_TIME_MS) / SF;
            if (f->velocity.x < 0)
            {
                f->velocity.x = 0;
            }
        }
        else if(f->velocity.x < 0)
        {
            // Decelerate towards the right
            f->velocity.x = v0.x + (f->run_decel * FRAME_TIME_MS) / SF;
            if(f->velocity.x > 0)
            {
                f->velocity.x = 0;
            }
        }
    }
    // Find the new X position
    upper_hurtbox.x0 = f->hurtbox.x0 + (((f->velocity.x + v0.x) * FRAME_TIME_MS) / (SF * 2));

    // Update Y kinematics
    if(f->relativePos != ABOVE_PLATFORM)
    {
        // Fighter is in the air, so there will be a new Y
        f->velocity.y = v0.y + (f->gravity * FRAME_TIME_MS) / SF;
        // TODO cap velocity?
        upper_hurtbox.y0 = f->hurtbox.y0 + (((f->velocity.y + v0.y) * FRAME_TIME_MS) / (SF * 2));
    }
    else
    {
        // If the fighter is on the ground, don't bother doing Y axis math
        f->velocity.y = 0;
        upper_hurtbox.y0 = f->hurtbox.y0;
    }

    // Finish up the upper hurtbox
    upper_hurtbox.x1 = upper_hurtbox.x0 + FIGHTER_SIZE;
    upper_hurtbox.y1 = upper_hurtbox.y0 + FIGHTER_SIZE;

    // Do a quick check to see if the binary search can be avoided altogether
    bool collisionDetected = false;
    for (uint8_t idx = 0; idx < numPlatforms; idx++)
    {
        if(boxesCollide(upper_hurtbox, platforms[idx].area, SF))
        {
            collisionDetected = true;
        }
    }

    // If no collisions were detected at the final position
    if(!collisionDetected)
    {
        // Just move it and be done
        memcpy(&f->hurtbox, &upper_hurtbox, sizeof(box_t));
    }
    else
    {
        // Save the starting position
        box_t lower_hurtbox;
        memcpy(&lower_hurtbox, &f->hurtbox, sizeof(box_t));

        // Start the binary search at the midpoint between lower and upper
        box_t test_hurtbox;
        test_hurtbox.x0 = (lower_hurtbox.x0 + upper_hurtbox.x0) / 2;
        test_hurtbox.y0 = (lower_hurtbox.y0 + upper_hurtbox.y0) / 2;
        test_hurtbox.x1 = test_hurtbox.x0 + FIGHTER_SIZE;
        test_hurtbox.y1 = test_hurtbox.y0 + FIGHTER_SIZE;

        // Binary search between where the fighter is and where the fighter
        // wants to be until it converges
        while(true)
        {
            // Check if there are any collisions at this position
            collisionDetected = false;
            for (uint8_t idx = 0; idx < numPlatforms; idx++)
            {
                if(boxesCollide(test_hurtbox, platforms[idx].area, SF))
                {
                    collisionDetected = true;
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
                memcpy(&f->hurtbox, &test_hurtbox, sizeof(box_t));
            }

            // Recalculate the new test point
            test_hurtbox.x0 = (lower_hurtbox.x0 + upper_hurtbox.x0) / 2;
            test_hurtbox.y0 = (lower_hurtbox.y0 + upper_hurtbox.y0) / 2;
            test_hurtbox.x1 = test_hurtbox.x0 + FIGHTER_SIZE;
            test_hurtbox.y1 = test_hurtbox.y0 + FIGHTER_SIZE;

            // Check for convergence
            if(((test_hurtbox.x0 == lower_hurtbox.x0) || (test_hurtbox.x0 == upper_hurtbox.x0)) &&
                    ((test_hurtbox.y0 == lower_hurtbox.y0) || (test_hurtbox.y0 == upper_hurtbox.y0)))
            {
                // Binary search has converged
                break;
            }
        }
    }

    // After the final location was found, check what the fighter is up against
    f->relativePos = FREE_FLOATING;
    for (uint8_t idx = 0; idx < numPlatforms; idx++)
    {
        // If the fighter is moving downward or not at all and hit a platform
        if ((f->velocity.y >= 0) &&
            (((f->hurtbox.y1 / SF) + 1) == (platforms[idx].area.y0 / SF)) && 
            (f->hurtbox.x0 < platforms[idx].area.x1 + SF) &&
            (f->hurtbox.x1 + SF > platforms[idx].area.x0))
        {
            // Fighter above platform
            f->velocity.y = 0;
            f->relativePos = ABOVE_PLATFORM;
            f->numJumps = 2;
            break;
        }
        // If the fighter is moving upward and hit a platform
        else if ((f->velocity.y <= 0) &&
            ((f->hurtbox.y0 / SF) == ((platforms[idx].area.y1 / SF) + 1)) && 
            (f->hurtbox.x0 < platforms[idx].area.x1 + SF) &&
            (f->hurtbox.x1 + SF > platforms[idx].area.x0))
        {
            // Fighter below platform
            f->velocity.y = 0;
            f->relativePos = BELOW_PLATFORM;
            break;
        }

        // If the fighter is moving rightward and hit a wall
        if ((f->velocity.x >= 0) &&
            (((f->hurtbox.x1 / SF) + 1) == (platforms[idx].area.x0 / SF)) &&
            (f->hurtbox.y0 < platforms[idx].area.y1 + SF) &&
            (f->hurtbox.y1 + SF > platforms[idx].area.y0))
        {
            // Fighter to left of platform
            f->velocity.x = 0;
            f->relativePos = LEFT_OF_PLATFORM;
            break;
        }
        // If the fighter is moving leftward and hit a wall
        else if ((f->velocity.x <= 0) &&
            ((f->hurtbox.x0 / SF) == ((platforms[idx].area.x1 / SF) + 1)) &&
            (f->hurtbox.y0 < platforms[idx].area.y1 + SF) &&
            (f->hurtbox.y1 + SF > platforms[idx].area.y0))
        {
            // Fighter to right of platform
            f->velocity.x = 0;
            f->relativePos = RIGHT_OF_PLATFORM;
            break;
        }
    }
}

/**
 * @brief TODO
 *
 * @param evt
 */
void fighterButtonCb(buttonEvt_t* evt)
{
    // Save the state for X axis kinematics
    fighters[0].btnState = evt->state;

    // Check for a jump
    if(evt->down)
    {
        switch(evt->button)
        {
            case UP:
            {
                if(fighters[0].numJumps > 0)
                {
                    fighters[0].numJumps--;
                    fighters[0].velocity.y = fighters[0].jump_velo;
                    fighters[0].relativePos = FREE_FLOATING;
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }
}
