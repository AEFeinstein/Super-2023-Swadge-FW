/*
 * mode_swadgestrument.c
 *
 *  Created on: 23 Oct 2022
 *      Author: Bryce
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

#include "mode_swadgestrument.h"

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
#define BAR_Y_MARGIN (swadgestrument->radiostars.h + CURSOR_HEIGHT + 2 + CORNER_OFFSET * 2)
#define BAR_X_WIDTH (swadgestrument->disp->w - (2 * BAR_X_MARGIN) - 1)
#define BAR_Y_SPACING (2 * CURSOR_HEIGHT + 5)

#define DEFAULT_PAUSE 5

/*==============================================================================
 * Enums
 *============================================================================*/

/*==============================================================================
 * Prototypes
 *============================================================================*/

void  swadgestrumentEnterMode(display_t* disp);
void  swadgestrumentExitMode(void);
void  swadgestrumentButtonCallback(buttonEvt_t* evt);
void  swadgestrumentTouchCallback(touch_event_t* evt);
void  swadgestrumentMainLoop(int64_t elapsedUs);
noteFrequency_t  arpModify(noteFrequency_t note, int8_t arpInterval);
uint8_t  getCurrentOctaveIdx(void);
noteFrequency_t  getCurrentNote(void);

/*==============================================================================
 * Structs
 *============================================================================*/

/*==============================================================================
 * Variables
 *============================================================================*/

// The swadge mode
swadgeMode modeSwadgestrument =
{
    .modeName = "Swadgestrument",
    .fnEnterMode = swadgestrumentEnterMode,
    .fnExitMode = swadgestrumentExitMode,
    .fnButtonCallback = swadgestrumentButtonCallback,
    .fnTouchCallback = swadgestrumentTouchCallback,
    .fnMainLoop = swadgestrumentMainLoop,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
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

    // Track touch
    bool touchHeld;
    int32_t touchPosition;
    int32_t touchIntensity;

    // Track the button
    bool shouldPlay;
    uint8_t keyIdx;
    uint8_t scaleIdx;
} swadgestrument_t;

swadgestrument_t* swadgestrument;

/*==============================================================================
 * Const Variables
 *============================================================================*/

// Text
const char keyText[] = "Select: Key";

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for swadgestrument
 */
void  swadgestrumentEnterMode(display_t* disp)
{
    // Allocate zero'd memory for the mode
    swadgestrument = calloc(1, sizeof(swadgestrument_t));

    swadgestrument->disp = disp;

    loadFont("ibm_vga8.font", &swadgestrument->ibm_vga8);
    loadFont("radiostars.font", &swadgestrument->radiostars);
    loadFont("mm.font", &swadgestrument->mm);

    stopNote();
}

/**
 * Called when swadgestrument is exited
 */
void  swadgestrumentExitMode(void)
{
    stopNote();

    freeFont(&swadgestrument->ibm_vga8);
    freeFont(&swadgestrument->radiostars);
    freeFont(&swadgestrument->mm);

    free(swadgestrument);
}

/**
 * This function is called when a button press is pressed. Buttons are
 * handled by interrupts and queued up for this callback, so there are no
 * strict timing restrictions for this function.
 *
 * @param evt The button event that occurred
 */
void  swadgestrumentTouchCallback(touch_event_t* evt)
{
    swadgestrument->touchHeld = evt->state != 0;
    swadgestrument->shouldPlay = evt->state != 0;
    //swadgestrument->touchPosition = roundf((evt->position * BAR_X_WIDTH) / 255);
}

/**
 * @brief Button callback function, plays notes and switches parameters
 *
 * @param evt The button event that occurred
 */
