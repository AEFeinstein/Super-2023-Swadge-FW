//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>

#include <esp_log.h>
#include <esp_timer.h>

#include "settingsManager.h"
#include "embeddedout.h"
#include "bresenham.h"
#include "musical_buzzer.h"
#include "led_util.h"
#include "swadge_util.h"

#include "mode_test.h"
#include "mode_main_menu.h"

//==============================================================================
// Defines
//==============================================================================

#define BTN_RADIUS 10
#define BTN_DIA (BTN_RADIUS * 2)
#define AB_SEP 6
#define DPAD_SEP 15

#define ACCEL_BAR_H      8
#define ACCEL_BAR_SEP    1
#define MAX_ACCEL_BAR_W 60

#define TOUCHBAR_WIDTH  100
#define TOUCHBAR_HEIGHT  20
#define TOUCHBAR_Y_OFF   32

//==============================================================================
// Enums
//==============================================================================

typedef enum
{
    BTN_NOT_PRESSED,
    BTN_PRESSED,
    BTN_RELEASED
} testButtonState_t;

typedef enum
{
    R_RISING,
    R_FALLING,
    G_RISING,
    G_FALLING,
    B_RISING,
    B_FALLING
} testLedState_t;

//==============================================================================
// Functions Prototypes
//==============================================================================

void testEnterMode(display_t* disp);
void testExitMode(void);
void testMainLoop(int64_t elapsedUs);
void testAudioCb(uint16_t* samples, uint32_t sampleCnt);
void testButtonCb(buttonEvt_t* evt);
void testTouchCb(touch_event_t* evt);
void testAccelerometerCallback(accel_t* accel);

void plotButtonState(int16_t x, int16_t y, testButtonState_t state);

//==============================================================================
// Variables
//==============================================================================

typedef struct
{
    font_t ibm_vga8;
    display_t* disp;
    wsg_t kd_idle0;
    wsg_t kd_idle1;
    uint64_t tSpriteElapsedUs;
    uint8_t spriteFrame;
    // Microphone test
    dft32_data dd;
    embeddednf_data end;
    embeddedout_data eod;
    uint8_t samplesProcessed;
    uint16_t maxValue;
    // Button
    testButtonState_t buttonStates[8];
    // Touch
    testButtonState_t touchStates[5];
    // Accelerometer
    accel_t accel;
    // LED
    led_t cLed;
    testLedState_t ledState;
    uint64_t tLedElapsedUs;
    // Test results
    bool buttonsPassed;
    bool touchPassed;
    bool accelPassed;
    bool bzrMicPassed;
} test_t;

test_t* test;

swadgeMode modeTest =
{
    .modeName = "Test",
    .fnEnterMode = testEnterMode,
    .fnExitMode = testExitMode,
    .fnMainLoop = testMainLoop,
    .fnButtonCallback = testButtonCb,
    .fnTouchCallback = testTouchCb,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = testAccelerometerCallback,
    .fnAudioCallback = testAudioCb,
    .fnTemperatureCallback = NULL,
    .overrideUsb = false
};

const song_t BlackDog =
{
    .notes = {
        {.note = E_5, .timeMs = 188},
        {.note = G_5, .timeMs = 188},
        {.note = G_SHARP_5, .timeMs = 188},
        {.note = A_5, .timeMs = 188},
        {.note = E_5, .timeMs = 188},
        {.note = C_6, .timeMs = 375},
        {.note = A_5, .timeMs = 375},
        {.note = D_6, .timeMs = 188},
        {.note = E_6, .timeMs = 188},
        {.note = C_6, .timeMs = 94},
        {.note = D_6, .timeMs = 94},
        {.note = C_6, .timeMs = 188},
        {.note = A_5, .timeMs = 183},
        {.note = SILENCE, .timeMs = 10},
        {.note = A_5, .timeMs = 183},
        {.note = C_6, .timeMs = 375},
        {.note = A_5, .timeMs = 375},
        {.note = G_5, .timeMs = 188},
        {.note = A_5, .timeMs = 183},
        {.note = SILENCE, .timeMs = 10},
        {.note = A_5, .timeMs = 183},
        {.note = D_5, .timeMs = 188},
        {.note = E_5, .timeMs = 188},
        {.note = C_5, .timeMs = 188},
        {.note = D_5, .timeMs = 188},
        {.note = A_4, .timeMs = 370},
        {.note = SILENCE, .timeMs = 10},
        {.note = A_4, .timeMs = 745},
    },
    .numNotes = 28,
    .shouldLoop = true
};

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Enter test mode, allocate memory, initialize code
 */
