//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <esp_log.h>

#include "led_util.h"
#include "swadgeMode.h"
#include "mode_colorchord.h"

// For colorchord
#include "embeddedout.h"

//==============================================================================
// Functions Prototypes
//==============================================================================

void colorchordEnterMode(display_t* disp);
void colorchordExitMode(void);
void colorchordMainLoop(int64_t elapsedUs);
void colorchordAudioCb(uint16_t* samples, uint32_t sampleCnt);
void colorchordButtonCb(buttonEvt_t* evt);

//==============================================================================
// Variables
//==============================================================================

typedef struct
{
    font_t ibm_vga8;
    display_t* disp;
    dft32_data dd;
    embeddednf_data end;
    embeddedout_data eod;
    uint8_t samplesProcessed;
    uint16_t maxValue;
} colorchord_t;

colorchord_t* colorchord;

swadgeMode modeColorchord =
{
    .modeName = "Colorchord",
    .fnEnterMode = colorchordEnterMode,
    .fnExitMode = colorchordExitMode,
    .fnMainLoop = colorchordMainLoop,
    .fnButtonCallback = colorchordButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = colorchordAudioCb,
    .fnTemperatureCallback = NULL
};

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Enter colorchord mode, allocate memory, initialize code
 */
void colorchordEnterMode(display_t* disp)
{
    // Allocate memory for this mode
    colorchord = (colorchord_t*)calloc(1, sizeof(colorchord_t));

    // Save a pointer to the display
    colorchord->disp = disp;

    // Load a font
    loadFont("ibm_vga8.font", &colorchord->ibm_vga8);

    // Init CC
    InitColorChord(&colorchord->end, &colorchord->dd);
    colorchord->maxValue = 1;
}

/**
 * @brief Exit colorchord mode, free memory
 */
void colorchordExitMode(void)
{
    freeFont(&colorchord->ibm_vga8);
    free(colorchord);
}

/**
 * @brief This is the main loop, and it draws to the TFT
 *
 * @param elapsedUs unused
 */
void colorchordMainLoop(int64_t elapsedUs __attribute__((unused)))
{
    // Clear everything
    colorchord->disp->clearPx();

    // Draw the spectrum as a bar graph

    int16_t binWidth = (colorchord->disp->w / FIXBINS);
    int16_t binMargin = (colorchord->disp->w - (binWidth * FIXBINS)) / 2;

    uint16_t mv = colorchord->maxValue;
    for(uint16_t i = 0; i < FIXBINS; i++)
    {
        if(colorchord->end.fuzzed_bins[i] > colorchord->maxValue)
        {
            colorchord->maxValue = colorchord->end.fuzzed_bins[i];
        }
        uint8_t height = ((colorchord->disp->h - colorchord->ibm_vga8.h - 2) * colorchord->end.fuzzed_bins[i]) / mv;
        fillDisplayArea(colorchord->disp,
                        binMargin + (i * binWidth),        colorchord->disp->h - height,
                        binMargin + ((i + 1) * binWidth), (colorchord->disp->h),
                        hsv2rgb((i * 256) / FIXBINS, 255, 255));
    }
}

/**
 * @brief Button callback, used to change settings
 *
 * @param evt The button event
 */
void colorchordButtonCb(buttonEvt_t* evt)
{
    if(evt->down)
    {
        switch(evt->button)
        {
            case UP:
            case DOWN:
            case LEFT:
            case RIGHT:
            case START:
            case SELECT:
            case BTN_A:
            case BTN_B:
            {
                // TODO stuff
                break;
            }
        }
    }
}

/**
 * @brief Audio callback. Take the samples and pass them to colorchord
 *
 * @param samples The samples to process
 * @param sampleCnt The number of samples to process
 */
void colorchordAudioCb(uint16_t* samples, uint32_t sampleCnt)
{
    // For each sample
    for(uint32_t idx = 0; idx < sampleCnt; idx++)
    {
        // Push to colorchord
        PushSample32(&colorchord->dd, samples[idx]);

        // If 128 samples have been pushed
        colorchord->samplesProcessed++;
        if(colorchord->samplesProcessed >= 128)
        {
            // Update LEDs
            colorchord->samplesProcessed = 0;
            HandleFrameInfo(&colorchord->end, &colorchord->dd);
            UpdateAllSameLEDs(&colorchord->eod, &colorchord->end); // TODO switch mode
            setLeds((led_t*)colorchord->eod.ledOut, NUM_LEDS);
        }
    }
}
