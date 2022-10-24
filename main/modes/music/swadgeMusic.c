/*
 * swadgeMusic.c
 *
 *  Created on: 24 Oct 2022
 *      Author: Bryce
 */

#include "swadgeMusic.h"

const noteFrequency_t allNotes[] =
{
    C_0,
    C_SHARP_0,
    D_0,
    D_SHARP_0,
    E_0,
    F_0,
    F_SHARP_0,
    G_0,
    G_SHARP_0,
    A_0,
    A_SHARP_0,
    B_0,

    C_1,
    C_SHARP_1,
    D_1,
    D_SHARP_1,
    E_1,
    F_1,
    F_SHARP_1,
    G_1,
    G_SHARP_1,
    A_1,
    A_SHARP_1,
    B_1,

    C_2,
    C_SHARP_2,
    D_2,
    D_SHARP_2,
    E_2,
    F_2,
    F_SHARP_2,
    G_2,
    G_SHARP_2,
    A_2,
    A_SHARP_2,
    B_2,

    C_3,
    C_SHARP_3,
    D_3,
    D_SHARP_3,
    E_3,
    F_3,
    F_SHARP_3,
    G_3,
    G_SHARP_3,
    A_3,
    A_SHARP_3,
    B_3,

    C_4,
    C_SHARP_4,
    D_4,
    D_SHARP_4,
    E_4,
    F_4,
    F_SHARP_4,
    G_4,
    G_SHARP_4,
    A_4,
    A_SHARP_4,
    B_4,

    C_5,
    C_SHARP_5,
    D_5,
    D_SHARP_5,
    E_5,
    F_5,
    F_SHARP_5,
    G_5,
    G_SHARP_5,
    A_5,
    A_SHARP_5,
    B_5,

    C_6,
    C_SHARP_6,
    D_6,
    D_SHARP_6,
    E_6,
    F_6,
    F_SHARP_6,
    G_6,
    G_SHARP_6,
    A_6,
    A_SHARP_6,
    B_6,

    C_7,
    C_SHARP_7,
    D_7,
    D_SHARP_7,
    E_7,
    F_7,
    F_SHARP_7,
    G_7,
    G_SHARP_7,
    A_7,
    A_SHARP_7,
    B_7,

    C_8,
    C_SHARP_8,
    D_8,
    D_SHARP_8,
    E_8,
    F_8,
    F_SHARP_8,
    G_8,
    G_SHARP_8,
    A_8,
    A_SHARP_8,
    B_8,

    C_9,
    C_SHARP_9,
    D_9,
    D_SHARP_9,
    E_9,
    F_9,
    F_SHARP_9,
    G_9,
    G_SHARP_9,
    A_9,
    A_SHARP_9,
    B_9,

    C_10,
    C_SHARP_10,
    D_10,
    D_SHARP_10,
    E_10,
    F_10,
    F_SHARP_10,
    G_10,
    G_SHARP_10,
    A_10,
    A_SHARP_10,
    B_10,
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * @brief Translate a musical note to a color
 *
 * @param led The led_t to write the color data to
 * @param note The note to translate to color
 * @param brightness The brightness of the LED
 */
void  noteToColor( led_t* led, noteFrequency_t note, uint8_t brightness)
{
    // First find the note in the list of all notes
    for(uint16_t idx = 0; idx < lengthof(allNotes); idx++)
    {
        if(note == allNotes[idx])
        {
            idx = idx % 12;
            idx = (idx * 255) / 12;

            uint32_t col = ECCtoHEX(idx, 0xFF, brightness);
            led->r = (col >> 16) & 0xFF;
            led->g = (col >>  8) & 0xFF;
            led->b = (col >>  0) & 0xFF;
            return;
        }
    }
}

/**
 * @brief Translate a musical note to a string. Only covers the notes we play
 *
 * @param note The note to translate to color
 * @return A null terminated string for this note
 */
char*  noteToStr(noteFrequency_t note)
{
    switch(note)
    {
        case SILENCE:
        {
            return "SILENCE";
        }
        case C_0:
        {
            return "C0";
        }
        case C_SHARP_0:
        {
            return "C#0";
        }
        case D_0:
        {
            return "D0";
        }
        case D_SHARP_0:
        {
            return "D#0";
        }
        case E_0:
        {
            return "E0";
        }
        case F_0:
        {
            return "F0";
        }
        case F_SHARP_0:
        {
            return "F#0";
        }
        case G_0:
        {
            return "G0";
        }
        case G_SHARP_0:
        {
            return "G#0";
        }
        case A_0:
        {
            return "A0";
        }
        case A_SHARP_0:
        {
            return "A#0";
        }
        case B_0:
        {
            return "B0";
        }
        case C_1:
        {
            return "C1";
        }
        case C_SHARP_1:
        {
            return "C#1";
        }
        case D_1:
        {
            return "D1";
        }
        case D_SHARP_1:
        {
            return "D#1";
        }
        case E_1:
        {
            return "E1";
        }
        case F_1:
        {
            return "F1";
        }
        case F_SHARP_1:
        {
            return "F#1";
        }
        case G_1:
        {
            return "G1";
        }
        case G_SHARP_1:
        {
            return "G#1";
        }
        case A_1:
        {
            return "A1";
        }
        case A_SHARP_1:
        {
            return "A#1";
        }
        case B_1:
        {
            return "B1";
        }
        case C_2:
        {
            return "C2";
        }
        case C_SHARP_2:
        {
            return "C#2";
        }
        case D_2:
        {
            return "D2";
        }
        case D_SHARP_2:
        {
            return "D#2";
        }
        case E_2:
        {
            return "E2";
        }
        case F_2:
        {
            return "F2";
        }
        case F_SHARP_2:
        {
            return "F#2";
        }
        case G_2:
        {
            return "G2";
        }
        case G_SHARP_2:
        {
            return "G#2";
        }
        case A_2:
        {
            return "A2";
        }
        case A_SHARP_2:
        {
            return "A#2";
        }
        case B_2:
        {
            return "B2";
        }
        case C_3:
        {
            return "C3";
        }
        case C_SHARP_3:
        {
            return "C#3";
        }
        case D_3:
        {
            return "D3";
        }
        case D_SHARP_3:
        {
            return "D#3";
        }
        case E_3:
        {
            return "E3";
        }
        case F_3:
        {
            return "F3";
        }
        case F_SHARP_3:
        {
            return "F#3";
        }
        case G_3:
        {
            return "G3";
        }
        case G_SHARP_3:
        {
            return "G#3";
        }
        case A_3:
        {
            return "A3";
        }
        case A_SHARP_3:
        {
            return "A#3";
        }
        case B_3:
        {
            return "B3";
        }
        case C_4:
        {
            return "C4";
        }
        case C_SHARP_4:
        {
            return "C#4";
        }
        case D_4:
        {
            return "D4";
        }
        case D_SHARP_4:
        {
            return "D#4";
        }
        case E_4:
        {
            return "E4";
        }
        case F_4:
        {
            return "F4";
        }
        case F_SHARP_4:
        {
            return "F#4";
        }
        case G_4:
        {
            return "G4";
        }
        case G_SHARP_4:
        {
            return "G#4";
        }
        case A_4:
        {
            return "A4";
        }
        case A_SHARP_4:
        {
            return "A#4";
        }
        case B_4:
        {
            return "B4";
        }
        case C_5:
        {
            return "C5";
        }
        case C_SHARP_5:
        {
            return "C#5";
        }
        case D_5:
        {
            return "D5";
        }
        case D_SHARP_5:
        {
            return "D#5";
        }
        case E_5:
        {
            return "E5";
        }
        case F_5:
        {
            return "F5";
        }
        case F_SHARP_5:
        {
            return "F#5";
        }
        case G_5:
        {
            return "G5";
        }
        case G_SHARP_5:
        {
            return "G#5";
        }
        case A_5:
        {
            return "A5";
        }
        case A_SHARP_5:
        {
            return "A#5";
        }
        case B_5:
        {
            return "B5";
        }
        case C_6:
        {
            return "C6";
        }
        case C_SHARP_6:
        {
            return "C#6";
        }
        case D_6:
        {
            return "D6";
        }
        case D_SHARP_6:
        {
            return "D#6";
        }
        case E_6:
        {
            return "E6";
        }
        case F_6:
        {
            return "F6";
        }
        case F_SHARP_6:
        {
            return "F#6";
        }
        case G_6:
        {
            return "G6";
        }
        case G_SHARP_6:
        {
            return "G#6";
        }
        case A_6:
        {
            return "A6";
        }
        case A_SHARP_6:
        {
            return "A#6";
        }
        case B_6:
        {
            return "B6";
        }
        case C_7:
        {
            return "C7";
        }
        case C_SHARP_7:
        {
            return "C#7";
        }
        case D_7:
        {
            return "D7";
        }
        case D_SHARP_7:
        {
            return "D#7";
        }
        case E_7:
        {
            return "E7";
        }
        case F_7:
        {
            return "F7";
        }
        case F_SHARP_7:
        {
            return "F#7";
        }
        case G_7:
        {
            return "G7";
        }
        case G_SHARP_7:
        {
            return "G#7";
        }
        case A_7:
        {
            return "A7";
        }
        case A_SHARP_7:
        {
            return "A#7";
        }
        case B_7:
        {
            return "B7";
        }
        case C_8:
        {
            return "C8";
        }
        case C_SHARP_8:
        {
            return "C#8";
        }
        case D_8:
        {
            return "D8";
        }
        case D_SHARP_8:
        {
            return "D#8";
        }
        case E_8:
        {
            return "E8";
        }
        case F_8:
        {
            return "F8";
        }
        case F_SHARP_8:
        {
            return "F#8";
        }
        case G_8:
        {
            return "G8";
        }
        case G_SHARP_8:
        {
            return "G#8";
        }
        case A_8:
        {
            return "A8";
        }
        case A_SHARP_8:
        {
            return "A#8";
        }
        case B_8:
        {
            return "B8";
        }
        case C_9:
        {
            return "C9";
        }
        case C_SHARP_9:
        {
            return "C#9";
        }
        case D_9:
        {
            return "D9";
        }
        case D_SHARP_9:
        {
            return "D#9";
        }
        case E_9:
        {
            return "E9";
        }
        case F_9:
        {
            return "F9";
        }
        case F_SHARP_9:
        {
            return "F#9";
        }
        case G_9:
        {
            return "G9";
        }
        case G_SHARP_9:
        {
            return "G#9";
        }
        case A_9:
        {
            return "A9";
        }
        case A_SHARP_9:
        {
            return "A#9";
        }
        case B_9:
        {
            return "B9";
        }
        case C_10:
        {
            return "C10";
        }
        case C_SHARP_10:
        {
            return "C#10";
        }
        case D_10:
        {
            return "D10";
        }
        case D_SHARP_10:
        {
            return "D#10";
        }
        case E_10:
        {
            return "E10";
        }
        case F_10:
        {
            return "F10";
        }
        case F_SHARP_10:
        {
            return "F#10";
        }
        case G_10:
        {
            return "G10";
        }
        case G_SHARP_10:
        {
            return "G#10";
        }
        case A_10:
        {
            return "A10";
        }
        case A_SHARP_10:
        {
            return "A#10";
        }
        case B_10:
        {
            return "B10";
        }
        default:
        {
            return "";
        }
    }
}
