//==============================================================================
// Includes
//==============================================================================

#include <string.h>

#include "esp_timer.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tusb_hid_gamepad.h"

#include "swadgeMode.h"

#include "musical_buzzer.h"
#include "nvs_manager.h"
#include "display.h"
#include "led_util.h"
#include "p2pConnection.h"
#include "bresenham.h"

#include "mode_demo.h"

#include "DFT32.h"
#include "embeddednf.h"
#include "embeddedout.h"

//==============================================================================
// Functions Prototypes
//==============================================================================

void demoEnterMode(display_t * disp);
void demoExitMode(void);
void demoMainLoop(int64_t elapsedUs);
void demoAccelerometerCb(accel_t* accel);
void demoAudioCb(uint16_t * samples, uint32_t sampleCnt);
void demoTemperatureCb(float tmp_c);
void demoButtonCb(buttonEvt_t* evt);
void demoTouchCb(touch_event_t* evt);
void demoEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);
void demoEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

void demoConCbFn(p2pInfo* p2p, connectionEvt_t evt);
void demoMsgRxCbFn(p2pInfo* p2p, const char* msg, const char* payload, uint8_t len);
void demoMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);

//==============================================================================
// Variables
//==============================================================================

/**
 * @brief Musical Notation: Beethoven's Ode to joy
 */
static const song_t odeToJoy =
{
    .notes =
    {
        {740, 400}, {740, 600}, {784, 400}, {880, 400},
        {880, 400}, {784, 400}, {740, 400}, {659, 400},
        {587, 400}, {587, 400}, {659, 400}, {740, 400},
        {740, 400}, {740, 200}, {659, 200}, {659, 800},

        {740, 400}, {740, 600}, {784, 400}, {880, 400},
        {880, 400}, {784, 400}, {740, 400}, {659, 400},
        {587, 400}, {587, 400}, {659, 400}, {740, 400},
        {659, 400}, {659, 200}, {587, 200}, {587, 800},

        {659, 400}, {659, 400}, {740, 400}, {587, 400},
        {659, 400}, {740, 200}, {784, 200}, {740, 400}, {587, 400},
        {659, 400}, {740, 200}, {784, 200}, {740, 400}, {659, 400},
        {587, 400}, {659, 400}, {440, 400}, {440, 400},

        {740, 400}, {740, 600}, {784, 400}, {880, 400},
        {880, 400}, {784, 400}, {740, 400}, {659, 400},
        {587, 400}, {587, 400}, {659, 400}, {740, 400},
        {659, 400}, {659, 200}, {587, 200}, {587, 800},
    },
    .numNotes = 66,
    .shouldLoop = false
};

typedef struct
{
    uint16_t demoHue;
    qoi_t megaman[9];
    font_t tom_thumb;
    font_t ibm_vga8;
    font_t radiostars;
    p2pInfo p;
    display_t * disp;
    float temperature;
    accel_t accel;
    dft32_data dd;
    embeddednf_data end;
    embeddedout_data eod;
    uint8_t samplesProcessed;
    uint16_t maxValue;
    uint64_t packetTimer;
    uint16_t packetsRx;
} demo_t;

demo_t * demo;

swadgeMode modeDemo =
{
    .modeName = "Demo",
    .fnEnterMode = demoEnterMode,
    .fnExitMode = demoExitMode,
    .fnMainLoop = demoMainLoop,
    .fnButtonCallback = demoButtonCb,
    .fnTouchCallback = demoTouchCb,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = demoEspNowRecvCb,
    .fnEspNowSendCb = demoEspNowSendCb,
    .fnAccelerometerCallback = demoAccelerometerCb,
    .fnAudioCallback = demoAudioCb,
    .fnTemperatureCallback = demoTemperatureCb
};

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 *
 */
