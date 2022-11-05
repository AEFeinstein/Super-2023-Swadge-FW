/*
 * mode_tunernome.c
 *
 *  Created on: September 17th, 2020
 *      Author: bryce
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "bresenham.h"
#include "display.h"
#include "embeddednf.h"
#include "embeddedout.h"
#include "esp_timer.h"
#include "led_util.h"
#include "linked_list.h"
#include "mode_main_menu.h"
#include "musical_buzzer.h"
#include "swadgeMode.h"
#include "settingsManager.h"

#include "mode_tunernome.h"

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define CORNER_OFFSET 12

#define NUM_GUITAR_STRINGS    lengthof(guitarNoteNames)
#define NUM_VIOLIN_STRINGS    lengthof(violinNoteNames)
#define NUM_UKULELE_STRINGS   lengthof(ukuleleNoteNames)
#define NUM_BANJO_STRINGS     lengthof(banjoNoteNames)
#define GUITAR_OFFSET         0
#define CHROMATIC_OFFSET      6 // adjust start point by quartertones
#define SENSITIVITY           5
#define TONAL_DIFF_IN_TUNE_DEVIATION 10

#define METRONOME_CENTER_X    tunernome->disp->w / 2
#define METRONOME_CENTER_Y    tunernome->disp->h - 16 - CORNER_OFFSET
#define METRONOME_RADIUS      135
#define TUNER_RADIUS          80
#define TUNER_TEXT_Y_OFFSET   30
#define TUNER_ARROW_Y_OFFSET  8
#define INITIAL_BPM           120
#define MAX_BPM               400
#define MAX_BEATS             16 // I mean, realistically, who's going to have more than 16 beats in a measure?
#define METRONOME_FLASH_MS    35
#define METRONOME_CLICK_MS    35
#define BPM_CHANGE_FIRST_MS   500
#define BPM_CHANGE_FAST_MS    2000
#define BPM_CHANGE_REPEAT_MS  50

/// Helper macro to return an integer clamped within a range (MIN to MAX)
#define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
/// Helper macro to return the absolute value of an integer
#define ABS(X) (((X) < 0) ? -(X) : (X))
/// Helper macro to return the highest of two integers
// #define MAX(X, Y) ( ((X) > (Y)) ? (X) : (Y) )
#define lengthof(x) (sizeof(x) / sizeof(x[0]))

#define NUM_SEMITONES 12

typedef enum
{
    TN_TUNER,
    TN_METRONOME
} tnMode;

typedef enum
{
    GUITAR_TUNER = 0,
    VIOLIN_TUNER,
    UKULELE_TUNER,
    BANJO_TUNER,
    SEMITONE_0,
    SEMITONE_1,
    SEMITONE_2,
    SEMITONE_3,
    SEMITONE_4,
    SEMITONE_5,
    SEMITONE_6,
    SEMITONE_7,
    SEMITONE_8,
    SEMITONE_9,
    SEMITONE_10,
    SEMITONE_11,
    LISTENING,
    MAX_GUITAR_MODES
} tuner_mode_t;

typedef struct
{
    tnMode mode;
    tuner_mode_t curTunerMode;

    display_t* disp;
    font_t ibm_vga8;
    font_t radiostars;
    font_t mm;

    buttonBit_t lastBpmButton;
    uint32_t bpmButtonCurChangeUs;
    uint32_t bpmButtonStartUs;
    uint32_t bpmButtonAccumulatedUs;

    dft32_data dd;
    embeddednf_data end;
    embeddedout_data eod;
    int audioSamplesProcessed;
    uint32_t intensities_filt[NUM_LEDS];
    int32_t diffs_filt[NUM_LEDS];

    uint8_t beatCtr;
    int bpm;
    int32_t tAccumulatedUs;
    bool isClockwise;
    int32_t usPerBeat;
    uint8_t beatLength;

    uint32_t semitone_intensity_filt[NUM_SEMITONES];
    int32_t semitone_diff_filt[NUM_SEMITONES];
    int16_t tonalDiff[NUM_SEMITONES];
    int16_t intensity[NUM_SEMITONES];

    wsg_t radiostarsArrowWsg;
    wsg_t mmArrowWsg;
    wsg_t flatWsg;

    uint32_t blinkStartUs;
    uint32_t blinkAccumulatedUs;
    bool isBlinking;
} tunernome_t;

typedef struct
{
    uint8_t top;
    uint8_t bottom;
} timeSignature;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void tunernomeEnterMode(display_t* disp);
void tunernomeExitMode(void);
void switchToSubmode(tnMode);
void tunernomeButtonCallback(buttonEvt_t* evt);
void modifyBpm(int16_t bpmMod);
void tunernomeSampleHandler(uint16_t* samples, uint32_t sampleCnt);
void recalcMetronome(void);
void plotInstrumentNameAndNotes(const char* instrumentName, const char** instrumentNotes,
                                uint16_t numNotes);
void plotTopSemiCircle(int xm, int ym, int r, paletteColor_t col);
void instrumentTunerMagic(const uint16_t freqBinIdxs[], uint16_t numStrings, led_t colors[],
                          const uint16_t stringIdxToLedIdx[]);
void tunernomeMainLoop(int64_t elapsedUs);
void ledReset(void* timer_arg);
void fasterBpmChange(void* timer_arg);

static inline int16_t getMagnitude(uint16_t idx);
static inline int16_t getDiffAround(uint16_t idx);
static inline int16_t getSemiMagnitude(int16_t idx);
static inline int16_t getSemiDiffAround(uint16_t idx);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode modeTunernome =
{
    .modeName = "Tunernome",
    .fnEnterMode = tunernomeEnterMode,
    .fnExitMode = tunernomeExitMode,
    .fnButtonCallback = tunernomeButtonCallback,
    .fnMainLoop = tunernomeMainLoop,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = tunernomeSampleHandler,
    .overrideUsb = false
};

tunernome_t* tunernome;

/*============================================================================
 * Const Variables
 *==========================================================================*/

/**
 * Indicies into fuzzed_bins[], a realtime DFT of sorts
 * fuzzed_bins[0] = A ... 1/2 steps are every 2.
 */
