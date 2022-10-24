/*
 * swadgeMusic.h
 *
 *  Created on: 24 Oct 2022
 *      Author: Bryce
 */

#ifndef SWADGE_MUSIC_H_
#define SWADGE_MUSIC_H_

#include "embeddedout.h"
#include "led_util.h"
#include "musical_buzzer.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define REST_BIT 0x10000 // Largest note is 144, which is 0b10010000

/// Helper macro to return an integer clamped within a range (MIN to MAX)
#define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
/// Helper macro to return the number of elements in an array
#define lengthof(x) (sizeof(x) / sizeof(x[0]))

/*==============================================================================
 * Enums
 *============================================================================*/

/*==============================================================================
 * Prototypes
 *============================================================================*/

void  noteToColor( led_t* led, noteFrequency_t note, uint8_t brightness);
char*  noteToStr(noteFrequency_t note);

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    // The parameter's name
    char* name;
    // The notes
    const noteFrequency_t* notes;
    const uint16_t notesLen;
} scale_t;

/*==============================================================================
 * Const Variables
 *============================================================================*/

// Text
const char scaleText[] =  "Start: Scale";
const char mutedText[] =  "Swadge is muted!";

// All the scales
#define NUM_OCTAVES 3
#define LOWER_OCTAVE

#ifdef LOWER_OCTAVE
const noteFrequency_t scl_M_Penta[] =
{
    C_4, D_4, E_4, G_4, A_4, C_5,
    C_5, D_5, E_5, G_5, A_5, C_6,
    C_6, D_6, E_6, G_6, A_6, C_7,
};
const noteFrequency_t scl_m_Penta[] =
{
    C_4, D_SHARP_4, F_4, G_4, A_SHARP_4, C_5,
    C_5, D_SHARP_5, F_5, G_5, A_SHARP_5, C_6,
    C_6, D_SHARP_6, F_6, G_6, A_SHARP_6, C_7,
};
const noteFrequency_t scl_m_Blues[] =
{
    C_4, D_SHARP_4, F_4, F_SHARP_4, G_4, A_SHARP_4, C_5,
    C_5, D_SHARP_5, F_5, F_SHARP_5, G_5, A_SHARP_5, C_6,
    C_6, D_SHARP_6, F_6, F_SHARP_6, G_6, A_SHARP_6, C_7,
};
const noteFrequency_t scl_M_Blues[] =
{
    C_4, D_4, D_SHARP_4, E_4, G_4, A_4, C_5,
    C_5, D_5, D_SHARP_5, E_5, G_5, A_5, C_6,
    C_6, D_6, D_SHARP_6, E_6, G_6, A_6, C_7,
};
const noteFrequency_t scl_Major[] =
{
    C_4, D_4, E_4, F_4, G_4, A_4, B_4, C_5,
    C_5, D_5, E_5, F_5, G_5, A_5, B_5, C_6,
    C_6, D_6, E_6, F_6, G_6, A_6, B_6, C_7,
};
const noteFrequency_t scl_Minor_Aeolian[] =
{
    C_4, D_4, D_SHARP_4, F_4, G_4, G_SHARP_4, A_SHARP_4, C_5,
    C_5, D_5, D_SHARP_5, F_5, G_5, G_SHARP_5, A_SHARP_5, C_6,
    C_6, D_6, D_SHARP_6, F_6, G_6, G_SHARP_6, A_SHARP_6, C_7,
};
const noteFrequency_t scl_Harm_Minor[] =
{
    C_4, D_4, D_SHARP_4, F_4, G_4, G_SHARP_4, B_4, C_5,
    C_5, D_5, D_SHARP_5, F_5, G_5, G_SHARP_5, B_5, C_6,
    C_6, D_6, D_SHARP_6, F_6, G_6, G_SHARP_6, B_6, C_7,
};
const noteFrequency_t scl_Dorian[] =
{
    C_4, D_4, D_SHARP_4, F_4, G_4, A_4, A_SHARP_4, C_5,
    C_5, D_5, D_SHARP_5, F_5, G_5, A_5, A_SHARP_5, C_6,
    C_6, D_6, D_SHARP_6, F_6, G_6, A_6, A_SHARP_6, C_7,
};
const noteFrequency_t scl_Phrygian[] =
{
    C_4, C_SHARP_4, D_SHARP_4, F_4, G_4, G_SHARP_4, A_SHARP_4, C_5,
    C_5, C_SHARP_5, D_SHARP_5, F_5, G_5, G_SHARP_5, A_SHARP_5, C_6,
    C_6, C_SHARP_6, D_SHARP_6, F_6, G_6, G_SHARP_6, A_SHARP_6, C_7,
};
const noteFrequency_t scl_Lydian[] =
{
    C_4, D_4, E_4, F_SHARP_4, G_4, A_4, B_4, C_5,
    C_5, D_5, E_5, F_SHARP_5, G_5, A_5, B_5, C_6,
    C_6, D_6, E_6, F_SHARP_6, G_6, A_6, B_6, C_7,
};
const noteFrequency_t scl_Mixolydian[] =
{
    C_4, D_4, E_4, F_4, G_4, A_4, A_SHARP_4, C_5,
    C_5, D_5, E_5, F_5, G_5, A_5, A_SHARP_5, C_6,
    C_6, D_6, E_6, F_6, G_6, A_6, A_SHARP_6, C_7,
};
const noteFrequency_t scl_Locrian[] =
{
    C_4, C_SHARP_4, D_SHARP_4, F_4, F_SHARP_4, G_SHARP_4, A_SHARP_4, C_5,
    C_5, C_SHARP_5, D_SHARP_5, F_5, F_SHARP_5, G_SHARP_5, A_SHARP_5, C_6,
    C_6, C_SHARP_6, D_SHARP_6, F_6, F_SHARP_6, G_SHARP_6, A_SHARP_6, C_7,
};
const noteFrequency_t scl_Dom_Bebop[] =
{
    C_4, D_4, E_4, F_4, G_4, A_4, A_SHARP_4, B_4, C_5,
    C_5, D_5, E_5, F_5, G_5, A_5, A_SHARP_5, B_5, C_6,
    C_6, D_6, E_6, F_6, G_6, A_6, A_SHARP_6, B_6, C_7,
};
const noteFrequency_t scl_M_Bebop[] =
{
    C_4, D_4, E_4, F_4, G_4, G_SHARP_4, A_SHARP_4, B_4, C_5,
    C_5, D_5, E_5, F_5, G_5, G_SHARP_5, A_SHARP_5, B_5, C_6,
    C_6, D_6, E_6, F_6, G_6, G_SHARP_6, A_SHARP_6, B_6, C_7,
};
const noteFrequency_t scl_Whole_Tone[] =
{
    C_4, D_4, E_4, F_SHARP_4, G_SHARP_4, A_SHARP_4, C_5,
    C_5, D_5, E_5, F_SHARP_5, G_SHARP_5, A_SHARP_5, C_6,
    C_6, D_6, E_6, F_SHARP_6, G_SHARP_6, A_SHARP_6, C_7,
};
const noteFrequency_t scl_Dacs[] =
{
    C_4, D_SHARP_4, F_4, F_SHARP_4, G_4, A_4,
    C_5, D_SHARP_5, F_5, F_SHARP_5, G_5, A_5,
    C_6, D_SHARP_6, F_6, F_SHARP_6, G_6, A_6,
};
const noteFrequency_t scl_Chromatic[] =
{
    C_4, C_SHARP_4, D_4, D_SHARP_4, E_4, F_4, F_SHARP_4, G_4, G_SHARP_4, A_4, A_SHARP_4, B_4, C_5,
    C_5, C_SHARP_5, D_5, D_SHARP_5, E_5, F_5, F_SHARP_5, G_5, G_SHARP_5, A_5, A_SHARP_5, B_5, C_6,
    C_6, C_SHARP_6, D_6, D_SHARP_6, E_6, F_6, F_SHARP_6, G_6, G_SHARP_6, A_6, A_SHARP_6, B_6, C_7,
};
#else
const noteFrequency_t scl_M_Penta[] =
{
    C_5, D_5, E_5, G_5, A_5, C_6,
    C_6, D_6, E_6, G_6, A_6, C_7,
    C_7, D_7, E_7, G_7, A_7, C_8,
};
const noteFrequency_t scl_m_Penta[] =
{
    C_5, D_SHARP_5, F_5, G_5, A_SHARP_5, C_6,
    C_6, D_SHARP_6, F_6, G_6, A_SHARP_6, C_7,
    C_7, D_SHARP_7, F_7, G_7, A_SHARP_7, C_8,
};
const noteFrequency_t scl_m_Blues[] =
{
    C_5, D_SHARP_5, F_5, F_SHARP_5, G_5, A_SHARP_5, C_6,
    C_6, D_SHARP_6, F_6, F_SHARP_6, G_6, A_SHARP_6, C_7,
    C_7, D_SHARP_7, F_7, F_SHARP_7, G_7, A_SHARP_7, C_8,
};
const noteFrequency_t scl_M_Blues[] =
{
    C_5, D_5, D_SHARP_5, E_5, G_5, A_5, C_6,
    C_6, D_6, D_SHARP_6, E_6, G_6, A_6, C_7,
    C_7, D_7, D_SHARP_7, E_7, G_7, A_7, C_8,
};
const noteFrequency_t scl_Major[] =
{
    C_5, D_5, E_5, F_5, G_5, A_5, B_5, C_6,
    C_6, D_6, E_6, F_6, G_6, A_6, B_6, C_7,
    C_7, D_7, E_7, F_7, G_7, A_7, B_7, C_8,
};
const noteFrequency_t scl_Minor_Aeolian[] =
{
    C_5, D_5, D_SHARP_5, F_5, G_5, G_SHARP_5, A_SHARP_5, C_6,
    C_6, D_6, D_SHARP_6, F_6, G_6, G_SHARP_6, A_SHARP_6, C_7,
    C_7, D_7, D_SHARP_7, F_7, G_7, G_SHARP_7, A_SHARP_7, C_8,
};
const noteFrequency_t scl_Harm_Minor[] =
{
    C_5, D_5, D_SHARP_5, F_5, G_5, G_SHARP_5, B_5, C_6,
    C_6, D_6, D_SHARP_6, F_6, G_6, G_SHARP_6, B_6, C_7,
    C_7, D_7, D_SHARP_7, F_7, G_7, G_SHARP_7, B_7, C_8,
};
const noteFrequency_t scl_Dorian[] =
{
    C_5, D_5, D_SHARP_5, F_5, G_5, A_5, A_SHARP_5, C_6,
    C_6, D_6, D_SHARP_6, F_6, G_6, A_6, A_SHARP_6, C_7,
    C_7, D_7, D_SHARP_7, F_7, G_7, A_7, A_SHARP_7, C_8,
};
const noteFrequency_t scl_Phrygian[] =
{
    C_5, C_SHARP_5, D_SHARP_5, F_5, G_5, G_SHARP_5, A_SHARP_5, C_6,
    C_6, C_SHARP_6, D_SHARP_6, F_6, G_6, G_SHARP_6, A_SHARP_6, C_7,
    C_7, C_SHARP_7, D_SHARP_7, F_7, G_7, G_SHARP_7, A_SHARP_7, C_8,
};
const noteFrequency_t scl_Lydian[] =
{
    C_5, D_5, E_5, F_SHARP_5, G_5, A_5, B_5, C_6,
    C_6, D_6, E_6, F_SHARP_6, G_6, A_6, B_6, C_7,
    C_7, D_7, E_7, F_SHARP_7, G_7, A_7, B_7, C_8,
};
const noteFrequency_t scl_Mixolydian[] =
{
    C_5, D_5, E_5, F_5, G_5, A_5, A_SHARP_5, C_6,
    C_6, D_6, E_6, F_6, G_6, A_6, A_SHARP_6, C_7,
    C_7, D_7, E_7, F_7, G_7, A_7, A_SHARP_7, C_8,
};
const noteFrequency_t scl_Locrian[] =
{
    C_5, C_SHARP_5, D_SHARP_5, F_5, F_SHARP_5, G_SHARP_5, A_SHARP_5, C_6,
    C_6, C_SHARP_6, D_SHARP_6, F_6, F_SHARP_6, G_SHARP_6, A_SHARP_6, C_7,
    C_7, C_SHARP_7, D_SHARP_7, F_7, F_SHARP_7, G_SHARP_7, A_SHARP_7, C_8,
};
const noteFrequency_t scl_Dom_Bebop[] =
{
    C_5, D_5, E_5, F_5, G_5, A_5, A_SHARP_5, B_5, C_6,
    C_6, D_6, E_6, F_6, G_6, A_6, A_SHARP_6, B_6, C_7,
    C_7, D_7, E_7, F_7, G_7, A_7, A_SHARP_7, B_7, C_8,
};
const noteFrequency_t scl_M_Bebop[] =
{
    C_5, D_5, E_5, F_5, G_5, G_SHARP_5, A_SHARP_5, B_5, C_6,
    C_6, D_6, E_6, F_6, G_6, G_SHARP_6, A_SHARP_6, B_6, C_7,
    C_7, D_7, E_7, F_7, G_7, G_SHARP_7, A_SHARP_7, B_7, C_8,
};
const noteFrequency_t scl_Whole_Tone[] =
{
    C_5, D_5, E_5, F_SHARP_5, G_SHARP_5, A_SHARP_5, C_6,
    C_6, D_6, E_6, F_SHARP_6, G_SHARP_6, A_SHARP_6, C_7,
    C_7, D_7, E_7, F_SHARP_7, G_SHARP_7, A_SHARP_7, C_8,
};
const noteFrequency_t scl_Dacs[] =
{
    C_5, D_SHARP_5, F_5, F_SHARP_5, G_5, A_5,
    C_6, D_SHARP_6, F_6, F_SHARP_6, G_6, A_6,
    C_7, D_SHARP_7, F_7, F_SHARP_7, G_7, A_7,
};
const noteFrequency_t scl_Chromatic[] =
{
    C_5, C_SHARP_5, D_5, D_SHARP_5, E_5, F_5, F_SHARP_5, G_5, G_SHARP_5, A_5, A_SHARP_5, B_5, C_6,
    C_6, C_SHARP_6, D_6, D_SHARP_6, E_6, F_6, F_SHARP_6, G_6, G_SHARP_6, A_6, A_SHARP_6, B_6, C_7,
    C_7, C_SHARP_7, D_7, D_SHARP_7, E_7, F_7, F_SHARP_7, G_7, G_SHARP_7, A_7, A_SHARP_7, B_7, C_8,
};
#endif