void demoEnterMode(display_t * disp)
{
    // Allocate memory for this mode
    demo = (demo_t *)malloc(sizeof(demo_t));
    memset(demo, 0, sizeof(demo_t));

    // Save a pointer to the display
    demo->disp = disp;

    // Test the buzzer, just once
    buzzer_play(&odeToJoy);

    // Test reading and writing NVR
#define MAGIC_VAL 0xAF
    int32_t magicVal = 0;
    if((false == readNvs32("magicVal", &magicVal)) || (MAGIC_VAL != magicVal))
    {
        ESP_LOGD("DEMO", "Writing magic val");
        writeNvs32("magicVal", MAGIC_VAL);
        writeNvs32("testVal", 0x01);
    }
    else
    {
        ESP_LOGD("DEMO", "Magic val read, 0x%02X", magicVal);
    }

    InitColorChord(&demo->end, &demo->dd);
    demo->maxValue = 1;

    // Load some QOIs
    loadQoi("run-1.qoi", &demo->megaman[0]);
    loadQoi("run-2.qoi", &demo->megaman[1]);
    loadQoi("run-3.qoi", &demo->megaman[2]);
    loadQoi("run-4.qoi", &demo->megaman[3]);
    loadQoi("run-5.qoi", &demo->megaman[4]);
    loadQoi("run-6.qoi", &demo->megaman[5]);
    loadQoi("run-7.qoi", &demo->megaman[6]);
    loadQoi("run-8.qoi", &demo->megaman[7]);
    loadQoi("run-9.qoi", &demo->megaman[8]);

    // Load some fonts
    loadFont("tom_thumb.font", &demo->tom_thumb);
    loadFont("ibm_vga8.font", &demo->ibm_vga8);
    loadFont("radiostars.font", &demo->radiostars);

    // Start a p2p connection
    p2pInitialize(&demo->p, "dmo", demoConCbFn, demoMsgRxCbFn, -10);
    p2pStartConnection(&demo->p);
}

/**
 * @brief TODO
 *
 */
void demoExitMode(void)
{
    for(uint8_t i = 0; i < (sizeof(demo->megaman) / sizeof(demo->megaman[0])); i++)
    {
        freeQoi(&demo->megaman[i]);
    }
    freeFont(&demo->tom_thumb);
    freeFont(&demo->ibm_vga8);
    freeFont(&demo->radiostars);

    p2pDeinit(&demo->p);

    free(demo);
}

/**
 * @brief TODO
 *
 * @param elapsedUs
 */