const uint16_t freqBinIdxsGuitar[] =
{
    38, // E string needs to skip an octave... Can't read sounds this low.
    24, // A string is exactly at note #24
    34, // D = A + 5 half steps = 34
    44, // G
    52, // B
    62  // e
};

/**
 * Indicies into fuzzed_bins[], a realtime DFT of sorts
 * fuzzed_bins[0] = A ... 1/2 steps are every 2.
 */
const uint16_t freqBinIdxsViolin[] =
{
    44, // G
    58, // D
    72, // A
    86  // E
};

/**
 * Indicies into fuzzed_bins[], a realtime DFT of sorts
 * fuzzed_bins[0] = A ... 1/2 steps are every 2.
 */
const uint16_t freqBinIdxsUkulele[] =
{
    68, // G
    54, // C
    62, // E
    72  // A
};

/**
 * Indicies into fuzzed_bins[], a realtime DFT of sorts
 * fuzzed_bins[0] = A ... 1/2 steps are every 2.
 */
const uint16_t freqBinIdxsBanjo[] =
{
    68, // G
    34, // D = A + 5 half steps = 34
    44, // G
    52, // B
    58  // D
};

const uint16_t fourNoteStringIdxToLedIdx[] =
{
    2,
    3,
    4,
    5
};

const uint16_t fiveNoteStringIdxToLedIdx[] =
{
    1,
    2,
    3,
    4,
    5
};

const uint16_t sixNoteStringIdxToLedIdx[] =
{
    1,
    2,
    3,
    4,
    5,
    6
};

const uint16_t twoLedFlashIdxs[] =
{
    0,
    7
};

const uint16_t fourLedFlashIdxs[] =
{
    1,
    3,
    4,
    6
};

const char* guitarNoteNames[] =
{
    "E2",
    "A2",
    "D3",
    "G3",
    "B3",
    "E4"
};

const char* violinNoteNames[] =
{
    "G3",
    "D4",
    "A4",
    "E5"
};

const char* ukuleleNoteNames[] =
{
    "G4",
    "C4",
    "E4",
    "A4"
};

const char* banjoNoteNames[] =
{
    "G4",
    "D3",
    "G3",
    "B3",
    "D4"
};

// End a string ending with "\1" to draw the flat symbol
const char* semitoneNoteNames[] =
{
    "C",
    "C#/D\1",
    "D",
    "D#/E\1",
    "E",
    "F",
    "F#/G\1",
    "G",
    "G#/A\1",
    "A",
    "A#/B\1",
    "B"
};

static const char theWordGuitar[] = "Guitar";
static const char theWordViolin[] = "Violin";
static const char theWordUkulele[] = "Ukulele";
static const char theWordBanjo[] = "Banjo";
static const char playStringsText[] = "Play all open strings";
static const char listeningText[] = "Listening for a note";
static const char leftStr[] = ": ";
static const char rightStrTuner[] = "Start: Tuner";
static const char rightStrMetronome[] = "Start: Metronome";

// TODO: these should be const after being assigned
static int TUNER_FLAT_THRES_X;
static int TUNER_SHARP_THRES_X;
static int TUNER_THRES_Y;

const song_t metronome_primary =
{
    .notes =
    {
        {F_SHARP_5, METRONOME_CLICK_MS}
    },
    .numNotes = 1,
    .shouldLoop = false
};