void testEnterMode(display_t* disp)
{
    // Allocate memory for this mode
    test = (test_t*)calloc(1, sizeof(test_t));

    // Save a pointer to the display
    test->disp = disp;

    // Load a font
    loadFont("ibm_vga8.font", &test->ibm_vga8);

    // Load a sprite
    loadWsg("kid0.wsg", &test->kd_idle0);
    loadWsg("kid1.wsg", &test->kd_idle1);

    // Init CC
    InitColorChord(&test->end, &test->dd);
    test->maxValue = 1;

    // Set the mic to listen
    setBgmIsMuted(false);
    setSfxIsMuted(false);
    setMicGain(7);

    // Play a song
    buzzer_play_bgm(&BlackDog);
}

/**
 * @brief Exit test mode, free memory
 */
void testExitMode(void)
{
    freeFont(&test->ibm_vga8);
    freeWsg(&test->kd_idle0);
    freeWsg(&test->kd_idle1);
    free(test);
}

/**
 * @brief This is the main loop, and it draws to the TFT
 *
 * @param elapsedUs unused
 */
void testMainLoop(int64_t elapsedUs __attribute__((unused)))
{
    // Check for a test pass
    if(test->buttonsPassed &&
            test->touchPassed &&
            test->accelPassed &&
            test->bzrMicPassed)
    {
        // Set NVM to indicate the test passed
        setTestModePassed(true);

        if(getTestModePassed())
        {
            switchToSwadgeMode(&modeMainMenu);
        }
    }

    // Clear everything
    test->disp->clearPx();

    // Draw the spectrum as a bar graph. Figure out bar and margin size
    int16_t binWidth = (test->disp->w / FIXBINS);
    int16_t binMargin = (test->disp->w - (binWidth * FIXBINS)) / 2;

    // Find the max value
    for(uint16_t i = 0; i < FIXBINS; i++)
    {
        if(test->end.fuzzed_bins[i] > test->maxValue)
        {
            test->maxValue = test->end.fuzzed_bins[i];
        }
    }

    // Plot the bars
    int32_t energy = 0;
    for(uint16_t i = 0; i < FIXBINS; i++)
    {
        energy += test->end.fuzzed_bins[i];
        uint8_t height = ((test->disp->h / 2) * test->end.fuzzed_bins[i]) /
                         test->maxValue;
        paletteColor_t color = test->bzrMicPassed ? c050 : c500; //paletteHsvToHex((i * 256) / FIXBINS, 255, 255);
        int16_t x0 = binMargin + (i * binWidth);
        int16_t x1 = binMargin + ((i + 1) * binWidth);
        // Big enough, fill an area
        fillDisplayArea(test->disp,
                        x0, test->disp->h - height,
                        x1, test->disp->h, color);
    }

    // Check for a pass
    if(energy > 100000)
    {
        test->bzrMicPassed = true;
    }

    // Draw button states
    int16_t centerLine = test->disp->h / 4;
    int16_t btnX = BTN_DIA;

    // LEFT
    plotButtonState(btnX, centerLine, test->buttonStates[2]);
    btnX += DPAD_SEP;
    // UP
    plotButtonState(btnX, centerLine - DPAD_SEP, test->buttonStates[0]);
    // DOWN
    plotButtonState(btnX, centerLine + DPAD_SEP, test->buttonStates[1]);
    btnX += DPAD_SEP;
    // RIGHT
    plotButtonState(btnX, centerLine, test->buttonStates[3]);
    btnX += BTN_DIA + 4;

    // SELECT
    plotButtonState(btnX, centerLine, test->buttonStates[7]);
    btnX += BTN_DIA + 2;
    // START
    plotButtonState(btnX, centerLine, test->buttonStates[6]);
    btnX += BTN_DIA + 4;

    // BTN_B
    plotButtonState(btnX, centerLine + AB_SEP, test->buttonStates[5]);
    btnX += BTN_DIA + 2;
    // BTN_A
    plotButtonState(btnX, centerLine - AB_SEP, test->buttonStates[4]);

    // Draw touch strip
    int16_t tBarX = test->disp->w - TOUCHBAR_WIDTH;
    uint8_t numTouchElem = (sizeof(test->touchStates) / sizeof(test->touchStates[0]));
    for(uint8_t touchIdx = 0; touchIdx < numTouchElem; touchIdx++)
    {
        switch(test->touchStates[touchIdx])
        {
            case BTN_NOT_PRESSED:
            {
                plotRect(test->disp,
                         tBarX - 1, TOUCHBAR_Y_OFF,
                         tBarX + (TOUCHBAR_WIDTH / numTouchElem), TOUCHBAR_Y_OFF + TOUCHBAR_HEIGHT,
                         c500);
                break;
            }
            case BTN_PRESSED:
            {
                fillDisplayArea(test->disp,
                                tBarX - 1, TOUCHBAR_Y_OFF,
                                tBarX + (TOUCHBAR_WIDTH / numTouchElem), TOUCHBAR_Y_OFF + TOUCHBAR_HEIGHT,
                                c005);
                break;
            }
            case BTN_RELEASED:
            {
                fillDisplayArea(test->disp,
                                tBarX - 1, TOUCHBAR_Y_OFF,
                                tBarX + (TOUCHBAR_WIDTH / numTouchElem), TOUCHBAR_Y_OFF + TOUCHBAR_HEIGHT,
                                c050);
                break;
            }
        }
        tBarX += (TOUCHBAR_WIDTH / numTouchElem);
    }

    // Set up drawing accel bars
    int16_t barY = TOUCHBAR_Y_OFF + TOUCHBAR_HEIGHT + 4;

    paletteColor_t accelColor = c500;
    if(test->accelPassed)
    {
        accelColor = c050;
    }

    // Plot X accel
    int16_t barWidth = ((test->accel.x + 8192) * MAX_ACCEL_BAR_W) / 16384;
    fillDisplayArea(test->disp, test->disp->w - barWidth, barY, test->disp->w, barY + ACCEL_BAR_H, accelColor);
    barY += (ACCEL_BAR_H + ACCEL_BAR_SEP);

    // Plot Y accel
    barWidth = ((test->accel.y + 8192) * MAX_ACCEL_BAR_W) / 16384;
    fillDisplayArea(test->disp, test->disp->w - barWidth, barY, test->disp->w, barY + ACCEL_BAR_H, accelColor);
    barY += (ACCEL_BAR_H + ACCEL_BAR_SEP);

    // Plot Z accel
    barWidth = ((test->accel.z + 8192) * MAX_ACCEL_BAR_W) / 16384;
    fillDisplayArea(test->disp, test->disp->w - barWidth, barY, test->disp->w, barY + ACCEL_BAR_H, accelColor);
    barY += (ACCEL_BAR_H + ACCEL_BAR_SEP);

    // Plot some text depending on test status
    char dbgStr[32] = {0};
    if(false == test->buttonsPassed)
    {
        sprintf(dbgStr, "Test Buttons");
    }
    else if(false == test->touchPassed)
    {
        sprintf(dbgStr, "Test Touch Strip");
    }
    else if(false == test->accelPassed)
    {
        sprintf(dbgStr, "Test Accelerometer");
    }
    else if(false == test->bzrMicPassed)
    {
        sprintf(dbgStr, "Test Buzzer & Mic");
    }
    else if (getTestModePassed())
    {
        sprintf(dbgStr, "All Tests Passed");
    }

    int16_t tWidth = textWidth(&test->ibm_vga8, dbgStr);
    drawText(test->disp, &test->ibm_vga8, c555, dbgStr, (test->disp->w - tWidth) / 2, 0);

    sprintf(dbgStr, "Verify RGB LEDs & Song");
    tWidth = textWidth(&test->ibm_vga8, dbgStr);
    drawText(test->disp, &test->ibm_vga8, c555, dbgStr, 70, test->ibm_vga8.h + 8);

    // Animate a sprite
    test->tSpriteElapsedUs += elapsedUs;
    while(test->tSpriteElapsedUs >= 500000)
    {
        test->tSpriteElapsedUs -= 500000;
        test->spriteFrame = (test->spriteFrame + 1) % 2;
    }

    // Draw the sprite
    if(0 == test->spriteFrame)
    {
        drawWsg(test->disp, &test->kd_idle0, 32, 4, false, false, 0);
    }
    else
    {
        drawWsg(test->disp, &test->kd_idle1, 32, 4, false, false, 0);
    }

    // Pulse LEDs, each color for 1s
    test->tLedElapsedUs += elapsedUs;
    while(test->tLedElapsedUs >= (1000000 / 512))
    {
        test->tLedElapsedUs -= (1000000 / 512);
        switch(test->ledState)
        {
            case R_RISING:
            {
                test->cLed.r++;
                if(255 == test->cLed.r)
                {
                    test->ledState = R_FALLING;
                }
                break;
            }
            case R_FALLING:
            {
                test->cLed.r--;
                if(0 == test->cLed.r)
                {
                    test->ledState = G_RISING;
                }
                break;
            }
            case G_RISING:
            {
                test->cLed.g++;
                if(255 == test->cLed.g)
                {
                    test->ledState = G_FALLING;
                }
                break;
            }
            case G_FALLING:
            {
                test->cLed.g--;
                if(0 == test->cLed.g)
                {
                    test->ledState = B_RISING;
                }
                break;
            }
            case B_RISING:
            {
                test->cLed.b++;
                if(255 == test->cLed.b)
                {
                    test->ledState = B_FALLING;
                }
                break;
            }
            case B_FALLING:
            {
                test->cLed.b--;
                if(0 == test->cLed.b)
                {
                    test->ledState = R_RISING;
                }
                break;
            }
        }
    }

    // Display LEDs
    led_t leds[NUM_LEDS] = {0};
    for(uint8_t i = 0; i < NUM_LEDS; i++)
    {
        leds[i].r = test->cLed.r;
        leds[i].g = test->cLed.g;
        leds[i].b = test->cLed.b;
    }
    setLeds(leds, NUM_LEDS);
}

