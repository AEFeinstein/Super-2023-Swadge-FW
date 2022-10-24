/*
 * mode_slide_whistle.c
 *
 *  Created on: 21 Nov 2019
 *      Author: adam and bbkiwi
 */

/*==============================================================================
 * Includes
 *============================================================================*/

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
#include "settingsManager.h"
#include "swadgeMode.h"
#include "swadgeMusic.h"
#include "touch_sensor.h"

#include "mode_slide_whistle.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define CORNER_OFFSET 12
#define TOP_TEXT_X_MARGIN CORNER_OFFSET / 2
#define LINE_BREAK_Y 8
#define TICK_HEIGHT 4
#define CURSOR_HEIGHT 5
#define CURSOR_WIDTH 1
#define BAR_X_MARGIN 0
#define BAR_Y_MARGIN (slideWhistle->radiostars.h + CURSOR_HEIGHT + 2 + CORNER_OFFSET * 2)
#define BAR_X_WIDTH (slideWhistle->disp->w - (2 * BAR_X_MARGIN) - 1)
#define BAR_Y_SPACING (2 * CURSOR_HEIGHT + 5)

#define DEFAULT_PAUSE 5

/*==============================================================================
 * Enums
 *============================================================================*/

typedef enum
{
    THIRTYSECOND_NOTE = 3,
    THIRTYSECOND_REST = 3 | REST_BIT,
    SIXTEENTH_NOTE    = 6,
    SIXTEENTH_REST    = 6 | REST_BIT,
    EIGHTH_NOTE       = 12,
    EIGHTH_REST       = 12 | REST_BIT,
    QUARTER_NOTE      = 24,
    QUARTER_REST      = 24 | REST_BIT,
    HALF_NOTE         = 48,
    HALF_REST         = 48 | REST_BIT,
    WHOLE_NOTE        = 96,
    WHOLE_REST        = 96 | REST_BIT,

    TRIPLET_SIXTYFOURTH_NOTE  = 1,
    TRIPLET_SIXTYFOURTH_REST  = 1 | REST_BIT,
    TRIPLET_THIRTYSECOND_NOTE = 2,
    TRIPLET_THIRTYSECOND_REST = 2 | REST_BIT,
    TRIPLET_SIXTEENTH_NOTE    = 4,
    TRIPLET_SIXTEENTH_REST    = 4 | REST_BIT,
    TRIPLET_EIGHTH_NOTE       = 8,
    TRIPLET_EIGHTH_REST       = 8 | REST_BIT,
    TRIPLET_QUARTER_NOTE      = 16,
    TRIPLET_QUARTER_REST      = 16 | REST_BIT,
    TRIPLET_HALF_NOTE         = 32,
    TRIPLET_HALF_REST         = 32 | REST_BIT,
    TRIPLET_WHOLE_NOTE        = 64,
    TRIPLET_WHOLE_REST        = 64 | REST_BIT,

    DOTTED_SIXTEENTH_NOTE = 9,
    DOTTED_SIXTEENTH_REST = 9 | REST_BIT,
    DOTTED_EIGHTH_NOTE    = 18,
    DOTTED_EIGHTH_REST    = 18 | REST_BIT,
    DOTTED_QUARTER_NOTE   = 36,
    DOTTED_QUARTER_REST   = 36 | REST_BIT,
    DOTTED_HALF_NOTE      = 72,
    DOTTED_HALF_REST      = 72 | REST_BIT,
    DOTTED_WHOLE_NOTE     = 144,
    DOTTED_WHOLE_REST     = 144 | REST_BIT,
} rhythmNote_t;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void  slideWhistleEnterMode(display_t* disp);
void  slideWhistleExitMode(void);
void  slideWhistleButtonCallback(buttonEvt_t* evt);
void  slideWhistleTouchCallback(touch_event_t* evt);
void  slideWhistleAccelerometerHandler(accel_t* accel);
void  slideWhistleMainLoop(int64_t elapsedUs);
void  slideWhistleBeatTimerFunc(void* arg __attribute__((unused)));
noteFrequency_t  arpModify(noteFrequency_t note, int8_t arpInterval);
int16_t  getCurrentCursorX(void);
int16_t  getCurrentCursorY(void);
uint8_t  getCurrentOctaveIdx(void);
noteFrequency_t  getCurrentNote(void);
void  plotBar(uint8_t yOffset);

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    uint8_t bpmMultiplier;
    uint8_t bpm;
} bpm_t;

typedef struct
{
    rhythmNote_t note;
    int8_t arp;
} rhythmArp_t;

typedef struct
{
    // The parameter's name
    char* name;
    // The rhythm
    const rhythmArp_t* rhythm;
    const uint16_t rhythmLen;
    // Inter-note pause
    const uint16_t interNotePauseMs;
    // Default BPM
    const uint8_t defaultBpm;
} rhythm_t;

