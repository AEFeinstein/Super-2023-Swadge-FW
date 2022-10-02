/*============================================================================
 * Includes
 *==========================================================================*/

#include "text_entry.h"
#include "display.h"
#include "bresenham.h"
#include "btn.h"
#include <string.h>

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    NO_SHIFT,
    SHIFT,
    CAPS_LOCK,
    SPECIAL_DONE
} keyModifier_t;

typedef enum
{
    KEY_SHIFT     = 0x01,
    KEY_CAPSLOCK  = 0x02,
    KEY_BACKSPACE = 0x03,
    KEY_SPACE     = 0x04,
    KEY_EOL       = 0x05,
    KEY_TAB       = 0x09,
    KEY_ENTER     = 0x0A,
} controlChar_t;

/*============================================================================
 * Variables
 *==========================================================================*/

static font_t textEntryIBM;
static int texLen;
static char* texString;
static keyModifier_t keyMod;
static int8_t selx;
static int8_t sely;
static char selChar;
static uint8_t cursorTimer;
static display_t * textEntryDisplay;
#define WHITE 215

#define KB_LINES 5

// See controlChar_t
static const char keyboard_upper[] = "\
~!@#$%^&*()_+\x03\x05\
\x09QWERTYUIOP{}|\x05\
\002ASDFGHJKL:\"\x0a\x05\
\x01ZXCVBNM<>?\x01\x05\
\x04";

// See controlChar_t
static const char keyboard_lower[] = "\
`1234567890-=\x03\x05\
\x09qwertyuiop[]\\\x05\
\002asdfghjkl;\'\x0a\x05\
\x01zxcvbnm,./\x01\x05\
\x04";

static const uint8_t lengthperline[] = { 14, 14, 13, 12, 1 };

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize the text entry
 *
 * @param max_len The length of buffer
 * @param buffer  A char* to store the entered text in
 */
void textEntryStart( display_t * usedisp, int max_len, char* buffer )
{
    textEntryDisplay = usedisp;
    texLen = max_len;
    texString = buffer;
    keyMod = NO_SHIFT;
    texString[0] = 0;
    cursorTimer = 0;
    if( !textEntryIBM.h )
    {
        loadFont("ibm_vga8.font", &textEntryIBM);
    }
}

/**
 * Finish the text entry by disarming the cursor blink timer
 */
void textEntryEnd( void )
{
}

/**
 * Draw the text entry UI
 *
 * @return true if text entry is still being used
 *         false if text entry is finished
 */
bool textEntryDraw(void)
{
    // If we're done, return false
    if( keyMod == SPECIAL_DONE )
    {
        return false;
    }

    // Draw the text entered so far
    {
        const int16_t text_h = 32;
        int16_t textLen = textWidth(&textEntryIBM, texString) + textEntryIBM.chars[0].w;
        int16_t endPos = drawText( textEntryDisplay, &textEntryIBM, WHITE, texString, (textEntryDisplay->w - textLen)/2, text_h);
    
        // If the blinky cursor should be shown, draw it
        if( (cursorTimer++) & 0x10 )
        {
            plotLine( textEntryDisplay, endPos + 1, text_h-2, endPos + 1, text_h + textEntryIBM.h + 1, WHITE, 0 );
        }
    }

    // Draw an indicator for the current key modifier
    switch(keyMod)
    {
        case SHIFT:
        {
            int16_t width = textWidth(&textEntryIBM, "Typing: Upper");
            int16_t typingWidth = textWidth(&textEntryIBM, "Typing: ");
            drawText( textEntryDisplay, &textEntryIBM, WHITE, "Typing: Upper", (textEntryDisplay->w - width)/2, textEntryDisplay->h - textEntryIBM.h - 2);
            plotLine( textEntryDisplay, (textEntryDisplay->w - width)/2 + typingWidth, textEntryDisplay->h - 1, textEntryDisplay->w - 1, textEntryDisplay->h - 1, WHITE, 0);
            break;
        }
        case NO_SHIFT:
        {
            int16_t width = textWidth(&textEntryIBM, "Typing: Lower");
            drawText(textEntryDisplay, &textEntryIBM, WHITE,  "Typing: Lower", (textEntryDisplay->w - width)/2, textEntryDisplay->h - textEntryIBM.h - 2);
            break;
        }
        case CAPS_LOCK:
        {
            int16_t width = textWidth(&textEntryIBM, "Typing: CAPS LOCK");
            int16_t typingWidth = textWidth(&textEntryIBM, "Typing: ");
            drawText(textEntryDisplay, &textEntryIBM, WHITE, "Typing: CAPS LOCK", (textEntryDisplay->w - width)/2, textEntryDisplay->h - textEntryIBM.h - 2 );
            plotLine(textEntryDisplay, (textEntryDisplay->w - width)/2 + typingWidth, textEntryDisplay->h - 1, textEntryDisplay->w - 1, textEntryDisplay->h - 1, WHITE, 0);
            break;
        }
        default:
        case SPECIAL_DONE:
        {
            break;
        }
    }

    // Draw the keyboard
    int x = 0;
    int y = 0;
    char c;
    const char* s = (keyMod == NO_SHIFT) ? keyboard_lower : keyboard_upper;
    while( (c = *s) )
    {
        // EOL character hit, move to the next row
        if( c == KEY_EOL )
        {
            x = 0;
            y++;
        }
        else
        {
            char sts[] = {c, 0};
            int posx = x * 14 + 44 + y*4;
            int posy = y * 14 + 49;
            int width = 9;
            // Draw the character, may be a control char
            switch( c )
            {
                case KEY_SHIFT:
                case KEY_CAPSLOCK:
                {
                    // Draw shift/capslock
                    plotLine( textEntryDisplay, posx, posy + 4, posx + 2, posy + 4, WHITE, 0 );
                    plotLine( textEntryDisplay, posx + 1, posy + 4, posx + 1, posy, WHITE, 0 );
                    plotLine( textEntryDisplay, posx + 1, posy, posx + 3, posy + 2, WHITE, 0 );
                    plotLine( textEntryDisplay, posx + 1, posy, posx - 1, posy + 2, WHITE, 0 );
                    break;
                }
                case KEY_BACKSPACE:
                {
                    // Draw backspace
                    plotLine( textEntryDisplay, posx - 1, posy + 2, posx + 3, posy + 2, WHITE, 0 );
                    plotLine( textEntryDisplay, posx - 1, posy + 2, posx + 1, posy + 0, WHITE, 0 );
                    plotLine( textEntryDisplay, posx - 1, posy + 2, posx + 1, posy + 4, WHITE, 0 );
                    break;
                }
                case KEY_SPACE:
                {
                    // Draw spacebar
                    plotRect( textEntryDisplay, posx + 1, posy + 1, posx + 160, posy + 3, WHITE);
                    width = 161;
                    break;
                }
                case KEY_TAB:
                {
                    // Draw tab
                    plotLine( textEntryDisplay, posx - 1, posy + 2, posx + 3, posy + 2, WHITE, 0 );
                    plotLine( textEntryDisplay, posx + 3, posy + 2, posx + 1, posy + 0, WHITE, 0 );
                    plotLine( textEntryDisplay, posx + 3, posy + 2, posx + 1, posy + 4, WHITE, 0 );
                    plotLine( textEntryDisplay, posx - 1, posy + 0, posx - 1, posy + 4, WHITE, 0 );
                    break;
                }
                case KEY_ENTER:
                {
                    // Draw an OK for enter

                    drawText( textEntryDisplay, &textEntryIBM, WHITE, "OK", posx, posy );
                    width = textWidth(& textEntryIBM, "OK");
                    break;
                }
                default:
                {
                    // Just draw the char
                    drawText(textEntryDisplay, &textEntryIBM, WHITE, sts, posx, posy );
                }
            }
            if( x == selx && y == sely )
            {
                //Draw Box around selected item.
                plotRect( textEntryDisplay, posx - 2, posy - 2, posx + width, posy + 13, WHITE );
                selChar = c;
            }
            x++;
        }
        s++;
    }
    return true;
}