/**
 * @brief Helper function to draw button statuses to the TFT
 *
 * @param x
 * @param y
 * @param state
 */
void plotButtonState(int16_t x, int16_t y, testButtonState_t state)
{
    switch(state)
    {
        case BTN_NOT_PRESSED:
        {
            plotCircle(test->disp, x, y, BTN_RADIUS, c500);
            break;
        }
        case BTN_PRESSED:
        {
            plotCircleFilled(test->disp, x, y, BTN_RADIUS, c005);
            break;
        }
        case BTN_RELEASED:
        {
            plotCircleFilled(test->disp, x, y, BTN_RADIUS, c050);
            break;
        }
    }
}

/**
 * @brief Button callback, used to change settings
 *
 * @param evt The button event
 */
void testButtonCb(buttonEvt_t* evt)
{
    // Find the button index based on bit
    uint8_t btnIdx = 0;
    switch(evt->button)
    {
        case UP:
        {
            btnIdx = 0;
            break;
        }
        case DOWN:
        {
            btnIdx = 1;
            break;
        }
        case LEFT:
        {
            btnIdx = 2;
            break;
        }
        case RIGHT:
        {
            btnIdx = 3;
            break;
        }
        case BTN_A:
        {
            btnIdx = 4;
            break;
        }
        case BTN_B:
        {
            btnIdx = 5;
            break;
        }
        case START:
        {
            btnIdx = 6;
            break;
        }
        case SELECT:
        {
            btnIdx = 7;
            break;
        }
        default:
        {
            return;
        }
    }

    // Transition states
    if(evt->down)
    {
        if(BTN_NOT_PRESSED == test->buttonStates[btnIdx])
        {
            test->buttonStates[btnIdx] = BTN_PRESSED;
        }
    }
    else
    {
        if(BTN_PRESSED == test->buttonStates[btnIdx])
        {
            test->buttonStates[btnIdx] = BTN_RELEASED;
        }
    }

    // Check if all buttons have passed
    test->buttonsPassed = true;
    for(uint8_t i = 0; i < 8; i++)
    {
        if(BTN_RELEASED != test->buttonStates[i])
        {
            test->buttonsPassed = false;
        }
    }
}