/*==============================================================================
 * Variables
 *============================================================================*/

// The swadge mode
swadgeMode modeSlideWhistle =
{
    .modeName = "TechnoSlideWhistle",
    .fnEnterMode = slideWhistleEnterMode,
    .fnExitMode = slideWhistleExitMode,
    .fnButtonCallback = slideWhistleButtonCallback,
    .fnTouchCallback = slideWhistleTouchCallback,
    .fnMainLoop = slideWhistleMainLoop,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = slideWhistleAccelerometerHandler,
    .fnAudioCallback = NULL,
    .overrideUsb = false
};

// The state data
typedef struct
{
    display_t* disp;
    font_t ibm_vga8;
    font_t radiostars;
    font_t mm;

    // Track motion
    int16_t roll;
    int16_t pitch;
    char accelStr[32];
    char accelStr2[32];

    // Track touch
    bool touchHeld;
    int32_t touchPosition;
    int32_t touchIntensity;

    // Track rhythm
    esp_timer_create_args_t beatTimerArgs;
    esp_timer_handle_t beatTimer;
    uint32_t timeUs;
    uint32_t lastCallTimeUs;
    int16_t rhythmNoteIdx;
    uint8_t bpmIdx;

    // Track the button
    bool aHeld;
    bool upHeld;
    bool downHeld;
    bool shouldPlay;
    noteFrequency_t heldNote;
    int16_t heldCursorX;
    int16_t heldCursorY;
    uint8_t rhythmIdx;
    uint8_t scaleIdx;
} slideWhistle_t;

slideWhistle_t* slideWhistle;

/*==============================================================================
 * Const Variables
 *============================================================================*/

// Text
const char rhythmText[] = "Sel: Rhythm";
const char bpmText[] =    "< >: BPM";
const char steerMeText[] = "Turn me like a steering wheel";
const char holdText[] = ": Hold";
const char playText[] =   ": Play";

// All the rhythms
const bpm_t bpms[] =
{
    {.bpmMultiplier = 10, .bpm = 250},
    {.bpmMultiplier = 11, .bpm = 227},
    {.bpmMultiplier = 13, .bpm = 192},
    {.bpmMultiplier = 15, .bpm = 167},
    {.bpmMultiplier = 18, .bpm = 139},
    {.bpmMultiplier = 22, .bpm = 114},
    {.bpmMultiplier = 29, .bpm = 86},
    {.bpmMultiplier = 41, .bpm = 61},
};

const rhythmArp_t constant[] =
{
    {.note = TRIPLET_SIXTYFOURTH_NOTE, .arp = 1},
};

const rhythmArp_t one_note[] =
{
    {.note = EIGHTH_NOTE, .arp = 1},
};

const rhythmArp_t octaves[] =
{
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 13},
};

const rhythmArp_t fifth[] =
{
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 8},
};

const rhythmArp_t major_tri[] =
{
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 5},
    {.note = SIXTEENTH_NOTE, .arp = 8},
};

const rhythmArp_t minor_tri[] =
{
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 4},
    {.note = SIXTEENTH_NOTE, .arp = 8},
};

const rhythmArp_t major_7[] =
{
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 1},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 5},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 8},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 12},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 8},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 5},
};

const rhythmArp_t minor_7[] =
{
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 1},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 4},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 8},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 11},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 8},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 4},
};

const rhythmArp_t dom_7[] =
{
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 1},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 5},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 8},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 11},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 8},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 4},
};

const rhythmArp_t swing[] =
{
    {.note = TRIPLET_QUARTER_NOTE, .arp = 1},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 1},
};

const rhythmArp_t syncopa[] =
{
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
};

const rhythmArp_t dw_stabs[] =
{
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_REST, .arp = 1},
};

const rhythmArp_t legendary[] =
{
    {.note = QUARTER_NOTE, .arp = 1},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 1},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 1},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 1},
    {.note = QUARTER_NOTE, .arp = 1},
    {.note = QUARTER_NOTE, .arp = 1},
};

const rhythmArp_t j_dawg[] =
{
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
};

const rhythmArp_t eightBMT[] =
{
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_REST, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_REST, .arp = 1},
    {.note = DOTTED_EIGHTH_NOTE, .arp = 1},
};

const rhythmArp_t the_goat[] =
{
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_REST, .arp = 1},
    {.note = DOTTED_EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
};

const rhythmArp_t sgp[] =
{
    {.note = DOTTED_EIGHTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = DOTTED_QUARTER_NOTE, .arp = 1},
    {.note = DOTTED_EIGHTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
};

const rhythmArp_t fourth_rock[] =
{
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 1},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 1},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 1},
    {.note = QUARTER_NOTE, .arp = 1},
    {.note = QUARTER_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = QUARTER_NOTE, .arp = 1},
};

