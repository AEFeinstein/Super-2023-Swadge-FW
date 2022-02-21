//==============================================================================
// Includes
//==============================================================================

#include "swadgeMode.h"
#include "mode_fighter.h"
#include "aabb_utils.h"

//==============================================================================
// Constants
//==============================================================================

// Division by a power of 2 has slightly more instructions than rshift, but handles negative numbers properly!
#define SF 128 // Scaling factor, a nice power of 2
#define FIGHTER_SIZE (16 * SF)

#define FRAME_TIME_MS 50 // 20fps

//==============================================================================
// Structs
//==============================================================================

typedef struct {
    box_t hurtbox;
    vector_t position;
    vector_t velocity;
    vector_t acceleration;
} fighter_t;

//==============================================================================
// Functoon Prototypes
//==============================================================================

void fighterEnterMode(display_t * disp);
void fighterExitMode(void);
void fighterMainLoop(int64_t elapsedUs);

void updatePosition(fighter_t * f);

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
    .fnButtonCallback = NULL, // fighterButtonCb,
    .fnTouchCallback = NULL, // fighterTouchCb,
    .wifiMode = NO_WIFI, // ESP_NOW,
    .fnEspNowRecvCb = NULL, // fighterEspNowRecvCb,
    .fnEspNowSendCb = NULL, // fighterEspNowSendCb,
    .fnAccelerometerCallback = NULL, // fighterAccelerometerCb,
    .fnAudioCallback = NULL, // fighterAudioCb,
    .fnTemperatureCallback = NULL, // fighterTemperatureCb
};

fighter_t fighters[2];
display_t * d;

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 *
 * @param disp
 */
void fighterEnterMode(display_t * disp)
{
    d = disp;

    fighters[0].position.x = 0 * SF;
    fighters[0].position.y = 0 * SF;
    fighters[0].velocity.x = 2 * SF;
    fighters[0].velocity.y = 2 * SF;
    fighters[0].acceleration.x = 0 * SF;
    fighters[0].acceleration.y = 0 * SF;
    fighters[0].hurtbox.x0 = fighters[0].position.x;
    fighters[0].hurtbox.y0 = fighters[0].position.y;
    fighters[0].hurtbox.x1 = fighters[0].position.x + FIGHTER_SIZE;
    fighters[0].hurtbox.y1 = fighters[0].position.y + FIGHTER_SIZE;

    fighters[1].position.x = (((disp->w - 1 - 100) * SF) - FIGHTER_SIZE);
    fighters[1].position.y = 8 * SF;
    fighters[1].velocity.x = -2 * SF;
    fighters[1].velocity.y = 2 * SF;
    fighters[1].acceleration.x = 0 * SF;
    fighters[1].acceleration.y = 0 * SF;
    fighters[1].hurtbox.x0 = fighters[1].position.x;
    fighters[1].hurtbox.y0 = fighters[1].position.y;
    fighters[1].hurtbox.x1 = fighters[1].position.x + FIGHTER_SIZE;
    fighters[1].hurtbox.y1 = fighters[1].position.y + FIGHTER_SIZE;
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
    if(frameElapsed > (FRAME_TIME_MS * 1000))
    {
        frameElapsed -= (FRAME_TIME_MS * 1000);

        updatePosition(&fighters[0]);
        updatePosition(&fighters[1]);
    }

    d->clearPx();

    rgba_pixel_t px = {
        .r = 0x1F,
        .g = 0x00,
        .b = 0x00,
        .a = PX_OPAQUE
    };
    drawBox(d, fighters[0].hurtbox, px, SF);

    px.r = 0;
    px.b = 0x1F;
    drawBox(d, fighters[1].hurtbox, px, SF);

    if(boxesCollide(fighters[0].hurtbox, fighters[1].hurtbox))
    {
        px.g = 0x1F;
        px.b = 0;
        fillDisplayArea(d, 0, 0, 10, 10, px);
    }
}

/**
 * @brief TODO
 * 
 * @param f 
 */
void updatePosition(fighter_t * f)
{
    // TODO update acceleration?

    f->velocity.x += ((f->acceleration.x * FRAME_TIME_MS) / SF);
    f->velocity.y += ((f->acceleration.y * FRAME_TIME_MS) / SF);

    f->position.x += (((f->velocity.x * FRAME_TIME_MS) / SF) + ((f->acceleration.x * FRAME_TIME_MS * FRAME_TIME_MS) / (SF * 4)));
    f->position.y += (((f->velocity.y * FRAME_TIME_MS) / SF) + ((f->acceleration.y * FRAME_TIME_MS * FRAME_TIME_MS) / (SF * 4)));

    f->hurtbox.x0 = f->position.x;
    f->hurtbox.y0 = f->position.y;
    f->hurtbox.x1 = f->position.x + FIGHTER_SIZE;
    f->hurtbox.y1 = f->position.y + FIGHTER_SIZE;
}