/**
 * handle button input for text entry
 *
 * @param down   true if the button was pressed, false if it was released
 * @param button The button that was pressed
 * @return true if text entry is still ongoing
 *         false if the enter key was pressed and text entry is done
 */
bool textEntryInput( uint8_t down, uint8_t button )
{
    // If this was a release, just return true
    if( !down )
    {
        return true;
    }

    // If text entry is done, return false
    if( keyMod == SPECIAL_DONE )
    {
        return false;
    }

    // Handle the button
    switch( button )
    {
        case BTN_A:
        case BTN_B:
        {
            // User selected this key
            int stringLen = strlen(texString);
            switch( selChar )
            {
                case KEY_ENTER:
                {
                    // Enter key was pressed, so text entry is done
                    keyMod = SPECIAL_DONE;
                    return false;
                }
                case KEY_SHIFT:
                case KEY_CAPSLOCK:
                {
                    // Rotate the keyMod from NO_SHIFT -> SHIFT -> CAPS LOCK, and back
                    if(NO_SHIFT == keyMod)
                    {
                        keyMod = SHIFT;
                    }
                    else if(SHIFT == keyMod)
                    {
                        keyMod = CAPS_LOCK;
                    }
                    else
                    {
                        keyMod = NO_SHIFT;
                    }
                    break;
                }
                case KEY_BACKSPACE:
                {
                    // If there is any text, delete the last char
                    if(stringLen > 0)
                    {
                        texString[stringLen - 1] = 0;
                    }
                    break;
                }
                default:
                {
                    // If there is still space, add the selected char
                    if( stringLen < texLen - 1 )
                    {
                        texString[stringLen] = selChar;
                        texString[stringLen + 1] = 0;

                        // Clear shift if it as active
                        if( keyMod == SHIFT )
                        {
                            keyMod = NO_SHIFT;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case LEFT:
        {
            // Move cursor
            selx--;
            break;
        }
        case DOWN:
        {
            // Move cursor
            sely++;
            break;
        }
        case RIGHT:
        {
            // Move cursor
            selx++;
            break;
        }
        case UP:
        {
            // Move cursor
            sely--;
            break;
        }
        default:
        {
            break;
        }
    }

    // Make sure the cursor is in bounds, wrap around if necessary
    if( sely < 0 )
    {
        sely = KB_LINES - 1;
    }
    else if( sely >= KB_LINES )
    {
        sely = 0;
    }

    // Make sure the cursor is in bounds, wrap around if necessary
    if( selx < 0 )
    {
        selx = lengthperline[sely] - 1;
    }
    else if( selx >= lengthperline[sely] )
    {
        selx = 0;
    }

    // All done, still entering text
    return true;
}