const rhythmArp_t dub[] =
{
    {.note = QUARTER_NOTE, .arp = 1},
    {.note = QUARTER_NOTE, .arp = 1},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 13},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 13},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 13},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 13},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 13},
    {.note = TRIPLET_EIGHTH_NOTE, .arp = 13},
};

const rhythmArp_t octavio[] =
{
    {.note = EIGHTH_NOTE, .arp = 13},
    {.note = EIGHTH_NOTE, .arp = 13},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 13},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 8},
    {.note = EIGHTH_NOTE, .arp = 1},
};

const rhythmArp_t cha_cha[] =
{
    {.note = QUARTER_NOTE, .arp = 8},
    {.note = EIGHTH_NOTE, .arp = 4},
    {.note = QUARTER_NOTE, .arp = 9},
    {.note = QUARTER_NOTE, .arp = 4},
    {.note = QUARTER_NOTE, .arp = 8},
    {.note = QUARTER_NOTE, .arp = 4},
    {.note = EIGHTH_NOTE, .arp = 9},
    {.note = QUARTER_NOTE, .arp = 9},
    {.note = QUARTER_NOTE, .arp = 4},
};

const rhythmArp_t its_a_me[] =
{
    {.note = EIGHTH_NOTE, .arp = 5},
    {.note = EIGHTH_NOTE, .arp = 5},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 5},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 5},
    {.note = EIGHTH_REST, .arp = 1},
    {.note = QUARTER_NOTE, .arp = 8},
    {.note = QUARTER_REST, .arp = 1},
    {.note = QUARTER_NOTE, .arp = -6},
    {.note = QUARTER_REST, .arp = 1},
};

const rhythmArp_t so_strange[] =
{
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 5},
    {.note = SIXTEENTH_NOTE, .arp = 8},
    {.note = SIXTEENTH_NOTE, .arp = 12},
    {.note = SIXTEENTH_NOTE, .arp = 13},
    {.note = SIXTEENTH_NOTE, .arp = 12},
    {.note = SIXTEENTH_NOTE, .arp = 8},
    {.note = SIXTEENTH_NOTE, .arp = 5},
};

const rhythmArp_t sans[] =
{
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 13},
    {.note = SIXTEENTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 8},
    {.note = SIXTEENTH_REST, .arp = 1},
    {.note = EIGHTH_NOTE, .arp = 7},
    {.note = EIGHTH_NOTE, .arp = 6},
    {.note = EIGHTH_NOTE, .arp = 4},
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 4},
    {.note = SIXTEENTH_NOTE, .arp = 6},
};

const rhythmArp_t ducktales_moon[] =
{
    {.note = SIXTEENTH_NOTE, .arp = 1},
    {.note = SIXTEENTH_NOTE, .arp = 8},
    {.note = SIXTEENTH_NOTE, .arp = 13},
    {.note = SIXTEENTH_NOTE, .arp = 15},
    {.note = SIXTEENTH_NOTE, .arp = 8},
    {.note = SIXTEENTH_NOTE, .arp = 13},
    {.note = SIXTEENTH_NOTE, .arp = 15},
    {.note = SIXTEENTH_NOTE, .arp = 18},
    {.note = SIXTEENTH_NOTE, .arp = 8},
    {.note = SIXTEENTH_NOTE, .arp = 18},
    {.note = SIXTEENTH_NOTE, .arp = 17},
    {.note = SIXTEENTH_NOTE, .arp = 8},
    {.note = SIXTEENTH_NOTE, .arp = 17},
    {.note = SIXTEENTH_NOTE, .arp = 15},
    {.note = SIXTEENTH_NOTE, .arp = 13},
};

const rhythmArp_t stickerbush[] =
{
    {.note = EIGHTH_NOTE, .arp = 12},
    {.note = DOTTED_EIGHTH_NOTE, .arp = 13},
    {.note = DOTTED_EIGHTH_NOTE, .arp = 12},
    {.note = EIGHTH_NOTE, .arp = 13},
    {.note = DOTTED_EIGHTH_NOTE, .arp = 8},
    {.note = SIXTEENTH_REST, .arp = 1},
    {.note = EIGHTH_REST, .arp = 1},
};

