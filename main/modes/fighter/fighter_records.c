//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fighter_menu.h"
#include "fighter_records.h"
#include "nvs_manager.h"
#include "bresenham.h"

//==============================================================================
// Defines
//==============================================================================

#define X_MARGIN 20
#define Y_MARGIN 8

//==============================================================================
// Structs
//==============================================================================

// NVS blob layout
typedef struct
{
    int32_t multiplayerRecords[3][2];
    int32_t homerunRecords[3];
} fighterNvs_t;

typedef struct
{
    display_t* disp;
    font_t* font;
    fighterNvs_t records;
} fighterRecords_t;

//==============================================================================
// Variables
//==============================================================================

fighterRecords_t* fr;
const char fighterRecKey[] = "fight_rec";

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Initialize the display for fighter records
 *
 * @param disp The display to draw to
 * @param font The font to draw with
 */
void initFighterRecords(display_t* disp, font_t* font)
{
    // Allocate memory for this display
    fr = calloc(1, sizeof(fighterRecords_t));

    // Save pointers to display and font
    fr->disp = disp;
    fr->font = font;

    // Read records from NVS
    size_t len = sizeof(fighterNvs_t);
    if(false == readNvsBlob(fighterRecKey, &fr->records, &len))
    {
        writeNvsBlob(fighterRecKey, &fr->records, sizeof(fighterNvs_t));
    }
}

/**
 * @brief Free memory allocated for the displaying the fighter records
 */
void deinitFighterRecords(void)
{
    free(fr);
}

/**
 * @brief Draw the records to the display
 *
 * @param elapsedUs unused
 */
void fighterRecordsLoop(int64_t elapsedUs __attribute__((unused)))
{
    // Draw a dim blue background with a grey grid, same as melee menu
    for(int16_t y = 0; y < fr->disp->h; y++)
    {
        for(int16_t x = 0; x < fr->disp->w; x++)
        {
            if(((x % 12) == 0) || ((y % 12) == 0))
            {
                SET_PIXEL(fr->disp, x, y, c111); // Grid
            }
            else
            {
                SET_PIXEL(fr->disp, x, y, c001); // Background
            }
        }
    }

    // To lay out text
    int16_t yOff = 6;
    int16_t tWidth = 0;

    // Text colors
    paletteColor_t headerColor = c540;
    paletteColor_t entryColor = c431;

    // Temporary string to print records to
    char recordStr[16];

    // Must match order of fightingCharacter_t
    const char* charNames[] =
    {
        str_charKD,
        str_charSN,
        str_charBF
    };

    // Multiplayer Header
    tWidth = textWidth(fr->font, str_multiplayer);
    drawText(fr->disp, fr->font, headerColor, str_multiplayer, (fr->disp->w - tWidth) / 2, yOff);
    yOff += (fr->font->h + Y_MARGIN);

    // Multiplayer entries
    for(fightingCharacter_t idx = 0; idx < (sizeof(charNames) / sizeof(charNames[0])); idx++)
    {
        // Draw name
        drawText(fr->disp, fr->font, entryColor, charNames[idx], X_MARGIN, yOff);
        // Compose, measure, and draw record
        sprintf(recordStr, "%d-%d", fr->records.multiplayerRecords[idx][0], fr->records.multiplayerRecords[idx][1]);
        tWidth = textWidth(fr->font, recordStr);
        drawText(fr->disp, fr->font, entryColor, recordStr, fr->disp->w - tWidth - X_MARGIN, yOff);
        // Update Y offset
        yOff += (fr->font->h + Y_MARGIN);
    }

    // Draw a dividing line
    plotLine(fr->disp, 0, fr->disp->h / 2, fr->disp->w, fr->disp->h / 2, c333, 0);

    // Reset the Y offset
    yOff = (fr->disp->h / 2) + 6;

    // Homerun Contest Header
    tWidth = textWidth(fr->font, str_hrContest);
    drawText(fr->disp, fr->font, headerColor, str_hrContest, (fr->disp->w - tWidth) / 2, yOff);
    yOff += (fr->font->h + Y_MARGIN);

    // Homerun Contest entries
    for(fightingCharacter_t idx = 0; idx < (sizeof(charNames) / sizeof(charNames[0])); idx++)
    {
        // Draw name
        drawText(fr->disp, fr->font, entryColor, charNames[idx], X_MARGIN, yOff);
        // Compose, measure, and draw record
        sprintf(recordStr, "%dm", fr->records.homerunRecords[idx]);
        tWidth = textWidth(fr->font, recordStr);
        drawText(fr->disp, fr->font, entryColor, recordStr, fr->disp->w - tWidth - X_MARGIN, yOff);
        // Update Y offset
        yOff += (fr->font->h + Y_MARGIN);
    }
}

/**
 * Check if a home run distance is a new record and if so, write it to NVS
 * This may be called from outside sources and does not use fr
 *
 * @param character The character for this record
 * @param distance The distance the sandbag traveled
 * @return true if this is a new record, false if it is not
 */
bool checkHomerunRecord(fightingCharacter_t character, int32_t distance)
{
    // Read records from NVS
    fighterNvs_t fnvs;
    size_t len = sizeof(fighterNvs_t);
    if(false == readNvsBlob(fighterRecKey, &fnvs, &len))
    {
        // If it couldn't be read, assume all zeros
        memset(&fnvs, 0, sizeof(fighterNvs_t));
    }

    // If this is a new record
    if (distance > fnvs.homerunRecords[character])
    {
        // Write it to NVS
        fnvs.homerunRecords[character] = distance;
        writeNvsBlob(fighterRecKey, &fnvs, sizeof(fighterNvs_t));
        return true;
    }
    return false;
}
