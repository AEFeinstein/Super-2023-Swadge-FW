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
#define FIGHTER_SIZE (16 * SF)

#define FRAME_TIME_MS 25 // 20fps

#define DEFAULT_GRAVITY    32

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    /* Position too! */
    box_t hurtbox;
    vector_t velocity;
    /* Gravity, Y is how floaty a jump is */
    vector_t acceleration; 
    bool isOnGround;
    /* A negative velocity applied when jumping.
     * The more negative, the higher the jump
     */
    int32_t jump_velo;
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

    fighters[0].hurtbox.x0 = 0 * SF;
    fighters[0].hurtbox.y0 = 0 * SF;
    fighters[0].hurtbox.x1 = fighters[0].hurtbox.x0 + FIGHTER_SIZE;
    fighters[0].hurtbox.y1 = fighters[0].hurtbox.y0 + FIGHTER_SIZE;
    fighters[0].velocity.x = 0 * SF;
    fighters[0].velocity.y = 0 * SF;
    fighters[0].acceleration.x = 0 * SF;
    fighters[0].acceleration.y = DEFAULT_GRAVITY * SF;
    fighters[0].isOnGround = false;
    fighters[0].jump_velo = -60;

    fighters[1].hurtbox.x0 = (((disp->w - 1 - 100) * SF) - FIGHTER_SIZE);
    fighters[1].hurtbox.y0 = 8 * SF;
    fighters[1].hurtbox.x1 = fighters[1].hurtbox.x0 + FIGHTER_SIZE;
    fighters[1].hurtbox.y1 = fighters[1].hurtbox.y0 + FIGHTER_SIZE;
    fighters[1].velocity.x = 0 * SF;
    fighters[1].velocity.y = 0 * SF;
    fighters[1].acceleration.x = 0 * SF;
    fighters[1].acceleration.y = DEFAULT_GRAVITY * SF;
    fighters[1].isOnGround = false;
    fighters[1].jump_velo = -60;
}

/**
 * @brief TODO
 *
 */
void fighterExitMode(void)
{

}

/**
 * @brief TODO
 *
 * @param elapsedUs
 */
void fighterMainLoop(int64_t elapsedUs)
{
    static int64_t frameElapsed = 0;
    frameElapsed += elapsedUs;
    if (frameElapsed > (FRAME_TIME_MS * 1000))
    {
        frameElapsed -= (FRAME_TIME_MS * 1000);

        updatePosition(&fighters[0], finalDest, sizeof(finalDest) / sizeof(finalDest[0]));
        updatePosition(&fighters[1], finalDest, sizeof(finalDest) / sizeof(finalDest[0]));

        // ESP_LOGI("FGT", "{[%d, %d], [%d, %d], %d}",
        //     fighters[0].hurtbox.x0,
        //     fighters[0].hurtbox.y0,
        //     fighters[0].velocity.x,
        //     fighters[0].velocity.y,
        //     fighters[0].isOnGround);
    }

    d->clearPx();

    rgba_pixel_t px =
    {
        .r = 0x1F,
        .g = 0x00,
        .b = 0x00,
        .a = PX_OPAQUE
    };
    drawBox(d, fighters[0].hurtbox, px, SF);

    px.r = 0;
    px.b = 0x1F;
    drawBox(d, fighters[1].hurtbox, px, SF);

    if (boxesCollide(fighters[0].hurtbox, fighters[1].hurtbox))
    {
        px.g = 0x1F;
        px.b = 0;
        fillDisplayArea(d, 0, 0, 10, 10, px);
    }

    px.r = 0x1F;
    px.g = 0x1F;
    px.b = 0x1F;
    for (uint8_t idx = 0; idx < sizeof(finalDest) / sizeof(finalDest[0]); idx++)
    {
        drawBox(d, finalDest[idx].area, px, SF);
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
    box_t upper_hurtbox;
    vector_t v0 = f->velocity;

    // Update X kinematics
    f->velocity.x = v0.x + (f->acceleration.x * FRAME_TIME_MS) / SF;
    upper_hurtbox.x0 = f->hurtbox.x0 + (((f->velocity.x + v0.x) * FRAME_TIME_MS) / (SF * 2));

    // Update Y kinematics
    f->velocity.y = v0.y + (f->acceleration.y * FRAME_TIME_MS) / SF;
    upper_hurtbox.y0 = f->hurtbox.y0 + (((f->velocity.y + v0.y) * FRAME_TIME_MS) / (SF * 2));

    // TODO cap velocity?
    // TODO don't update Y velo if on ground?

    // Finish up the upper hurtbox
    upper_hurtbox.x1 = upper_hurtbox.x0 + FIGHTER_SIZE;
    upper_hurtbox.y1 = upper_hurtbox.y0 + FIGHTER_SIZE;

    // Do a quick check to see if the binary search can be avoided altogether
    bool collisionDetected = false;
    for (uint8_t idx = 0; idx < numPlatforms; idx++)
    {
        if(boxesCollide(upper_hurtbox, platforms[idx].area))
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
                if(boxesCollide(test_hurtbox, platforms[idx].area))
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

        // After the final location was found, check what the fighter is up against
        f->isOnGround = false;
        for (uint8_t idx = 0; idx < numPlatforms; idx++)
        {
            // If a fighter is moving vertically
            if (f->velocity.y)
            {
                // If the fighter is moving downward and hit a platform
                if ((f->velocity.y > 0) && (((f->hurtbox.y1 / SF) + 1) == (platforms[idx].area.y0 / SF)))
                {
                    // Fighter above platform
                    f->velocity.y = 0;
                }
                // If the fighter is moving upward and hit a platform
                else if ((f->velocity.y < 0) && ((f->hurtbox.y0 / SF) == ((platforms[idx].area.y1 / SF) + 1)))
                {
                    // Fighter below platform
                    f->velocity.y = 0;
                }
            }

            // If a fighter is moving horizontally
            if (f->velocity.x)
            {
                // If the fighter is moving rightward and hit a wall
                if ((f->velocity.x > 0) && (((f->hurtbox.x1 / SF) + 1) == (platforms[idx].area.x0 / SF)))
                {
                    // Fighter to left of platform
                    f->velocity.x = 0;
                }
                // If the fighter is moving leftward and hit a wall
                else if ((f->velocity.x < 0) && ((f->hurtbox.x0 / SF) == ((platforms[idx].area.x1 / SF) + 1)))
                {
                    // Fighter to right of platform
                    f->velocity.x = 0;
                }
            }
        }
    }

    // Once the fighter is in the final position, no matter if there was a
    // collision or not, check if they are on the ground
    f->isOnGround = false;
    for (uint8_t idx = 0; idx < numPlatforms; idx++)
    {
        // A fighter only has to be on one platform to be on the ground.
        if ((f->velocity.y >= 0) &&
                ((f->hurtbox.y1 / SF) + 1) == (platforms[idx].area.y0 / SF) &&
                (f->hurtbox.x0 <= platforms[idx].area.x1) &&
                (f->hurtbox.x1 >= platforms[idx].area.x0))
        {
            f->isOnGround = true;
            f->velocity.y = 0;
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
    if(evt->down)
    {
        fighters[0].velocity.y = fighters[0].jump_velo * SF;
        fighters[0].isOnGround = false;
    }
}