const rhythm_t rhythms[] =
{
    {
        .name = "Slide",
        .rhythm = constant,
        .rhythmLen = lengthof(constant),
        .interNotePauseMs = 0,
        .defaultBpm = 0 // 250
    },
    {
        .name = "Qrtr",
        .rhythm = one_note,
        .rhythmLen = lengthof(one_note),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 5 // 114
    },
    {
        .name = "Octave",
        .rhythm = octaves,
        .rhythmLen = lengthof(octaves),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 4 // 139
    },
    {
        .name = "Fifth",
        .rhythm = fifth,
        .rhythmLen = lengthof(fifth),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 4 // 139
    },
    {
        .name = "Maj 3",
        .rhythm = major_tri,
        .rhythmLen = lengthof(major_tri),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 3 // 167
    },
    {
        .name = "Min 3",
        .rhythm = minor_tri,
        .rhythmLen = lengthof(minor_tri),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 3 // 167
    },
    {
        .name = "Maj 7",
        .rhythm = major_7,
        .rhythmLen = lengthof(major_7),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 4 // 139
    },
    {
        .name = "Min 7",
        .rhythm = minor_7,
        .rhythmLen = lengthof(minor_7),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 4 // 139
    },
    {
        .name = "Dom 7",
        .rhythm = dom_7,
        .rhythmLen = lengthof(dom_7),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 4 // 139
    },
    {
        .name = "Swing",
        .rhythm = swing,
        .rhythmLen = lengthof(swing),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 3 // 167
    },
    {
        .name = "Syncopa",
        .rhythm = syncopa,
        .rhythmLen = lengthof(syncopa),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 2 // 192
    },
    {
        .name = "DW_stabs",
        .rhythm = dw_stabs,
        .rhythmLen = lengthof(dw_stabs),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 6 // 86
    },
    {
        .name = "Lgnd",
        .rhythm = legendary,
        .rhythmLen = lengthof(legendary),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 4 // 139
    },
    {
        .name = "J-Dawg",
        .rhythm = j_dawg,
        .rhythmLen = lengthof(j_dawg),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 1 // 227
    },
    {
        .name = "8BMT",
        .rhythm = eightBMT,
        .rhythmLen = lengthof(eightBMT),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 3 // 167
    },
    {
        .name = "Goat",
        .rhythm = the_goat,
        .rhythmLen = lengthof(the_goat),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 5 // 114
    },
    {
        .name = "Sgp",
        .rhythm = sgp,
        .rhythmLen = lengthof(sgp),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 3 // 167
    },
    {
        .name = "Mars",
        .rhythm = fourth_rock,
        .rhythmLen = lengthof(fourth_rock),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 4 // 139
    },
    {
        .name = "dub",
        .rhythm = dub,
        .rhythmLen = lengthof(dub),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 4 // 139
    },
    {
        .name = "Octavio",
        .rhythm = octavio,
        .rhythmLen = lengthof(octavio),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 0 // 250
    },
    {
        .name = "Cha-Cha",
        .rhythm = cha_cha,
        .rhythmLen = lengthof(cha_cha),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 3 // 167
    },
    {
        .name = "It's-A-Me",
        .rhythm = its_a_me,
        .rhythmLen = lengthof(its_a_me),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 3 // 167
    },
    {
        .name = "Strange",
        .rhythm = so_strange,
        .rhythmLen = lengthof(so_strange),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 4 // 139
    },
    {
        .name = "Sans?",
        .rhythm = sans,
        .rhythmLen = lengthof(sans),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 5 // 114
    },
    {
        .name = "Quack",
        .rhythm = ducktales_moon,
        .rhythmLen = lengthof(ducktales_moon),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 6 // 86
    },
    {
        .name = "Stckrbsh",
        .rhythm = stickerbush,
        .rhythmLen = lengthof(stickerbush),
        .interNotePauseMs = DEFAULT_PAUSE,
        .defaultBpm = 6 // 86
    }
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for slideWhistle
 */
void  slideWhistleEnterMode(display_t* disp)
{
    // Allocate zero'd memory for the mode
    slideWhistle = calloc(1, sizeof(slideWhistle_t));

    slideWhistle->disp = disp;

    loadFont("ibm_vga8.font", &slideWhistle->ibm_vga8);
    loadFont("radiostars.font", &slideWhistle->radiostars);
    loadFont("mm.font", &slideWhistle->mm);

    stopNote();

    // Set default BPM to default
    slideWhistle->bpmIdx = rhythms[slideWhistle->rhythmIdx].defaultBpm;

    // Set a timer to tick every 1ms, forever
    slideWhistle->beatTimerArgs.arg = NULL;
    slideWhistle->beatTimerArgs.callback = slideWhistleBeatTimerFunc;
    slideWhistle->beatTimerArgs.dispatch_method = ESP_TIMER_TASK;
    slideWhistle->beatTimerArgs.name = "slideWhistleBeatTimer";
    slideWhistle->beatTimerArgs.skip_unhandled_events = true;
    esp_timer_create(&slideWhistle->beatTimerArgs, &slideWhistle->beatTimer);
    if(!getSfxIsMuted())
    {
        esp_timer_start_periodic(slideWhistle->beatTimer, 1000);
    }
}

/**
 * Called when slideWhistle is exited
 */
void  slideWhistleExitMode(void)
{
    stopNote();

    freeFont(&slideWhistle->ibm_vga8);
    freeFont(&slideWhistle->radiostars);
    freeFont(&slideWhistle->mm);

    esp_timer_stop(slideWhistle->beatTimer);

    esp_timer_delete(slideWhistle->beatTimer);

    free(slideWhistle);
}

/**
 * This function is called when a button press is pressed. Buttons are
 * handled by interrupts and queued up for this callback, so there are no
 * strict timing restrictions for this function.
 *
 * @param evt The button event that occurred
 */
void  slideWhistleTouchCallback(touch_event_t* evt)
{
    slideWhistle->touchHeld = evt->state != 0;
    slideWhistle->shouldPlay = evt->state != 0 || slideWhistle->aHeld;
    //slideWhistle->touchPosition = roundf((evt->position * BAR_X_WIDTH) / 255);
}

/**
 * @brief Button callback function, plays notes and switches parameters
 *
 * @param evt The button event that occurred
 */
void  slideWhistleButtonCallback(buttonEvt_t* evt)
{
    switch(evt->button)
    {
        case START:
        {
            if(evt->down)
            {
                // Cycle the scale
                slideWhistle->scaleIdx = (slideWhistle->scaleIdx + 1) % lengthof(scales);
            }
            break;
        }
        case SELECT:
        {
            if(evt->down)
            {
                // Cycle the rhythm
                slideWhistle->rhythmIdx = (slideWhistle->rhythmIdx + 1) % lengthof(rhythms);
                slideWhistle->timeUs = 0;
                slideWhistle->rhythmNoteIdx = 0;
                slideWhistle->lastCallTimeUs = 0;
                slideWhistle->bpmIdx = rhythms[slideWhistle->rhythmIdx].defaultBpm;
            }

            break;
        }
        case BTN_A:
        {
            slideWhistle->rhythmNoteIdx = 0;
            slideWhistle->lastCallTimeUs = 0;
            slideWhistle->aHeld = evt->down;
            slideWhistle->shouldPlay = evt->down || slideWhistle->touchHeld;
            break;
        }
        case BTN_B:
        {
            if(evt->down)
            {
                slideWhistle->heldNote = getCurrentNote();
                slideWhistle->heldCursorX = getCurrentCursorX();
                slideWhistle->heldCursorY = getCurrentCursorY();
            }
            else
            {
                slideWhistle->heldNote = SILENCE;
                slideWhistle->heldCursorX = 0;
                slideWhistle->heldCursorY = 0;
            }
            break;
        }
        case UP:
        {
            slideWhistle->upHeld = evt->down;
            break;
        }
        case DOWN:
        {
            slideWhistle->downHeld = evt->down;
            break;
        }
        case LEFT:
        {
            if(evt->down)
            {
                slideWhistle->bpmIdx = (slideWhistle->bpmIdx + 1) % lengthof(bpms);
            }
            break;
        }
        case RIGHT:
        {
            if(evt->down)
            {
                if(slideWhistle->bpmIdx == 0)
                {
                    slideWhistle->bpmIdx = lengthof(bpms);
                }
                slideWhistle->bpmIdx = slideWhistle->bpmIdx - 1;
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * @brief Callback function for accelerometer values
 * Use the current vector to find pitch and roll, then update the display
 *
 * @param accel The freshly read accelerometer values
 */
void  slideWhistleAccelerometerHandler(accel_t* accel)
{
    // Get the centroid at the same rate at the accel for smoothness
    getTouchCentroid(&slideWhistle->touchPosition, &slideWhistle->touchIntensity);
    slideWhistle->touchPosition = (slideWhistle->touchPosition * BAR_X_WIDTH) / 1023;
    slideWhistle->touchPosition = CLAMP(slideWhistle->touchPosition, BAR_X_MARGIN,
                                        slideWhistle->disp->w - 1 - BAR_X_MARGIN);

    // Only find values when the swadge is pointed up
    if(accel-> x <= 0)
    {
        return;
    }

    // Find the roll and pitch in radians
    float rollF = atanf(accel->y / (float)accel->x);
    float pitchF = atanf((-1 * accel->z) / sqrtf((accel->y * accel->y) + (accel->x * accel->x)));

    // Normalize the values to [0,1]
    rollF = ((rollF) / M_PI) + 0.5f;
    pitchF = ((pitchF) / M_PI) + 0.5f;

    // Round and scale to BAR_X_WIDTH
    // this maps 30 degrees to the far left and 150 degrees to the far right
    // (30 / 180) == 0.167, (180 - (2 * 30)) / 180 == 0.666
    slideWhistle->roll = BAR_X_WIDTH - roundf(((rollF - 0.167f) * BAR_X_WIDTH) / 0.666f);
    slideWhistle->roll = CLAMP(slideWhistle->roll, BAR_X_MARGIN, slideWhistle->disp->w - 1 - BAR_X_MARGIN);

    slideWhistle->pitch = roundf(pitchF * BAR_X_WIDTH);
    slideWhistle->pitch = CLAMP(slideWhistle->pitch, BAR_X_MARGIN, slideWhistle->disp->w - 1 - BAR_X_MARGIN);

    snprintf(slideWhistle->accelStr, sizeof(slideWhistle->accelStr), "roll %5d pitch %5d",
             slideWhistle->roll, slideWhistle->pitch);
    snprintf(slideWhistle->accelStr2, sizeof(slideWhistle->accelStr2), "x %4d, y %4d, z %4d",
             accel->x, accel->y, accel->z);

    //slideWhistleMainLoop();
}

/**
 * Update the display by drawing the current state of affairs
 */
void  slideWhistleMainLoop(int64_t elapsedUs)
{
    slideWhistle->disp->clearPx();
    fillDisplayArea(slideWhistle->disp, 0, 0, slideWhistle->disp->w, slideWhistle->disp->h, c100);

    // Plot the bars
    for(int i = 0; i < NUM_OCTAVES; i++)
    {
        plotBar(slideWhistle->disp->h - BAR_Y_MARGIN - i * BAR_Y_SPACING);
    }

    if(slideWhistle->heldNote != SILENCE)
    {
        // Plot the cursor for the held note
        fillDisplayArea(
                    slideWhistle->disp,
                    slideWhistle->heldCursorX - CURSOR_WIDTH,
                    slideWhistle->heldCursorY - CURSOR_HEIGHT,
                    slideWhistle->heldCursorX + CURSOR_WIDTH,
                    slideWhistle->heldCursorY + CURSOR_HEIGHT,
                    c511);
    }

    // Plot the cursor
    fillDisplayArea(
                slideWhistle->disp,
                getCurrentCursorX() - CURSOR_WIDTH,
                getCurrentCursorY() - CURSOR_HEIGHT,
                getCurrentCursorX() + CURSOR_WIDTH,
                getCurrentCursorY() + CURSOR_HEIGHT,
                c551);

    // Plot instructions
    drawText(
            slideWhistle->disp,
            &slideWhistle->ibm_vga8, c235,
            steerMeText,
            (slideWhistle->disp->w - textWidth(&slideWhistle->ibm_vga8, steerMeText)) / 2,
            TOP_TEXT_X_MARGIN);
    
    int16_t secondLineStartY = slideWhistle->ibm_vga8.h + LINE_BREAK_Y + TOP_TEXT_X_MARGIN;

    // Plot the rhythm
    drawText(
        slideWhistle->disp,
        &slideWhistle->radiostars, c444,
        rhythmText,
        CORNER_OFFSET,
        secondLineStartY);
    drawText(
        slideWhistle->disp,
        &slideWhistle->radiostars, c555,
        rhythms[slideWhistle->rhythmIdx].name,
        slideWhistle->disp->w - textWidth(&slideWhistle->radiostars, rhythms[slideWhistle->rhythmIdx].name) - CORNER_OFFSET,
        secondLineStartY);

    // Plot the scale
    drawText(
        slideWhistle->disp,
        &slideWhistle->radiostars, c444,
        scaleText,
        CORNER_OFFSET,
        secondLineStartY + slideWhistle->radiostars.h + LINE_BREAK_Y);
    drawText(
        slideWhistle->disp,
        &slideWhistle->radiostars, c555,
        scales[slideWhistle->scaleIdx].name,
        slideWhistle->disp->w - textWidth(&slideWhistle->radiostars, scales[slideWhistle->scaleIdx].name) - CORNER_OFFSET,
        secondLineStartY + slideWhistle->radiostars.h + LINE_BREAK_Y);

    // Plot the BPM
    drawText(
        slideWhistle->disp,
        &slideWhistle->radiostars, c444,
        bpmText,
        CORNER_OFFSET,
        secondLineStartY + (slideWhistle->radiostars.h + LINE_BREAK_Y) * 2);

    char bpmStr[16] = {0};
    snprintf(bpmStr, sizeof(bpmStr), "%d", bpms[slideWhistle->bpmIdx].bpm);
    drawText(
        slideWhistle->disp,
        &slideWhistle->radiostars, c555,
        bpmStr,
        slideWhistle->disp->w - textWidth(&slideWhistle->radiostars, bpmStr) - CORNER_OFFSET,
        secondLineStartY + (slideWhistle->radiostars.h + LINE_BREAK_Y) * 2);

    // Debug print
    drawText(slideWhistle->disp, &slideWhistle->ibm_vga8, c444, slideWhistle->accelStr, 0,
             slideWhistle->disp->h / 4 + CORNER_OFFSET + 10);
    drawText(slideWhistle->disp, &slideWhistle->ibm_vga8, c444, slideWhistle->accelStr2, 0,
             slideWhistle->disp->h / 4 + slideWhistle->ibm_vga8.h + 11 + CORNER_OFFSET);

    // Warn the user that the swadge is muted, if that's the case
    if(getSfxIsMuted())
    {
        drawText(
            slideWhistle->disp,
            &slideWhistle->radiostars, c551,
            mutedText,
            (slideWhistle->disp->w - textWidth(&slideWhistle->radiostars, mutedText)) / 2,
            slideWhistle->disp->h / 2);
    }
    else
    {
        // Plot the note
        drawText(slideWhistle->disp, &slideWhistle->mm, c555, noteToStr(getCurrentNote()),
                 (slideWhistle->disp->w - textWidth(&slideWhistle->mm, noteToStr(getCurrentNote()))) / 2, slideWhistle->disp->h / 2);
    }

    // Plot the button funcs
    int16_t afterText = drawText(
        slideWhistle->disp,
        &slideWhistle->radiostars, c511,
        "B",
        CORNER_OFFSET,
        slideWhistle->disp->h - slideWhistle->radiostars.h - CORNER_OFFSET);
    drawText(
        slideWhistle->disp,
        &slideWhistle->radiostars, c444,
        holdText,
        afterText,
        slideWhistle->disp->h - slideWhistle->radiostars.h - CORNER_OFFSET);

    afterText = drawText(
                    slideWhistle->disp,
                    &slideWhistle->radiostars, c444,
                    "X~Y/",
                    slideWhistle->disp->w - textWidth(&slideWhistle->radiostars, playText) - textWidth(&slideWhistle->radiostars,
                            "X~Y/A") - CORNER_OFFSET,
                    slideWhistle->disp->h - slideWhistle->radiostars.h - CORNER_OFFSET);
    afterText = drawText(
                    slideWhistle->disp,
                    &slideWhistle->radiostars, c151,
                    "A",
                    afterText,
                    slideWhistle->disp->h - slideWhistle->radiostars.h - CORNER_OFFSET);
    drawText(
        slideWhistle->disp,
        &slideWhistle->radiostars, c444,
        playText,
        afterText,
        slideWhistle->disp->h - slideWhistle->radiostars.h - CORNER_OFFSET);
}

/**
 * @brief Plot a horizontal bar indicating where the note boundaries are
 *
 * @param yOffset The Y Offset of the middle of the bar, not the ticks
 */
void  plotBar(uint8_t yOffset)
{
    // Plot the main bar
    plotLine(slideWhistle->disp,
             BAR_X_MARGIN,
             yOffset,
             slideWhistle->disp->w - BAR_X_MARGIN,
             yOffset,
             c555, 0);

    // Plot tick marks at each of the note boundaries
    for(uint8_t tick = 0; tick < (scales[slideWhistle->scaleIdx].notesLen / NUM_OCTAVES) + 1; tick++)
    {
        int16_t x = BAR_X_MARGIN + ( (tick * ((slideWhistle->disp->w - 1) - (BAR_X_MARGIN * 2))) /
                                     (scales[slideWhistle->scaleIdx].notesLen / NUM_OCTAVES)) ;
        plotLine(slideWhistle->disp, x, yOffset - TICK_HEIGHT,
                 x, yOffset + TICK_HEIGHT,
                 c555, 0);
    }
}

/**
 * @brief Called every 1ms to handle the rhythm, arpeggios, and setting the buzzer
 *
 * @param arg unused
 */
void  slideWhistleBeatTimerFunc(void* arg __attribute__((unused)))
{
    // Keep track of time with microsecond precision
    uint32_t currentCallTimeUs = esp_timer_get_time();

    // These bools control what we should do
    bool shouldPlayNote = false;
    bool isFirstNote = false;
    bool isInterNotePause = false;

    // Figure out what to do based on the time
    if(0 == slideWhistle->lastCallTimeUs)
    {
        // Being called for the fist time, start playing the first note
        slideWhistle->timeUs = 0;
        slideWhistle->lastCallTimeUs = currentCallTimeUs;
        shouldPlayNote = true;
        isFirstNote = true;
    }
    else
    {
        // Figure out the delta between calls in microseconds, then increment time
        slideWhistle->timeUs += (currentCallTimeUs - slideWhistle->lastCallTimeUs);
        slideWhistle->lastCallTimeUs = currentCallTimeUs;

        // Find the time where we should change notes
        uint32_t rhythmIntervalUs = (1000 * bpms[slideWhistle->bpmIdx].bpmMultiplier * ((~REST_BIT) &
                                     rhythms[slideWhistle->rhythmIdx].rhythm[slideWhistle->rhythmNoteIdx].note));
        // If time crossed a rhythm boundary do something different
        if(slideWhistle->timeUs >= rhythmIntervalUs)
        {
            slideWhistle->timeUs -= rhythmIntervalUs;
            shouldPlayNote = true;
        }
        // Or if it crossed the boundary into an inter-note pause
        else if(slideWhistle->timeUs >= rhythmIntervalUs - (1000 * rhythms[slideWhistle->rhythmIdx].interNotePauseMs))
        {
            shouldPlayNote = true;
            isInterNotePause = true;
        }
        else
        {
            // Nothing to do here
            return;
        }
    }

    led_t leds[NUM_LEDS] = {{0}};
    // If this is an internote pause
    if(isInterNotePause)
    {
        // Turn off the buzzer and set the LEDs to dim
        stopNote();
        noteToColor(&leds[0], getCurrentNote(), 0x20);
    }
    else if(true == shouldPlayNote)
    {
        // If this isn't the first note (we need to start the first note sometime)
        if(false == isFirstNote)
        {
            // Move to play the next note
            slideWhistle->rhythmNoteIdx = (slideWhistle->rhythmNoteIdx + 1) % rhythms[slideWhistle->rhythmIdx].rhythmLen;
        }

        // If the button isn't held, or the BPM is being modified, or this is a rest
        if(!slideWhistle->shouldPlay ||
                (rhythms[slideWhistle->rhythmIdx].rhythm[slideWhistle->rhythmNoteIdx].note & REST_BIT))
        {
            // Turn off the buzzer and set the LEDs to dim
            stopNote();
            noteToColor(&leds[0], getCurrentNote(), 0x20);
        }
        else
        {
            // Arpeggiate as necessary
            playNote(arpModify(getCurrentNote(), rhythms[slideWhistle->rhythmIdx].rhythm[slideWhistle->rhythmNoteIdx].arp));

            // Set LEDs to bright
            noteToColor(&leds[0], getCurrentNote(), 0x40);
        }
    }
    else
    {
        return;
    }

    // Copy LED color from the first LED to all of them
    for(uint8_t ledIdx = 1; ledIdx < NUM_LEDS; ledIdx++)
    {
        leds[ledIdx].r = leds[0].r;
        leds[ledIdx].g = leds[0].g;
        leds[ledIdx].b = leds[0].b;
    }
    setLeds(leds, NUM_LEDS);
}

/**
 * @return the current X coordinate of the cursor
 * 
 */
int16_t  getCurrentCursorX(void)
{
    if(slideWhistle->touchHeld)
    {
        return slideWhistle->touchPosition;
    }
    else
    {
        return slideWhistle->roll;
    }
}

/**
 * @return the current Y coordinate of the cursor
 * 
 */
int16_t  getCurrentCursorY(void)
{
    return slideWhistle->disp->h - BAR_Y_MARGIN - getCurrentOctaveIdx() * BAR_Y_SPACING;
}

/**
 * @return the current octave index selected
 * 
 */
uint8_t  getCurrentOctaveIdx(void)
{
    if(slideWhistle->upHeld && !slideWhistle->downHeld)
    {
        return 2;
    }
    else if(slideWhistle->downHeld && !slideWhistle->upHeld)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/**
 * @return the current note the angle or touch position coresponds to.
 * doesn't matter if it should be played right now or not
 * 
 */
noteFrequency_t  getCurrentNote(void)
{
    if(slideWhistle->heldNote != SILENCE)
    {
        return slideWhistle->heldNote;
    }

    // Get the index of the note to play
    uint8_t noteIdx = (getCurrentCursorX() * (scales[slideWhistle->scaleIdx].notesLen / NUM_OCTAVES)) / (BAR_X_WIDTH + 1);

    // See which octave we should play
    if(slideWhistle->upHeld && !slideWhistle->downHeld)
    {
        uint8_t offset = scales[slideWhistle->scaleIdx].notesLen / NUM_OCTAVES;
        return scales[slideWhistle->scaleIdx].notes[noteIdx + (offset * 2)];
    }
    else if(slideWhistle->downHeld && !slideWhistle->upHeld)
    {
        return scales[slideWhistle->scaleIdx].notes[noteIdx];
    }
    else
    {
        uint8_t offset = scales[slideWhistle->scaleIdx].notesLen / NUM_OCTAVES;
        return scales[slideWhistle->scaleIdx].notes[noteIdx + offset];
    }
}

/**
 * @brief Arpeggiate a note
 *
 * @param note The root note to arpeggiate
 * @param arpInterval The interval to arpeggiate it by
 * @return noteFrequency_t The arpeggiated note
 */
noteFrequency_t  arpModify(noteFrequency_t note, int8_t arpInterval)
{
    // Don't need to do anything for these
    if(1 == arpInterval || 0 == arpInterval)
    {
        return note;
    }

    // First find the note in the list of all notes
    for(uint16_t i = 0; i < lengthof(allNotes); i++)
    {
        if(note == allNotes[i])
        {
            if(arpInterval < 0)
            {
                // Then shift down by arpInterval
                while(++arpInterval)
                {
                    i--;
                }
            }
            else if(arpInterval > 0)
            {
                // Then shift up by arpInterval
                while(--arpInterval)
                {
                    i++;
                }
            }

            // Then return the arpeggiated note
            return allNotes[i];
        }
    }
    return note;
}