void demoMainLoop(int64_t elapsedUs)
{
    // Rotate through all the hues in two seconds
    /*
    static uint64_t ledTime = 0;
    ledTime += elapsedUs;
    if(ledTime >= (2000000/360))
    {
        ledTime -= (2000000/360);

        led_t leds[NUM_LEDS] = {0};
        for(int i = 0; i < NUM_LEDS; i++)
        {
            uint16_t tmpHue = (demo->demoHue + ((360/NUM_LEDS) * i)) % 360;
            led_strip_hsv2rgb(tmpHue, 100, 100, &leds[i].r, &leds[i].g, &leds[i].b);
        }
        demo->demoHue = (demo->demoHue + 1) % 360;
        setLeds(leds, NUM_LEDS);
    }
    */

    // Move megaman sometimes
    static int megaIdx = 0;
    static int megaPos = 0;
    static uint64_t megaTime = 0;
    megaTime += elapsedUs;
    if(megaTime >= 150000)
    {
        megaTime -= 150000;
        
        megaPos += 4;
        if(megaPos >= demo->disp->w)
        {
            megaPos = -demo->megaman[0].w;
        }
        megaIdx = (megaIdx + 1) % 9;
    }

    demo->disp->clearPx();

    // Draw the spectrum as a bar graph
    uint16_t mv = demo->maxValue;
    for(uint16_t i = 0; i < FIXBINS; i++) // 120
    {
        if(demo->end.fuzzed_bins[i] > demo->maxValue)
        {
            demo->maxValue = demo->end.fuzzed_bins[i];
        }
        uint8_t height = ((demo->disp->h - demo->ibm_vga8.h - 2) * demo->end.fuzzed_bins[i]) / mv;
        fillDisplayArea(demo->disp,
            i * 2,        demo->disp->h - height,
            (i + 1) * 2, (demo->disp->h - demo->ibm_vga8.h - 2),
            hsv2rgb(64 + (i * 2), 1, 1));
    }
    
    rgba_pixel_t pxCol = {
        .a = PX_OPAQUE,
        .r = 0x1F,
        .g = 0x00,
        .b = 0x00,
    };

    // Draw text
    drawText(demo->disp, &demo->radiostars, pxCol, "hello TFT", 10, 64);

    // Draw image
    drawQoi(demo->disp, &demo->megaman[megaIdx], megaPos, (demo->disp->h-demo->megaman[0].h)/2);

    // Draw a single white pixel in the middle of the display
    pxCol.r = 0x1F;
    pxCol.g = 0x1F;
    pxCol.b = 0x1F;
    demo->disp->setPx(
        demo->disp->w / 2,
        demo->disp->h / 2,
        pxCol);

    // Draw a yellow line
    pxCol.r = 0x1F;
    pxCol.g = 0x1F;
    pxCol.b = 0x00;
    plotLine(demo->disp, 10, 5, 50, 20, pxCol);

    // Draw a magenta rectangle
    pxCol.r = 0x1F;
    pxCol.g = 0x00;
    pxCol.b = 0x1F;
    plotRect(demo->disp, 70, 5, 100, 20, pxCol);

    // Draw a cyan circle
    pxCol.r = 0x00;
    pxCol.g = 0x1F;
    pxCol.b = 0x1F;
    plotCircle(demo->disp, 140, 30, 20, pxCol);

    // Draw temperature to display
    char tempStr[128];
    sprintf(tempStr, "%2.2f C", demo->temperature);
    uint16_t tWidth = textWidth(&(demo->ibm_vga8), tempStr);
    pxCol.r = 0x1F;
    pxCol.g = 0x00;
    pxCol.b = 0x1F;
    drawText(demo->disp, &(demo->ibm_vga8), pxCol, tempStr, demo->disp->w - tWidth, demo->disp->h - demo->ibm_vga8.h);

    // Draw acceleration to display
    char accelStr[128];
    sprintf(accelStr, "X: %3d, Y: %3d, Z: %3d", demo->accel.x, demo->accel.y, demo->accel.z);
    pxCol.r = 0x00;
    pxCol.g = 0x1F;
    pxCol.b = 0x1F;
    drawText(demo->disp, &(demo->ibm_vga8), pxCol, accelStr, 0, demo->disp->h - demo->ibm_vga8.h);

    // Twice a second push out some USB data
    static uint64_t usbTime = 0;
    usbTime += elapsedUs;
    if(usbTime >= 500000)
    {
        usbTime -= 500000;

        // Only send data if USB is ready
        if(tud_ready())
        {
            // Static variables to track button and hat position
            static hid_gamepad_button_bm_t btnPressed = GAMEPAD_BUTTON_A;
            static hid_gamepad_hat_t hatDir = GAMEPAD_HAT_CENTERED;

            // Build and send the state over USB
            hid_gamepad_report_t report =
            {
                .buttons = btnPressed,
                .hat = hatDir
            };
            tud_gamepad_report(&report);

            // Move to the next button
            btnPressed <<= 1;
            if(btnPressed > GAMEPAD_BUTTON_THUMBR)
            {
                btnPressed = GAMEPAD_BUTTON_A;
            }

            // Move to the next hat dir
            hatDir++;
            if(hatDir > GAMEPAD_HAT_UP_LEFT)
            {
                hatDir = GAMEPAD_HAT_CENTERED;
            }
        }
    }
}

/**
 * @brief TODO
 *
 * @param evt
 */
void demoButtonCb(buttonEvt_t* evt)
{
    ESP_LOGD("DEMO", "%s (%d %d %d)", __func__, evt->button, evt->down, evt->state);
}

/**
 * @brief TODO
 *
 * @param evt
 */
void demoTouchCb(touch_event_t* evt)
{
    ESP_LOGD("DEMO", "%s (%d %d %d)", __func__, evt->pad_num, evt->pad_status, evt->pad_val);
}