const song_t metronome_secondary =
{
    .notes =
    {
        {F_SHARP_4, METRONOME_CLICK_MS}
    },
    .numNotes = 1,
    .shouldLoop = false
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for tunernome
 */
void tunernomeEnterMode(display_t* disp)
{
    // Allocate zero'd memory for the mode
    tunernome = calloc(1, sizeof(tunernome_t));

    tunernome->disp = disp;

    loadFont("ibm_vga8.font", &tunernome->ibm_vga8);
    loadFont("radiostars.font", &tunernome->radiostars);
    loadFont("mm.font", &tunernome->mm);

    float intermedX = cosf(TONAL_DIFF_IN_TUNE_DEVIATION * M_PI / 17 );
    float intermedY = sinf(TONAL_DIFF_IN_TUNE_DEVIATION * M_PI / 17 );
    TUNER_SHARP_THRES_X = round(METRONOME_CENTER_X - (intermedX * TUNER_RADIUS));
    TUNER_FLAT_THRES_X = round(METRONOME_CENTER_X + (intermedX * TUNER_RADIUS));
    TUNER_THRES_Y = round(METRONOME_CENTER_Y - (ABS(intermedY) * TUNER_RADIUS));

    loadWsg("arrow12.wsg", &(tunernome->radiostarsArrowWsg));
    loadWsg("arrow21.wsg", &(tunernome->mmArrowWsg));
    loadWsg("flat_mm.wsg", &(tunernome->flatWsg));

    tunernome->beatLength = 4;
    tunernome->beatCtr = 0;
    tunernome->bpm = INITIAL_BPM;
    tunernome->curTunerMode = GUITAR_TUNER;

    switchToSubmode(TN_TUNER);

    InitColorChord(&tunernome->end, &tunernome->dd);

    tunernome->blinkStartUs = 0;
    tunernome->blinkAccumulatedUs = 0;
    tunernome->isBlinking = false;
}

/**
 * Switch internal mode
 */
void switchToSubmode(tnMode newMode)
{
    switch(newMode)
    {
        case TN_TUNER:
        {
            tunernome->mode = newMode;

            buzzer_stop();

            led_t leds[NUM_LEDS] = {{0}};
            setLeds(leds, NUM_LEDS);

            tunernome->disp->clearPx();

            break;
        }
        case TN_METRONOME:
        {
            tunernome-> mode = newMode;

            tunernome->isClockwise = true;
            tunernome->beatCtr = tunernome->beatLength - 1; // This assures that the first beat is a primary beat/downbeat
            tunernome->tAccumulatedUs = 0;

            tunernome->lastBpmButton = 0;
            tunernome->bpmButtonCurChangeUs = 0;
            tunernome->bpmButtonStartUs = 0;
            tunernome->bpmButtonAccumulatedUs = 0;

            tunernome->blinkStartUs = 0;
            tunernome->blinkAccumulatedUs = 0;
            tunernome->isBlinking = false;

            recalcMetronome();

            led_t leds[NUM_LEDS] = {{0}};
            setLeds(leds, NUM_LEDS);

            tunernome->disp->clearPx();
            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * Called when tunernome is exited
 */
void tunernomeExitMode(void)
{
    buzzer_stop();

    freeFont(&tunernome->ibm_vga8);
    freeFont(&tunernome->radiostars);
    freeFont(&tunernome->mm);

    freeWsg(&(tunernome->radiostarsArrowWsg));
    freeWsg(&(tunernome->mmArrowWsg));
    freeWsg(&(tunernome->flatWsg));

    free(tunernome);
}

/**
 * Inline helper function to get the magnitude of a frequency bin from fuzzed_bins[]
 *
 * @param idx The index to get a magnitude from
 * @return A signed magnitude, even though fuzzed_bins[] is unsigned
 */
static inline int16_t getMagnitude(uint16_t idx)
{
    return tunernome->end.fuzzed_bins[idx];
}

/**
 * Inline helper function to get the difference in magnitudes around a given
 * frequency bin from fuzzed_bins[]
 *
 * @param idx The index to get a difference in magnitudes around
 * @return The difference in magnitudes of the bins before and after the given index
 */
static inline int16_t getDiffAround(uint16_t idx)
{
    return getMagnitude(idx + 1) - getMagnitude(idx - 1);
}

/**
 * Inline helper function to get the magnitude of a frequency bin from folded_bins[]
 *
 * @param idx The index to get a magnitude from
 * @return A signed magnitude, even though folded_bins[] is unsigned
 */
static inline int16_t getSemiMagnitude(int16_t idx)
{
    if(idx < 0)
    {
        idx += FIXBPERO;
    }
    if(idx > FIXBPERO - 1)
    {
        idx -= FIXBPERO;
    }
    return tunernome->end.folded_bins[idx];
}

/**
 * Inline helper function to get the difference in magnitudes around a given
 * frequency bin from folded_bins[]
 *
 * @param idx The index to get a difference in magnitudes around
 * @return The difference in magnitudes of the bins before and after the given index
 */
static inline int16_t getSemiDiffAround(uint16_t idx)
{
    return getSemiMagnitude(idx + 1) - getSemiMagnitude(idx - 1);
}

/**
 * Recalculate the per-bpm values for the metronome
 */
void recalcMetronome(void)
{
    // Figure out how many microseconds are in one beat
    tunernome->usPerBeat = (60 * 1000000) / tunernome->bpm;

}

// TODO: make this compatible with instruments with an odd number of notes
/**
 * Plot instrument name and the note names of strings, arranged to match LED positions, in middle of display
 * @param instrumentName The name of the instrument to plot to the display
 * @param instrumentNotes The note names of the strings of the instrument to plot to the display
 * @param numNotes The number of notes in instrumentsNotes
 */
void plotInstrumentNameAndNotes(const char* instrumentName, const char** instrumentNotes,
                                uint16_t numNotes)
{
    // Mode name
    drawText(tunernome->disp, &tunernome->mm, c555, instrumentName,
             (tunernome->disp->w - textWidth(&tunernome->mm, instrumentName)) / 2,
             (tunernome->disp->h - tunernome->mm.h) / 2 - TUNER_TEXT_Y_OFFSET);

    // Note names of strings, arranged to match LED positions
    bool oddNumLedRows = (int) ceil(numNotes / 2.0f) % 2 == 1;
    // Left Column
    for(int i = 0; i < ceil(numNotes / 2.0f); i++)
    {
        int y;
        if(oddNumLedRows)
        {
            y = (tunernome->disp->h - tunernome->mm.h) / 2 + (tunernome->mm.h + 5) * (1 - i) - TUNER_TEXT_Y_OFFSET;
        }
        else
        {
            y = tunernome->disp->h / 2 + (tunernome->mm.h + 5) * (- i) + 2 - TUNER_TEXT_Y_OFFSET;
        }

        char buf[2] = {0};
        strncpy(buf, instrumentNotes[i], 1);
        drawText(tunernome->disp, &tunernome->mm, c555, buf,
                 (tunernome->disp->w - textWidth(&tunernome->mm, instrumentName)) / 2 -
                 textWidth(&tunernome->mm, /*placeholder for widest note name + ' '*/ "A4 "), y);

        strncpy(buf, instrumentNotes[i] + 1, 1);
        drawText(tunernome->disp, &tunernome->mm, c555, buf,
                 (tunernome->disp->w - textWidth(&tunernome->mm, instrumentName)) / 2 -
                 textWidth(&tunernome->mm, /*placeholder for widest octave number + ' '*/ "4 "), y);
    }
    oddNumLedRows = (int) floor(numNotes / 2.0f) % 2 == 1;
    // Right Column
    for(int i = ceil(numNotes / 2.0f); i < numNotes; i++)
    {
        int y;
        if(oddNumLedRows)
        {
            y = (tunernome->disp->h - tunernome->mm.h) / 2 + (tunernome->mm.h + 5) * (i - ceil(numNotes / 2.0f) - 1) - TUNER_TEXT_Y_OFFSET;
        }
        else
        {
            y = tunernome->disp->h / 2 + (tunernome->mm.h + 5) * (i - ceil(numNotes / 2.0f) - 1) + 2 - TUNER_TEXT_Y_OFFSET;
        }

        char buf[2] = {0};
        strncpy(buf, instrumentNotes[i], 1);
        drawText(tunernome->disp, &tunernome->mm, c555, buf,
                 (tunernome->disp->w + textWidth(&tunernome->mm, instrumentName)) / 2 + textWidth(&tunernome->mm, " "), y);
        
        strncpy(buf, instrumentNotes[i] + 1, 1);
        drawText(tunernome->disp, &tunernome->mm, c555, buf,
                 (tunernome->disp->w + textWidth(&tunernome->mm, instrumentName)) / 2 + textWidth(&tunernome->mm, /*' ' + placeholder for widest note name without octave number*/ " A"), y);
    }

    drawText(tunernome->disp, &tunernome->radiostars, c555, playStringsText,
             (tunernome->disp->w - textWidth(&tunernome->radiostars, playStringsText)) / 2,
             METRONOME_CENTER_Y - (TUNER_RADIUS + tunernome->radiostars.h) / 2);
}

/**
 * Instrument-agnostic tuner magic. Updates LEDs
 * @param freqBinIdxs An array of the indices of notes for the instrument's strings. See freqBinIdxsGuitar for an example.
 * @param numStrings The number of strings on the instrument, also the number of elements in freqBinIdxs and stringIdxToLedIdx, if applicable
 * @param colors The RGB colors of the LEDs to set
 * @param stringIdxToLedIdx A remapping from each index into freqBinIdxs (same index into stringIdxToLedIdx), to the index of an LED to map that string/freqBinIdx to. Set to NULL to skip remapping.
 */
void instrumentTunerMagic(const uint16_t freqBinIdxs[], uint16_t numStrings, led_t colors[],
                          const uint16_t stringIdxToLedIdx[])
{
    uint32_t i;
    for( i = 0; i < numStrings; i++ )
    {
        // Pick out the current magnitude and filter it
        tunernome->intensities_filt[i] = (getMagnitude(freqBinIdxs[i] + GUITAR_OFFSET) + tunernome->intensities_filt[i]) -
                                         (tunernome->intensities_filt[i] >> 5);

        // Pick out the difference around current magnitude and filter it too
        tunernome->diffs_filt[i] = (getDiffAround(freqBinIdxs[i] + GUITAR_OFFSET) + tunernome->diffs_filt[i]) -
                                   (tunernome->diffs_filt[i] >> 5);

        // This is the magnitude of the target frequency bin, cleaned up
        int16_t intensity = (tunernome->intensities_filt[i] >> SENSITIVITY) - 40; // drop a baseline.
        intensity = CLAMP(intensity, 0, 255);

        // This is the tonal difference. You "calibrate" out the intensity.
        int16_t tonalDiff = (tunernome->diffs_filt[i] >> SENSITIVITY) * 200 / (intensity + 1);

        int32_t red, grn, blu;
        // Is the note in tune, i.e. is the magnitude difference in surrounding bins small?
        if( (ABS(tonalDiff) < TONAL_DIFF_IN_TUNE_DEVIATION) )
        {
            // Note is in tune, make it white
            red = 255;
            grn = 255;
            blu = 255;
        }
        else
        {
            // Check if the note is sharp or flat
            if( tonalDiff > 0 )
            {
                // Note too sharp, make it red
                red = 255;
                grn = blu = 255 - (tonalDiff) * 15;
            }
            else
            {
                // Note too flat, make it blue
                blu = 255;
                grn = red = 255 - (-tonalDiff) * 15;
            }

            // Make sure LED output isn't more than 255
            red = CLAMP(red, INT_MIN, 255);
            grn = CLAMP(grn, INT_MIN, 255);
            blu = CLAMP(blu, INT_MIN, 255);
        }

        // Scale each LED's brightness by the filtered intensity for that bin
        red = (red >> 3 ) * ( intensity >> 3);
        grn = (grn >> 3 ) * ( intensity >> 3);
        blu = (blu >> 3 ) * ( intensity >> 3);

        // Set the LED, ensure each channel is between 0 and 255
        colors[(stringIdxToLedIdx != NULL) ? stringIdxToLedIdx[i] : i].r = CLAMP(red, 0, 255);
        colors[(stringIdxToLedIdx != NULL) ? stringIdxToLedIdx[i] : i].g = CLAMP(grn, 0, 255);
        colors[(stringIdxToLedIdx != NULL) ? stringIdxToLedIdx[i] : i].b = CLAMP(blu, 0, 255);
    }
}

/**
 * This is called periodically to render the OLED image
 *
 * @param elapsedUs The time elapsed since the last time this function was
 *                  called. Use this value to determine when it's time to do
 *                  things
 */
void tunernomeMainLoop(int64_t elapsedUs)
{
    tunernome->disp->clearPx();
    fillDisplayArea(tunernome->disp, 0, 0, tunernome->disp->w, tunernome->disp->h, c001);

    switch(tunernome->mode)
    {
        default:
        case TN_TUNER:
        {
            // Instructions at top of display
            drawText(tunernome->disp, &tunernome->radiostars, c115, "Flat", CORNER_OFFSET, CORNER_OFFSET);
            drawText(tunernome->disp, &tunernome->radiostars, c555, "In-Tune", (tunernome->disp->w - textWidth(&tunernome->radiostars,
                     "In-Tune")) / 2, CORNER_OFFSET);
            drawText(tunernome->disp, &tunernome->radiostars, c500, "Sharp", tunernome->disp->w - textWidth(&tunernome->radiostars,
                     "Sharp") - CORNER_OFFSET, CORNER_OFFSET);

            // A/B/Start button functions at bottom of display
            int16_t afterText = drawText(tunernome->disp, &tunernome->ibm_vga8, c151, "A", CORNER_OFFSET,
                                         tunernome->disp->h - tunernome->ibm_vga8.h - CORNER_OFFSET);
            afterText = drawText(tunernome->disp, &tunernome->ibm_vga8, c555, "/", afterText,
                                 tunernome->disp->h - tunernome->ibm_vga8.h - CORNER_OFFSET);
            afterText = drawText(tunernome->disp, &tunernome->ibm_vga8, c511, "B", afterText,
                                 tunernome->disp->h - tunernome->ibm_vga8.h - CORNER_OFFSET);
            afterText = drawText(tunernome->disp, &tunernome->ibm_vga8, c555, leftStr, afterText,
                                         tunernome->disp->h - tunernome->ibm_vga8.h - CORNER_OFFSET);

            char gainStr[16] = {0};
            snprintf(gainStr, sizeof(gainStr) - 1, "Gain: %d", getMicGain());
            drawText(tunernome->disp, &tunernome->ibm_vga8, c555, gainStr, afterText,
                     tunernome->disp->h - tunernome->ibm_vga8.h - CORNER_OFFSET);

            drawText(tunernome->disp, &tunernome->ibm_vga8, c555, rightStrMetronome,
                     tunernome->disp->w - textWidth(&tunernome->ibm_vga8, rightStrMetronome) - CORNER_OFFSET,
                     tunernome->disp->h - tunernome->ibm_vga8.h - CORNER_OFFSET);

            // Up/Down arrows in middle of display around current note/mode
            drawWsg(tunernome->disp, &(tunernome->radiostarsArrowWsg),
                    (tunernome->disp->w - tunernome->radiostarsArrowWsg.w) / 2 + 1,
                    (tunernome->disp->h - tunernome->mm.h) / 2 - tunernome->radiostarsArrowWsg.h - TUNER_ARROW_Y_OFFSET - TUNER_TEXT_Y_OFFSET,
                    false, false, 0);
            drawWsg(tunernome->disp, &(tunernome->radiostarsArrowWsg),
                    (tunernome->disp->w - tunernome->radiostarsArrowWsg.w) / 2 + 1,
                    (tunernome->disp->h + tunernome->mm.h) / 2 + TUNER_ARROW_Y_OFFSET - TUNER_TEXT_Y_OFFSET,
                    false, true, 0);

            // Current note/mode in middle of display
            switch(tunernome->curTunerMode)
            {
                case GUITAR_TUNER:
                {
                    plotInstrumentNameAndNotes(theWordGuitar, guitarNoteNames, NUM_GUITAR_STRINGS);
                    break;
                }
                case VIOLIN_TUNER:
                {
                    plotInstrumentNameAndNotes(theWordViolin, violinNoteNames, NUM_VIOLIN_STRINGS);
                    break;
                }
                case UKULELE_TUNER:
                {
                    plotInstrumentNameAndNotes(theWordUkulele, ukuleleNoteNames, NUM_UKULELE_STRINGS);
                    break;
                }
                case BANJO_TUNER:
                {
                    plotInstrumentNameAndNotes(theWordBanjo, banjoNoteNames, NUM_BANJO_STRINGS);
                    break;
                }
                case LISTENING:
                {
                    // Find the note that has the highest intensity. Must be larger than 100
                    int16_t maxIntensity = 100;
                    int8_t semitoneNum = -1;
                    for(uint8_t semitone = 0; semitone < NUM_SEMITONES; semitone++)
                    {
                        if(tunernome->intensity[semitone] > maxIntensity)
                        {
                            maxIntensity = tunernome->intensity[semitone];
                            semitoneNum = semitone;
                        }
                    }

                    drawText(tunernome->disp, &tunernome->radiostars, c555, listeningText,
                             (tunernome->disp->w - textWidth(&tunernome->radiostars, listeningText)) / 2,
                             METRONOME_CENTER_Y - (TUNER_RADIUS + tunernome->radiostars.h) / 2);

                    led_t leds[NUM_LEDS] = {{0}};

                    // If some note is intense
                    if(-1 != semitoneNum)
                    {
                        // Plot text on top of everything else
                        bool shouldDrawFlat = (semitoneNoteNames[semitoneNum][strlen(semitoneNoteNames[semitoneNum]) - 1] == 1);
                        char buf[5] = {0};
                        strncpy(buf, semitoneNoteNames[semitoneNum], 4);
                        int16_t tWidth = textWidth(&tunernome->mm, buf);
                        if(shouldDrawFlat)
                        {
                            tWidth += tunernome->flatWsg.w + 1;
                        }
                        int16_t textEnd = drawText(tunernome->disp, &tunernome->mm, c555, buf,
                                                   (tunernome->disp->w - tWidth) / 2 + 1,
                                                   (tunernome->disp->h - tunernome->mm.h) / 2 - TUNER_TEXT_Y_OFFSET);

                        // Append the wsg for a flat
                        if(shouldDrawFlat)
                        {
                            drawWsg(tunernome->disp, &tunernome->flatWsg, textEnd,
                                    (tunernome->disp->h - tunernome->mm.h) / 2 - TUNER_TEXT_Y_OFFSET,
                                    false, false, 0);
                        }

                        // Set the LEDs to a colorchord-like value
                        uint32_t toneColor = ECCtoHEX((semitoneNum * 256) / NUM_SEMITONES, 0xFF, 0x80);
                        for(uint8_t i = 0; i < NUM_LEDS; i++)
                        {
                            leds[i].r = (toneColor >>  0) & 0xFF;
                            leds[i].g = (toneColor >>  8) & 0xFF;
                            leds[i].b = (toneColor >> 16) & 0xFF;
                        }
                    }

                    // Set LEDs, this may turn them off
                    setLeds(leds, NUM_LEDS);
                    break;
                }
                case MAX_GUITAR_MODES:
                    break;
                case SEMITONE_0:
                case SEMITONE_1:
                case SEMITONE_2:
                case SEMITONE_3:
                case SEMITONE_4:
                case SEMITONE_5:
                case SEMITONE_6:
                case SEMITONE_7:
                case SEMITONE_8:
                case SEMITONE_9:
                case SEMITONE_10:
                case SEMITONE_11:
                default:
                {
                    // Draw tuner needle based on the value of tonalDiff, which is at most -32768 to 32767
                    // clamp it to the range -180 -> 180
                    int16_t clampedTonalDiff = CLAMP(tunernome->tonalDiff[tunernome->curTunerMode - SEMITONE_0] / 2, -180, 180);

                    // If the signal isn't intense enough, don't move the needle
                    if(tunernome->semitone_intensity_filt[tunernome->curTunerMode - SEMITONE_0] < 1000)
                    {
                        clampedTonalDiff = -180;
                    }

                    // Find the end point of the unit-length needle
                    float intermedX = sinf(clampedTonalDiff * M_PI / 360.0f );
                    float intermedY = cosf(clampedTonalDiff * M_PI / 360.0f );

                    // Find the actual end point of the full-length needle
                    int x = round(METRONOME_CENTER_X + (intermedX * TUNER_RADIUS));
                    int y = round(METRONOME_CENTER_Y - (intermedY * TUNER_RADIUS));

                    // Plot the needle
                    plotLine(tunernome->disp, METRONOME_CENTER_X, METRONOME_CENTER_Y, x, y, c555, 0);
                    // Plot dashed lines indicating the 'in tune' range
                    plotLine(tunernome->disp, METRONOME_CENTER_X, METRONOME_CENTER_Y, TUNER_FLAT_THRES_X, TUNER_THRES_Y, c555, 2);
                    plotLine(tunernome->disp, METRONOME_CENTER_X, METRONOME_CENTER_Y, TUNER_SHARP_THRES_X, TUNER_THRES_Y, c555, 2);
                    // Plot a semicircle around it all
                    plotCircleQuadrants(tunernome->disp, METRONOME_CENTER_X, METRONOME_CENTER_Y, TUNER_RADIUS, false, false, true, true,
                                        c555);

                    // Plot text on top of everything else
                    uint8_t semitoneNum = (tunernome->curTunerMode - SEMITONE_0);
                    bool shouldDrawFlat = (semitoneNoteNames[semitoneNum][strlen(semitoneNoteNames[semitoneNum]) - 1] == 1);
                    char buf[5] = {0};
                    strncpy(buf, semitoneNoteNames[semitoneNum], 4);
                    int16_t tWidth = textWidth(&tunernome->mm, buf);
                    if(shouldDrawFlat)
                    {
                        tWidth += tunernome->flatWsg.w + 1;
                    }
                    int16_t textEnd = drawText(tunernome->disp, &tunernome->mm, c555, buf,
                                               (tunernome->disp->w - tWidth) / 2 + 1,
                                               (tunernome->disp->h - tunernome->mm.h) / 2 - TUNER_TEXT_Y_OFFSET);

                    // Append the wsg for a flat
                    if(shouldDrawFlat)
                    {
                        drawWsg(tunernome->disp, &tunernome->flatWsg, textEnd,
                                (tunernome->disp->h - tunernome->mm.h) / 2 - TUNER_TEXT_Y_OFFSET,
                                false, false, 0);
                    }
                    break;
                }
            }

            break;
        }
        case TN_METRONOME:
        {
            // BPM at top of display
            char bpmStr[16];
            sprintf(bpmStr, "%d BPM", tunernome->bpm);
            int16_t beforeText = (tunernome->disp->w - textWidth(&tunernome->mm, bpmStr)) / 2;
            int16_t afterText = drawText(tunernome->disp, &tunernome->mm, c555, bpmStr, beforeText, 5);
            
            drawWsg(tunernome->disp, &(tunernome->mmArrowWsg),
                    beforeText - tunernome->mmArrowWsg.w - 8,
                    5,
                    false, false, 0);
            drawWsg(tunernome->disp, &(tunernome->mmArrowWsg),
                    afterText + 7,
                    5,
                    false, true, 0);

            // Draw the current beat it's on
            float beatXSpacing = (tunernome->disp->w / tunernome->beatLength);
            uint8_t beatRadius = 12;
            if (tunernome->beatLength > 11) {
                beatRadius = 8;
            }
            for(uint8_t i = 0; i < tunernome->beatLength; i++)
            {
                // Am aware of the fact that the spacing is off when you add more circles.
                // Oh well.
                uint16_t beatX = (beatXSpacing * i) + (beatXSpacing / 2);
                if (tunernome->beatCtr == i) {
                    plotCircleFilled(tunernome->disp, beatX, 48, beatRadius, c555);
                } else {
                    plotCircle(tunernome->disp, beatX, 48, beatRadius, c555);
                }
            }

            // Draw the amount of beats
            char beatStr[16];
            sprintf(beatStr, "< Beats %d >", tunernome->beatLength);
            drawText(tunernome->disp, &tunernome->ibm_vga8, c555, beatStr, 
                     CORNER_OFFSET, tunernome->disp->h - tunernome->ibm_vga8.h - CORNER_OFFSET);
            
            // Draw text to switch to tuner mode
            drawText(tunernome->disp, &tunernome->ibm_vga8, c555, rightStrTuner,
                     tunernome->disp->w - textWidth(&tunernome->ibm_vga8, rightStrTuner) - CORNER_OFFSET,
                     tunernome->disp->h - tunernome->ibm_vga8.h - CORNER_OFFSET);

            // Other logic things
            if(tunernome->isBlinking)
            {
                if(tunernome->blinkAccumulatedUs == 0)
                {
                    tunernome->blinkAccumulatedUs = esp_timer_get_time() - tunernome->blinkStartUs;
                }
                else
                {
                    tunernome->blinkAccumulatedUs += elapsedUs;
                }

                if(tunernome->blinkAccumulatedUs > METRONOME_FLASH_MS * 1000)
                {
                    led_t leds[NUM_LEDS] = {{0}};
                    setLeds(leds, NUM_LEDS);
                }
            }

            bool shouldBlink = false;
            // If the arm is sweeping clockwise
            if(tunernome->isClockwise)
            {
                // Add to tAccumulatedUs
                tunernome->tAccumulatedUs += elapsedUs;
                // If it's crossed the threshold for one beat
                if(tunernome->tAccumulatedUs >= tunernome->usPerBeat)
                {
                    // Flip the metronome arm
                    tunernome->isClockwise = false;
                    // Start counting down by subtacting the excess time from tAccumulatedUs
                    tunernome->tAccumulatedUs = tunernome->usPerBeat - (tunernome->tAccumulatedUs - tunernome->usPerBeat);
                    // Blink LED Tick color
                    shouldBlink = true;
                } // if(tAccumulatedUs >= tunernome->usPerBeat)
            } // if(tunernome->isClockwise)
            else
            {
                // Subtract from tAccumulatedUs
                tunernome->tAccumulatedUs -= elapsedUs;
                // If it's crossed the threshold for one beat
                if(tunernome->tAccumulatedUs <= 0)
                {
                    // Flip the metronome arm
                    tunernome->isClockwise = true;
                    // Start counting up by flipping the excess time from negative to positive
                    tunernome->tAccumulatedUs = -(tunernome->tAccumulatedUs);
                    // Blink LED Tock color
                    shouldBlink = true;
                } // if(tunernome->tAccumulatedUs <= 0)
            } // if(!tunernome->isClockwise)

            if(shouldBlink)
            {
                // Add one to the beat counter
                tunernome->beatCtr = (tunernome->beatCtr + 1) % tunernome->beatLength;

                const song_t* song;
                led_t leds[NUM_LEDS] = {{0}};

                if(0 == tunernome->beatCtr)
                {
                    song = &metronome_primary;
                    for(int i = 0; i < NUM_LEDS; i++)
                    {
                        leds[i].r = 0x40;
                        leds[i].g = 0xFF;
                        leds[i].b = 0x00;
                    }
                }
                else
                {
                    song = &metronome_secondary;
                    for(int i = 0; i < 4; i++)
                    {
                        leds[fourLedFlashIdxs[i]].r = 0x40;
                        leds[fourLedFlashIdxs[i]].g = 0x00;
                        leds[fourLedFlashIdxs[i]].b = 0xFF;
                    }
                }

                buzzer_play_sfx(song);
                setLeds(leds, NUM_LEDS);
                tunernome->isBlinking = true;
                tunernome->blinkStartUs = esp_timer_get_time();
                tunernome->blinkAccumulatedUs = 0;
            }

            // Runs after the up or down button is held long enough in metronome mode.
            // Repeats the button press while the button is held.
            if(tunernome->lastBpmButton != 0)
            {
                if(tunernome->bpmButtonAccumulatedUs == 0)
                {
                    tunernome->bpmButtonAccumulatedUs = esp_timer_get_time() - tunernome->bpmButtonStartUs;
                    tunernome->bpmButtonCurChangeUs = tunernome->bpmButtonAccumulatedUs;
                }
                else
                {
                    tunernome->bpmButtonAccumulatedUs += elapsedUs;
                    tunernome->bpmButtonCurChangeUs += elapsedUs;
                }

                if(tunernome->bpmButtonAccumulatedUs >= BPM_CHANGE_FIRST_MS * 1000)
                {
                    if(tunernome->bpmButtonCurChangeUs >= BPM_CHANGE_REPEAT_MS * 1000)
                    {
                        int16_t mod = 1;
                        if(tunernome->bpmButtonAccumulatedUs >= BPM_CHANGE_FAST_MS * 1000)
                        {
                            mod = 3;
                        }
                        switch(tunernome->lastBpmButton)
                        {
                            case UP:
                            {
                                modifyBpm(mod);
                                break;
                            }
                            case DOWN:
                            {
                                modifyBpm(-mod);
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }

                        tunernome->bpmButtonCurChangeUs = 0;
                    }
                }
            }

            // Draw metronome arm based on the value of tAccumulatedUs, which is between (0, usPerBeat)
            float intermedX = -1 * cosf(tunernome->tAccumulatedUs * M_PI / tunernome->usPerBeat );
            float intermedY = -1 * sinf(tunernome->tAccumulatedUs * M_PI / tunernome->usPerBeat );
            int x = round(METRONOME_CENTER_X - (intermedX * METRONOME_RADIUS));
            int y = round(METRONOME_CENTER_Y - (ABS(intermedY) * METRONOME_RADIUS));
            plotLine(tunernome->disp, METRONOME_CENTER_X, METRONOME_CENTER_Y, x, y, c555, 0);
            break;
        } // case TN_METRONOME:
    } // switch(tunernome->mode)
}

/**
 * TODO
 *
 * @param evt The button event that occurred
 */
void tunernomeButtonCallback(buttonEvt_t* evt)
{
    switch (tunernome->mode)
    {
        default:
        case TN_TUNER:
        {
            if(evt->down)
            {
                switch(evt->button)
                {
                    case UP:
                    {
                        tunernome->curTunerMode = (tunernome->curTunerMode + 1) % MAX_GUITAR_MODES;
                        break;
                    }
                    case DOWN:
                    {
                        if(0 == tunernome->curTunerMode)
                        {
                            tunernome->curTunerMode = MAX_GUITAR_MODES - 1;
                        }
                        else
                        {
                            tunernome->curTunerMode--;
                        }
                        break;
                    }
                    case BTN_A:
                    {
                        // Cycle microphone sensitivity
                        incMicGain();
                        break;
                    }
                    case START:
                    {
                        switchToSubmode(TN_METRONOME);
                        break;
                    }
                    case BTN_B:
                    {
                        // Cycle microphone sensitivity
                        decMicGain();
                        break;
                    }
                    default:
                    {
                        break;
                    }
                } // switch(button)
            } // if(down)
            break;
        } // case TN_TUNER:
        case TN_METRONOME:
        {
            if(evt->down)
            {
                switch(evt->button)
                {
                    case UP:
                    {
                        modifyBpm(1);
                        tunernome->lastBpmButton = evt->button;
                        tunernome->bpmButtonStartUs = esp_timer_get_time();
                        tunernome->bpmButtonCurChangeUs = 0;
                        tunernome->bpmButtonAccumulatedUs = 0;
                        break;
                    }
                    case DOWN:
                    {
                        modifyBpm(-1);
                        tunernome->lastBpmButton = evt->button;
                        tunernome->bpmButtonStartUs = esp_timer_get_time();
                        tunernome->bpmButtonCurChangeUs = 0;
                        tunernome->bpmButtonAccumulatedUs = 0;
                        break;
                    }
                    case LEFT:
                    {
                        if (tunernome->beatLength > 1)
                        {
                            tunernome->beatLength -= 1;
                        }
                        break;
                    }
                    case RIGHT:
                    {
                        if (tunernome->beatLength < MAX_BEATS)
                        {
                            tunernome->beatLength += 1;
                        }
                        break;
                    }
                    case START:
                    {
                        switchToSubmode(TN_TUNER);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                } // switch(button)
            } // if(down)
            else
            {
                switch(evt->button)
                {
                    case UP:
                    case DOWN:
                    {
                        if(evt->button == tunernome->lastBpmButton)
                        {
                            tunernome->lastBpmButton = 0;
                            tunernome->bpmButtonStartUs = 0;
                            tunernome->bpmButtonCurChangeUs = 0;
                            tunernome->bpmButtonAccumulatedUs = 0;
                        }
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
            break;
        } // case TN_METRONOME:
    }
}

/**
 * Increases the bpm by bpmMod, may be negative
 *
 * @param bpmMod The amount to change the BPM
 */
void modifyBpm(int16_t bpmMod)
{
    tunernome->bpm = CLAMP(tunernome->bpm + bpmMod, 1, MAX_BPM);
    recalcMetronome();
}

/**
 * This function is called whenever audio samples are read from the
 * microphone (ADC) and are ready for processing. Samples are read at 8KHz.
 *
 * @param samples A pointer to 12 bit audio samples
 * @param sampleCnt The number of samples read
 */
void tunernomeSampleHandler(uint16_t* samples, uint32_t sampleCnt)
{
    if(tunernome->mode == TN_TUNER)
    {
        for(uint32_t i = 0; i < sampleCnt; i++)
        {
            PushSample32( &tunernome->dd, samples[i] );
        }
        tunernome->audioSamplesProcessed += sampleCnt;

        // If at least 128 samples have been processed
        if( tunernome->audioSamplesProcessed >= 128 )
        {
            // Colorchord magic
            HandleFrameInfo(&tunernome->end, &tunernome->dd);

            led_t colors[NUM_LEDS] = {{0}};

            switch(tunernome->curTunerMode)
            {
                case GUITAR_TUNER:
                {
                    instrumentTunerMagic(freqBinIdxsGuitar, NUM_GUITAR_STRINGS, colors, sixNoteStringIdxToLedIdx);
                    break;
                }
                case VIOLIN_TUNER:
                {
                    instrumentTunerMagic(freqBinIdxsViolin, NUM_VIOLIN_STRINGS, colors, fourNoteStringIdxToLedIdx);
                    break;
                }
                case UKULELE_TUNER:
                {
                    instrumentTunerMagic(freqBinIdxsUkulele, NUM_UKULELE_STRINGS, colors, fourNoteStringIdxToLedIdx);
                    break;
                }
                case BANJO_TUNER:
                {
                    instrumentTunerMagic(freqBinIdxsBanjo, NUM_BANJO_STRINGS, colors, fiveNoteStringIdxToLedIdx);
                    break;
                }
                case MAX_GUITAR_MODES:
                    break;
                case SEMITONE_0:
                case SEMITONE_1:
                case SEMITONE_2:
                case SEMITONE_3:
                case SEMITONE_4:
                case SEMITONE_5:
                case SEMITONE_6:
                case SEMITONE_7:
                case SEMITONE_8:
                case SEMITONE_9:
                case SEMITONE_10:
                case SEMITONE_11:
                case LISTENING:
                default:
                {
                    for(uint8_t semitone = 0; semitone < NUM_SEMITONES; semitone++)
                    {
                        // uint8_t semitoneIdx = (tunernome->curTunerMode - SEMITONE_0) * 2;
                        uint8_t semitoneIdx = semitone * 2;
                        // Pick out the current magnitude and filter it
                        tunernome->semitone_intensity_filt[semitone] = (getSemiMagnitude(semitoneIdx + CHROMATIC_OFFSET) +
                                tunernome->semitone_intensity_filt[semitone]) -
                                (tunernome->semitone_intensity_filt[semitone] >> 5);

                        // Pick out the difference around current magnitude and filter it too
                        tunernome->semitone_diff_filt[semitone] = (getSemiDiffAround(semitoneIdx + CHROMATIC_OFFSET) +
                                tunernome->semitone_diff_filt[semitone]) -
                                (tunernome->semitone_diff_filt[semitone] >> 5);


                        // This is the magnitude of the target frequency bin, cleaned up
                        tunernome->intensity[semitone] = (tunernome->semitone_intensity_filt[semitone] >> SENSITIVITY) -
                                                         40; // drop a baseline.
                        tunernome->intensity[semitone] = CLAMP(tunernome->intensity[semitone], 0, 255);

                        //This is the tonal difference. You "calibrate" out the intensity.
                        tunernome->tonalDiff[semitone] = (tunernome->semitone_diff_filt[semitone] >> SENSITIVITY) * 200 /
                                                         (tunernome->intensity[semitone] + 1);
                    }

                    // tonal diff is -32768 to 32767. if its within -10 to 10 (now defined as TONAL_DIFF_IN_TUNE_DEVIATION), it's in tune.
                    // positive means too sharp, negative means too flat
                    // intensity is how 'loud' that frequency is, 0 to 255. you'll have to play around with values
                    int32_t red, grn, blu;
                    // Is the note in tune, i.e. is the magnitude difference in surrounding bins small?
                    if( (ABS(tunernome->tonalDiff[tunernome->curTunerMode - SEMITONE_0]) < TONAL_DIFF_IN_TUNE_DEVIATION) )
                    {
                        // Note is in tune, make it white
                        red = 255;
                        grn = 255;
                        blu = 255;
                    }
                    else
                    {
                        // Check if the note is sharp or flat
                        if( tunernome->tonalDiff[tunernome->curTunerMode - SEMITONE_0] > 0 )
                        {
                            // Note too sharp, make it red
                            red = 255;
                            grn = blu = 255 - (tunernome->tonalDiff[tunernome->curTunerMode - SEMITONE_0] - TONAL_DIFF_IN_TUNE_DEVIATION) * 15;
                        }
                        else
                        {
                            // Note too flat, make it blue
                            blu = 255;
                            grn = red = 255 - (-(tunernome->tonalDiff[tunernome->curTunerMode - SEMITONE_0] + TONAL_DIFF_IN_TUNE_DEVIATION)) * 15;
                        }

                        // Make sure LED output isn't more than 255
                        red = CLAMP(red, INT_MIN, 255);
                        grn = CLAMP(grn, INT_MIN, 255);
                        blu = CLAMP(blu, INT_MIN, 255);
                    }

                    // Scale each LED's brightness by the filtered intensity for that bin
                    red = (red >> 3 ) * ( tunernome->intensity[tunernome->curTunerMode - SEMITONE_0] >> 3);
                    grn = (grn >> 3 ) * ( tunernome->intensity[tunernome->curTunerMode - SEMITONE_0] >> 3);
                    blu = (blu >> 3 ) * ( tunernome->intensity[tunernome->curTunerMode - SEMITONE_0] >> 3);

                    // Set the LED, ensure each channel is between 0 and 255
                    uint32_t i;
                    for (i = 0; i < NUM_GUITAR_STRINGS; i++)
                    {
                        colors[i].r = CLAMP(red, 0, 255);
                        colors[i].g = CLAMP(grn, 0, 255);
                        colors[i].b = CLAMP(blu, 0, 255);
                    }

                    break;
                } // default:
            } // switch(tunernome->curTunerMode)

            if(LISTENING != tunernome->curTunerMode)
            {
                // Draw the LEDs
                setLeds(colors, NUM_LEDS);
            }
            // Reset the sample count
            tunernome->audioSamplesProcessed = 0;
        }
    } // if(tunernome-> mode == TN_TUNER)
}