const scale_t scales[] =
{
    {
        .name = "Ma Pent",
        .notes = scl_M_Penta,
        .notesLen = lengthof(scl_M_Penta)
    },
    {
        .name = "mi Pent",
        .notes = scl_m_Penta,
        .notesLen = lengthof(scl_m_Penta)
    },
    {
        .name = "Ma Blu",
        .notes = scl_M_Blues,
        .notesLen = lengthof(scl_M_Blues)
    },
    {
        .name = "mi Blu",
        .notes = scl_m_Blues,
        .notesLen = lengthof(scl_m_Blues)
    },
    {
        .name = "Major",
        .notes = scl_Major,
        .notesLen = lengthof(scl_Major)
    },
    {
        .name = "Minor",
        .notes = scl_Minor_Aeolian,
        .notesLen = lengthof(scl_Minor_Aeolian)
    },
    {
        .name = "H Minor",
        .notes = scl_Harm_Minor,
        .notesLen = lengthof(scl_Harm_Minor)
    },
    {
        .name = "Dorian",
        .notes = scl_Dorian,
        .notesLen = lengthof(scl_Dorian)
    },
    {
        .name = "Phrygian",
        .notes = scl_Phrygian,
        .notesLen = lengthof(scl_Phrygian)
    },
    {
        .name = "Lydian",
        .notes = scl_Lydian,
        .notesLen = lengthof(scl_Lydian)
    },
    {
        .name = "Mixolyd",
        .notes = scl_Mixolydian,
        .notesLen = lengthof(scl_Mixolydian)
    },
    {
        .name = "Locrian",
        .notes = scl_Locrian,
        .notesLen = lengthof(scl_Locrian)
    },
    {
        .name = "D Bebop",
        .notes = scl_Dom_Bebop,
        .notesLen = lengthof(scl_Dom_Bebop)
    },
    {
        .name = "M Bebop",
        .notes = scl_M_Bebop,
        .notesLen = lengthof(scl_M_Bebop)
    },
    {
        .name = "Whole T",
        .notes = scl_Whole_Tone,
        .notesLen = lengthof(scl_Whole_Tone)
    },
    {
        .name = "DACs",
        .notes = scl_Dacs,
        .notesLen = lengthof(scl_Dacs)
    },
    {
        .name = "Chroma",
        .notes = scl_Chromatic,
        .notesLen = lengthof(scl_Chromatic)
    },
};

extern const noteFrequency_t allNotes[];

#endif /*SWADGE_MUSIC_H_*/