/**
 * @brief TODO
 *
 * @param accel
 */
void demoAccelerometerCb(accel_t* accel)
{
    demo->accel = *accel;
}

/**
 * @brief TODO
 * 
 * @param samples 
 * @param sampleCnt 
 */
void demoAudioCb(uint16_t * samples, uint32_t sampleCnt)
{
    bool ledsUpdated = false;
    for(uint32_t idx = 0; idx < sampleCnt; idx++)
    {
        PushSample32(&demo->dd, samples[idx]);

        demo->samplesProcessed++;
        if(!ledsUpdated && demo->samplesProcessed >= 128)
        {
            demo->samplesProcessed = 0;
            HandleFrameInfo(&demo->end, &demo->dd);
            UpdateAllSameLEDs(&demo->eod, &demo->end);
            setLeds((led_t*)demo->eod.ledOut, NUM_LEDS);
            ledsUpdated = true;
        }
    }
}
 
/**
 * @brief TODO
 *
 * @param tmp_c
 */
void demoTemperatureCb(float tmp_c)
{
    demo->temperature = tmp_c;
}

/**
 * @brief TODO
 *
 * @param mac_addr
 * @param data
 * @param len
 * @param rssi
 */
void demoEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi)
{
    p2pRecvCb(&demo->p, mac_addr, data, len, rssi);
}

/**
 * @brief TODO
 *
 * @param mac_addr
 * @param status
 */
void demoEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    p2pSendCb(&demo->p, mac_addr, status);
}

/**
 * @brief TODO
 *
 * @param p2p
 * @param evt
 */
void demoConCbFn(p2pInfo* p2p __attribute__((unused)), connectionEvt_t evt)
{
    switch(evt)
    {
        case CON_STARTED:
        {
            ESP_LOGI("DEMO", "%s :: CON_STARTED", __func__);
            break;
        }
        case RX_GAME_START_ACK:
        {
            ESP_LOGI("DEMO", "%s :: RX_GAME_START_ACK", __func__);
            break;
        }
        case RX_GAME_START_MSG:
        {
            ESP_LOGI("DEMO", "%s :: RX_GAME_START_MSG", __func__);
            break;
        }
        case CON_ESTABLISHED:
        {
            ESP_LOGI("DEMO", "%s :: CON_ESTABLISHED", __func__);
            switch(p2pGetPlayOrder(p2p))
            {
                default:
                case NOT_SET:
                case GOING_SECOND:
                {
                    break;
                }
                case GOING_FIRST:
                {
                    const char randPayload[] = "zb4o5LBYgsmDuyreOtBcPIi8kINXYW0";
                    p2pSendMsg(p2p, "gst", randPayload, sizeof(randPayload), demoMsgTxCbFn);
                    break;
                } 
            }
            break;
        }
        default:
        case CON_LOST:
        {
            ESP_LOGI("DEMO", "%s :: CON_LOST", __func__);
            p2pStartConnection(&demo->p);
            break;
        }
    }
}

/**
 * @brief TODO
 *
 * @param p2p
 * @param msg
 * @param payload
 * @param len
 */
void demoMsgRxCbFn(p2pInfo* p2p, const char* msg, const char* payload, uint8_t len)
{
    ESP_LOGD("DEMO", "%s -> [%d] -> %s", __func__, len, payload);

    // Echo
    p2pSendMsg(p2p, msg, payload, len, demoMsgTxCbFn);

    if(0 == demo->packetTimer)
    {
        demo->packetTimer = esp_timer_get_time();
    }
    demo->packetsRx++;
    if(100 == demo->packetsRx)
    {
        ESP_LOGI("DEMO", "100 packets in %llu", esp_timer_get_time() - demo->packetTimer);
        demo->packetsRx = 0;
        demo->packetTimer = 0;
    }
}

/**
 * @brief TODO
 *
 * @param p2p
 * @param status
 */
void demoMsgTxCbFn(p2pInfo* p2p __attribute__((unused)), messageStatus_t status)
{
    ESP_LOGD("DEMO", "%s :: %d", __func__, status);
}