void  swadgestrumentButtonCallback(buttonEvt_t* evt)
{
    switch(evt->button)
    {
        case START:
        {
            if(evt->down)
            {
                // Cycle the scale
                swadgestrument->scaleIdx = (swadgestrument->scaleIdx + 1) % lengthof(scales);
            }
            break;
        }
        case SELECT:
        {
            if(evt->down)
            {
                // Cycle the key
                //swadgestrument->keyIdx = (swadgestrument->keyIdx + 1) % lengthof(keys);
            }

            break;
        }
        case BTN_A:
        {
            swadgestrument->shouldPlay = evt->down || swadgestrument->touchHeld;
            break;
        }
        case BTN_B:
        {
            if(evt->down)
            {
            }
            else
            {
            }
            break;
        }
        case UP:
        {
            break;
        }
        case DOWN:
        {
            break;
        }
        case LEFT:
        {
            if(evt->down)
            {
            }
            break;
        }
        case RIGHT:
        {
            if(evt->down)
            {
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
 * Update the display by drawing the current state of affairs
 */
void  swadgestrumentMainLoop(int64_t elapsedUs)
{
    swadgestrument->disp->clearPx();
    fillDisplayArea(swadgestrument->disp, 0, 0, swadgestrument->disp->w, swadgestrument->disp->h, c100);

    // Plot instructions
    drawText(
            swadgestrument->disp,
            &swadgestrument->ibm_vga8, c235,
            "bbbbb",
            (swadgestrument->disp->w - textWidth(&swadgestrument->ibm_vga8, "bbbbb")) / 2,
            TOP_TEXT_X_MARGIN);
    
    int16_t secondLineStartY = swadgestrument->ibm_vga8.h + LINE_BREAK_Y + TOP_TEXT_X_MARGIN;

    // Plot the key
    drawText(
        swadgestrument->disp,
        &swadgestrument->radiostars, c444,
        keyText,
        CORNER_OFFSET,
        secondLineStartY);
    /*drawText(
        swadgestrument->disp,
        &swadgestrument->radiostars, c555,
        keys[swadgestrument->keyIdx].name,
        swadgestrument->disp->w - textWidth(&swadgestrument->radiostars, keys[swadgestrument->keyIdx].name) - CORNER_OFFSET,
        secondLineStartY);*/

    // Plot the scale
    drawText(
        swadgestrument->disp,
        &swadgestrument->radiostars, c444,
        scaleText,
        CORNER_OFFSET,
        secondLineStartY + swadgestrument->radiostars.h + LINE_BREAK_Y);
    drawText(
        swadgestrument->disp,
        &swadgestrument->radiostars, c555,
        scales[swadgestrument->scaleIdx].name,
        swadgestrument->disp->w - textWidth(&swadgestrument->radiostars, scales[swadgestrument->scaleIdx].name) - CORNER_OFFSET,
        secondLineStartY + swadgestrument->radiostars.h + LINE_BREAK_Y);

    // Plot the BPM
    drawText(
        swadgestrument->disp,
        &swadgestrument->radiostars, c444,
        "aaaaa",
        CORNER_OFFSET,
        secondLineStartY + (swadgestrument->radiostars.h + LINE_BREAK_Y) * 2);

    char bpmStr[16] = {0};
    snprintf(bpmStr, sizeof(bpmStr), "%d", 0);
    drawText(
        swadgestrument->disp,
        &swadgestrument->radiostars, c555,
        bpmStr,
        swadgestrument->disp->w - textWidth(&swadgestrument->radiostars, bpmStr) - CORNER_OFFSET,
        secondLineStartY + (swadgestrument->radiostars.h + LINE_BREAK_Y) * 2);

    // Warn the user that the swadge is muted, if that's the case
    if(getSfxIsMuted())
    {
        drawText(
            swadgestrument->disp,
            &swadgestrument->radiostars, c551,
            mutedText,
            (swadgestrument->disp->w - textWidth(&swadgestrument->radiostars, mutedText)) / 2,
            swadgestrument->disp->h / 2);
    }
    else
    {
        // Plot the note
        drawText(swadgestrument->disp, &swadgestrument->mm, c555, noteToStr(getCurrentNote()),
                 (swadgestrument->disp->w - textWidth(&swadgestrument->mm, noteToStr(getCurrentNote()))) / 2, swadgestrument->disp->h / 2);
    }

    // Plot the button funcs
    int16_t afterText = drawText(
        swadgestrument->disp,
        &swadgestrument->radiostars, c511,
        "B",
        CORNER_OFFSET,
        swadgestrument->disp->h - swadgestrument->radiostars.h - CORNER_OFFSET);
    drawText(
        swadgestrument->disp,
        &swadgestrument->radiostars, c444,
        "ccccc",
        afterText,
        swadgestrument->disp->h - swadgestrument->radiostars.h - CORNER_OFFSET);

    afterText = drawText(
                    swadgestrument->disp,
                    &swadgestrument->radiostars, c444,
                    "X~Y/",
                    swadgestrument->disp->w - textWidth(&swadgestrument->radiostars, "ddddd") - textWidth(&swadgestrument->radiostars,
                            "X~Y/A") - CORNER_OFFSET,
                    swadgestrument->disp->h - swadgestrument->radiostars.h - CORNER_OFFSET);
    afterText = drawText(
                    swadgestrument->disp,
                    &swadgestrument->radiostars, c151,
                    "A",
                    afterText,
                    swadgestrument->disp->h - swadgestrument->radiostars.h - CORNER_OFFSET);
    drawText(
        swadgestrument->disp,
        &swadgestrument->radiostars, c444,
        "eeeee",
        afterText,
        swadgestrument->disp->h - swadgestrument->radiostars.h - CORNER_OFFSET);
}

/**
 * @return the current octave index selected
 * 
 */
uint8_t  getCurrentOctaveIdx(void)
{
    return 0;
}

/**
 * @return the current note the angle or touch position coresponds to.
 * doesn't matter if it should be played right now or not
 * 
 */
noteFrequency_t  getCurrentNote(void)
{
    return SILENCE;
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
