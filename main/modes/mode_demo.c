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
#include "QMA6981.h"
#include "esp_temperature_sensor.h"
#include "nvs_manager.h"
#include "display.h"
#include "led_util.h"
#include "p2pConnection.h"

#include "mode_demo.h"

//==============================================================================
// Functions Prototypes
//==============================================================================

void demoEnterMode(display_t * disp);
void demoExitMode(void);
void demoMainLoop(int64_t elapsedUs);
void demoAccelerometerCb(accel_t* accel);
void demoButtonCb(buttonEvt_t* evt);
void demoTouchCb(touch_event_t* evt);
void demoEspNowRecvCb(const uint8_t* mac_addr, const uint8_t* data, uint8_t len, int8_t rssi);
void demoEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status);

void demoConCbFn(p2pInfo* p2p, connectionEvt_t evt);
void demoMsgRxCbFn(p2pInfo* p2p, const char* msg, const uint8_t* payload, uint8_t len);
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
    .fnAccelerometerCallback = demoAccelerometerCb
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
        writeNvs32("magicVal", 0x01);
        writeNvs32("testVal", 0x01);
    }
    else
    {
        ESP_LOGD("DEMO", "Magic val read, 0x%02X", magicVal);
    }

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
    // Print temperature every second
    static uint64_t temperatureTime = 0;
    temperatureTime += elapsedUs;
    if(temperatureTime >= 1000000)
    {
        temperatureTime -= 1000000;
        ESP_LOGD("DEMO", "temperature %fc", readTemperatureSensor());
    }

    // Rotate through all the hues in two seconds
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

    // Draw to the TFT periodically
    static uint64_t tftTime = 0;
    tftTime += elapsedUs;
    if(tftTime >= 250000)
    {
        tftTime -= 250000;

        demo->disp->clearPx();

        rgba_pixel_t textColor = {
            .a = PX_OPAQUE,
            .r = 0x00,
            .g = 0x00,
            .b = 0x00,
        };

        textColor.r = 0x1F;
        drawText(demo->disp, &demo->tom_thumb, textColor, "hello TFT", 10, 64);
        textColor.r = 0x00;
        textColor.g = 0x1F;
        drawText(demo->disp, &demo->ibm_vga8, textColor, "hello TFT", 10, 84);
        textColor.g = 0x00;
        textColor.b = 0x1F;
        drawText(demo->disp, &demo->radiostars, textColor, "hello TFT", 10, 104);

        static int megaIdx = 0;
        static int megaPos = 0;

        drawQoi(demo->disp, &demo->megaman[megaIdx], megaPos, (demo->disp->h-demo->megaman[0].h)/2);

        megaPos += 4;
        if(megaPos >= demo->disp->w)
        {
            megaPos = -demo->megaman[0].w;
        }
        megaIdx = (megaIdx + 1) % 9;
    }

    // Twice a second push out some USB data
    static uint64_t usbTime = 0;
    usbTime += elapsedUs;
    // Don't send until USB is ready
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
    uint64_t cTimeUs = esp_timer_get_time();
    static uint64_t lastAccelPrint = 0;
    if(cTimeUs - lastAccelPrint >= 1000000)
    {
        lastAccelPrint = cTimeUs;
        ESP_LOGD("DEMO", "Accel %4d %4d %4d", accel->x, accel->y, accel->z);
    }
}

/**
 * @brief TODO
 * 
 * @param mac_addr 
 * @param data 
 * @param len 
 * @param rssi 
 */
void demoEspNowRecvCb(const uint8_t* mac_addr, const uint8_t* data, uint8_t len, int8_t rssi)
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
void demoConCbFn(p2pInfo* p2p, connectionEvt_t evt)
{
    ESP_LOGD("DEMO", "%s :: %d", __func__, evt);
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param msg 
 * @param payload 
 * @param len 
 */
void demoMsgRxCbFn(p2pInfo* p2p, const char* msg, const uint8_t* payload, uint8_t len)
{
    ESP_LOGD("DEMO", "%s :: %d", __func__, len);
}

/**
 * @brief TODO
 * 
 * @param p2p 
 * @param status 
 */
void demoMsgTxCbFn(p2pInfo* p2p, messageStatus_t status)
{
    ESP_LOGD("DEMO", "%s :: %d", __func__, status);
}