/**
 * @brief Touch callback
 *
 * @param evt the touch event
 */
void testTouchCb(touch_event_t* evt)
{
    // Transition states
    if(evt->down)
    {
        if(BTN_NOT_PRESSED == test->touchStates[evt->pad])
        {
            test->touchStates[evt->pad] = BTN_PRESSED;
        }
    }
    else
    {
        if(BTN_PRESSED == test->touchStates[evt->pad])
        {
            test->touchStates[evt->pad] = BTN_RELEASED;
        }
    }

    // Check if all buttons have passed
    test->touchPassed = true;
    for(uint8_t i = 0; i < 5; i++)
    {
        if(BTN_RELEASED != test->touchStates[i])
        {
            test->touchPassed = false;
        }
    }
}

/**
 * @brief Audio callback. Take the samples and pass them to test
 *
 * @param samples The samples to process
 * @param sampleCnt The number of samples to process
 */
void testAudioCb(uint16_t* samples, uint32_t sampleCnt)
{
    // For each sample
    for(uint32_t idx = 0; idx < sampleCnt; idx++)
    {
        // Push to test
        PushSample32(&test->dd, samples[idx]);

        // If 128 samples have been pushed
        test->samplesProcessed++;
        if(test->samplesProcessed >= 128)
        {
            // Update LEDs
            test->samplesProcessed = 0;
            HandleFrameInfo(&test->end, &test->dd);

            // Keep track of max value for the spectrogram
            int16_t maxVal = 0;
            for(uint16_t i = 0; i < FIXBINS; i++)
            {
                if(test->end.fuzzed_bins[i] > maxVal)
                {
                    maxVal = test->end.fuzzed_bins[i];
                }
            }

            // If already passed, just return
            if(true == test->bzrMicPassed)
            {
                return;
            }
        }
    }
}

/**
 * @brief Callback for the accelerometer. Save and validate data
 *
 * @param accel The acceleration vector
 */
void testAccelerometerCallback(accel_t* accel)
{
    // Save accel values
    test->accel.x = accel->x;
    test->accel.y = accel->y;
    test->accel.z = accel->z;

    // Make sure all values are nonzero
    if((accel->x != 0) && (accel->y != 0) && (accel->z != 0))
    {
        // Make sure some value is shook
        if((accel->x > 4096) || (accel->y > 4096) || (accel->z > 4096))
        {
            // Pass!
            test->accelPassed = true;
        }
    }
}
