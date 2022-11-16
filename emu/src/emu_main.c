//==============================================================================
// Includes
//==============================================================================

#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>

#ifdef __linux__
#include <execinfo.h>
#include <signal.h>
#endif

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "swadge_esp32.h"
#include "swadgeMode.h"
#include "mode_main_menu.h"
#include "btn.h"

#include "list.h"

#include "emu_esp.h"
#include "sound.h"
#include "emu_display.h"
#include "emu_sound.h"
#include "emu_sensors.h"
#include "emu_main.h"

#include "fighter_menu.h"
#include "jumper_menu.h"
#include "mode_colorchord.h"
#include "mode_credits.h"
#include "mode_dance.h"
#include "mode_flight.h"
#include "mode_gamepad.h"
#include "mode_main_menu.h"
#include "mode_slide_whistle.h"
#include "mode_test.h"
#include "mode_tiltrads.h"
#include "mode_tunernome.h"
#include "mode_paint.h"
#include "paint_share.h"
#include "paint_share.h"
#include "picross_menu.h"
#include "mode_platformer.h"
#include "mode_jukebox.h"
#include "mode_diceroller.h"
#include "mode_bee.h"

//Make it so we don't need to include any other C files in our build.
#define CNFG_IMPLEMENTATION
#define CNFGOGL
#include "rawdraw_sf.h"

//==============================================================================
// Defines
//==============================================================================

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MIN_LED_WIDTH 64

#define BG_COLOR  0x191919FF // This color isn't part of the palette
#define DIV_COLOR 0x808080FF

extern char* emuNvsFilename;


static const char dvorakKeysP1[] = {',', 'o', 'a', 'e', 'n', 't', 'r', 'c'};
static const char dvorakKeysP2[] = {'y', 'i', 'u', 'd', 'm', 'b', 'p', 'f'};

// A list of all modes
swadgeMode * allModes[] =
{
    &modeFighter,
    &modeJumper,
    &modeColorchord,
    &modeCredits,
    &modeDance,
    &modeFlight,
    &modeGamepad,
    &modeMainMenu,
    &modeSlideWhistle,
    &modeTest,
    &modeTiltrads,
    &modeTunernome,
    &modePaint,
    &modePaintShare,
    &modePaintReceive,
    &modePicross,
    &modePlatformer,
    &modeDiceRoller,
    &modeJukebox,
    &modeBee,
};

//==============================================================================
// Function prototypes
//==============================================================================

void drawBitmapPixel(uint32_t* bitmapDisplay, int w, int h, int x, int y, uint32_t col);
void plotRoundedCorners(uint32_t* bitmapDisplay, int w, int h, int r, uint32_t col);

#ifdef __linux__
void init_crashSignals(void);
void signalHandler_crash(int signum, siginfo_t* si, void* vcontext);
#endif

//==============================================================================
// Variables
//==============================================================================

static bool isRunning = true;

//==============================================================================
// Functions
//==============================================================================

/**
 * This function must be provided for rawdraw. Key events are received here
 *
 * @param keycode The key code, a lowercase ascii char
 * @param bDown true if the key was pressed, false if it was released
 */
void HandleKey( int keycode, int bDown )
{
    emuSensorHandleKey(keycode, bDown);
}

/**
 * @brief Handle mouse click events from rawdraw
 *
 * @param x The x coordinate of the mouse event
 * @param y The y coordinate of the mouse event
 * @param button The mouse button that was pressed or released
 * @param bDown true if the button was pressed, false if it was released
 */
void HandleButton( int x UNUSED, int y UNUSED, int button UNUSED, int bDown UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Handle mouse motion events from rawdraw
 *
 * @param x The x coordinate of the mouse event
 * @param y The y coordinate of the mouse event
 * @param mask A mask of mouse buttons that are currently held down
 */
void HandleMotion( int x UNUSED, int y UNUSED, int mask UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Free memory on exit
 */
void HandleDestroy()
{
    // Stop the main loop
    isRunning = false;

    // Cleanup swadge stuff
    cleanupOnExit();

    // Then free display memory
    deinitDisplayMemory();

    // Close sound
    deinitSound();
}

/**
 * @brief Helper function to draw to a bitmap display
 *
 * @param bitmapDisplay The display to draw to
 * @param w The width of the display
 * @param h The height of the display
 * @param x The X coordinate to draw a pixel at
 * @param y The Y coordinate to draw a pixel at
 * @param col The color to draw the pixel
 */
void drawBitmapPixel(uint32_t* bitmapDisplay, int w, int h, int x, int y, uint32_t col)
{
    if((y * w) + x < (w * h))
    {
        bitmapDisplay[(y * w) + x] = col;
    }
}

/**
 * @brief Helper functions to draw rounded corners to the display
 *
 * @param bitmapDisplay The display to round the corners on
 * @param w The width of the display
 * @param h The height of the display
 * @param r The radius of the rounded corners
 * @param col The color to draw the rounded corners
 */
void plotRoundedCorners(uint32_t* bitmapDisplay, int w, int h, int r, uint32_t col)
{
    int or = r;
    int x = -r, y = 0, err = 2 - 2 * r; /* bottom left to top right */
    do
    {
        for(int xLine = 0; xLine <= (or + x); xLine++)
        {
            drawBitmapPixel(bitmapDisplay, w, h,     xLine    , h - (or - y) - 1, col); /* I.   Quadrant -x -y */
            drawBitmapPixel(bitmapDisplay, w, h, w - xLine - 1, h - (or - y) - 1, col); /* II.  Quadrant +x -y */
            drawBitmapPixel(bitmapDisplay, w, h,     xLine    ,     (or - y)    , col); /* III. Quadrant -x -y */
            drawBitmapPixel(bitmapDisplay, w, h, w - xLine - 1,     (or - y)    , col); /* IV.  Quadrant +x -y */
        }

        r = err;
        if (r <= y)
        {
            err += ++y * 2 + 1;    /* e_xy+e_y < 0 */
        }
        if (r > x || err > y) /* e_xy+e_x > 0 or no 2nd y-step */
        {
            err += ++x * 2 + 1;    /* -> x-step now */
        }
    } while (x < 0);
}

int strCommonPrefixLen(const char* a, const char* b)
{
    if (a == NULL || b == NULL)
    {
        return 0;
    }

    int i = 0;

    for (; *a && *b && tolower(*a) == tolower(*b); a++, b++, i++);
    return i;
}

int getButtonIndex(const char* text, char** end)
{
    const char* const keyMap[] =
    {
        "up",
        "down",
        "left",
        "right",
        "a",
        "b",
        "start",
        "select",
    };

    for (uint8_t i = 0; i < sizeof(keyMap) / sizeof(keyMap[0]); i++)
    {
        // Check if the name of the current key matches the one in the map
        int prefix = strCommonPrefixLen(keyMap[i], text);
        if (prefix == strlen(keyMap[i]))
        {
            if (end)
            {
                *end = text + prefix;
            }
            return i;
        }
    }

    return -1;
}

int getTouchIndex(const char* text, char** end)
{
    // doubled up so we can do either 1 or Y, and X or 5
    // we'll just modulo it
    const char* const touchMap[] =
    {
        "1",
        "2",
        "3",
        "4",
        "5",

        "y",
        "2",
        "3",
        "4",
        "x",
    };

    for (uint8_t i = 0; i < sizeof(touchMap) / sizeof(*touchMap); i++)
    {
        int prefix = strCommonPrefixLen(touchMap[i], text);
        if (prefix > 0)
        {
            if (end)
            {
                *end = text + prefix;
            }
            return i % 5;
        }
    }

    return -1;
}

bool handleFuzzButtons(const char* buttons)
{
    const char* cur = buttons;
    bool found = false;
    int buttonIndex, touchIndex;

    fuzzKeyCount = 0;
    fuzzTouchCount = 0;

    while (*cur)
    {
        found = false;
        buttonIndex = getButtonIndex(cur, &cur);

        if (buttonIndex >= 0)
        {
            fuzzKeysP1[fuzzKeyCount] = keyButtonsP1[buttonIndex];
            fuzzKeysP2[fuzzKeyCount++] = keyButtonsP2[buttonIndex];
            found = true;
        }

        if (!found)
        {
            touchIndex = getTouchIndex(cur, &cur);
            if (touchIndex >= 0)
            {
                fuzzTouchP1[fuzzTouchCount] = keyTouchP1[touchIndex];
                fuzzTouchP2[fuzzTouchCount++] = keyTouchP2[touchIndex];
                found = true;
            }
        }

        if (!found)
        {
            fprintf(stderr, "ERROR: Unknown key in fuzz buttons list at %s\n", cur);
            return false;
        }

        while (*cur == ',' || *cur == ' ') cur++;
    }

    return true;
}

bool parseKeyConfig(const char* config, char* outKeys, char* outTouch)
{
    const char* cur = config;
    const char* colon;
    const char* comma;
    const char* escape;

    // Each loop will process one key -> button/touchpad mapping
    while (*cur)
    {
        colon = strchr(cur, ':');
        comma = strchr(cur, ',');
        escape = strchr(cur, '\\');

        while (escape && escape == colon - 1)
        {
            // escape char is immediately before a colon, so skip that colon
            colon = strchr(colon + 1, ':');
        }

        while (escape && escape == comma - 1)
        {
            // escape char is immediately before a comma, so skip that comma
            comma = strchr(comma + 1, ',');
        }

        if (cur == escape)
        {
            cur++;

            if (!*cur)
            {
                fprintf(stderr, "ERROR: Trailing '\\' at end of keybind string\n");
            }
        }

        // Expect the comma to be immediately after the key bind character
        if (colon && colon - cur > 1)
        {
            fprintf(stderr, "ERROR: Key binding must only be one character, was %zu at '%s'\n", colon - cur, cur);
            return false;
        }

        if (colon && (!comma || colon < comma))
        {
            // colon is right after the key
            char key = *cur;

            cur = colon + 1;

            if (!*cur)
            {
                fprintf(stderr, "ERROR: Unexpected end of keybind string, expecting ':' after %c\n", key);
                return false;
            }

            bool found = false;
            if (outKeys)
            {
                int buttonIndex = getButtonIndex(cur, &cur);

                if (buttonIndex >= 0)
                {
                    outKeys[buttonIndex] = key;
                    found = true;
                }
            }

            if (!found && outTouch)
            {
                int keyIndex = getTouchIndex(cur, &cur);
                if (keyIndex >= 0)
                {
                    outTouch[keyIndex] = key;
                    found = true;
                }
            }

            if (!found)
            {
                fprintf(stderr, "ERROR: Unknown button name in keybinding string at %s\n", cur);
                return false;
            }

            // Move to past the comma
            if (comma)
            {
                cur = comma + 1;
            }

            // Skip trailing spaces
            while (*cur && *cur == ' ')
            {
                cur++;
            }
        }
        else
        {
            fprintf(stderr, "ERROR: Expecting `:` and button name after %s\n", cur);
            return false;
        }
    }

    return true;
}



static const struct option opts[] = {
    {"start-mode", required_argument, NULL, 'm'},
    {"lock", no_argument, &lockMode, true},
    {"fuzz", no_argument, &monkeyAround, true},
    {"fuzz-mode-timer", required_argument, NULL, 't'},
    {"fuzz-buttons", required_argument, NULL, 0},
    {"fuzz-buttons-p2", required_argument, NULL, 0},
    {"fuzz-button-delay", required_argument, NULL, 0},
    {"fuzz-button-prob", required_argument, NULL, 0},
    {"nvs-file", required_argument, NULL, 'f'},
    {"keys", required_argument, NULL, 'k'},
    {"keys-p2", required_argument, NULL, 'p'},
    {"dvorak", no_argument, NULL, 0},
    {"help", no_argument, NULL, 'h'},

    {NULL, 0, NULL, 0},
};

void handleArgs(int argc, char** argv)
{
    char* executableName = *argv;

    char* startMode = NULL;
    char* fuzzButtons = NULL;
    bool fuzzP2 = true;
    char* p1Keys = NULL;
    char* p2Keys = NULL;

    int optVal, optIndex;

    while (true)
    {
        optVal = getopt_long(argc, argv, "m:lt:f:k:p:h", opts, &optIndex);

        if (optVal < 0)
        {
            // No more options
            break;
        }

        switch (optVal)
        {
            case 0:
            {
                // Handle options without a short opt
                switch (optIndex)
                {
                    // Dvorak
                    case 11:
                        memcpy(keyButtonsP1, dvorakKeysP1, sizeof(dvorakKeysP1) / sizeof(*dvorakKeysP1));
                        memcpy(keyButtonsP2, dvorakKeysP2, sizeof(dvorakKeysP2) / sizeof(*dvorakKeysP2));
                    break;

                    // Fuzz Buttons
                    case 4:
                        // Handle it later
                        fuzzButtons = optarg;
                    break;

                    // Fuzz Buttons P2
                    case 5:
                        // Lazily handle yes/no
                        if (optarg && (optarg[0] == 'N' || optarg[0] == 'n'))
                        {
                            fuzzP2 = false;
                        }
                    break;

                    // Fuzz Button Delay
                    case 6:

                    break;

                    // Fuzz Button Probability
                    case 7:
                    break;
                }
                break;
            }

            case 'm':
            {
                startMode = optarg;
                break;
            }

            case 't':
            {
                fuzzerModeTestTime = atoi(optarg) * 1000000;
                resetToMenuTimer = fuzzerModeTestTime;

                if (fuzzerModeTestTime <= 0)
                {
                    fprintf(stderr, "ERROR: Invalid numeric argument for option %s: '%s'\n", argv[optind - 2], optarg);
                    exit(1);
                    return;
                }
                break;
            }

            case 'f':
            {
                emuNvsFilename = optarg;
                break;
            }

            case 'k':
            {
                p1Keys = optarg;
                break;
            }

            case 'p':
            {
                p2Keys = optarg;
                break;
            }

            case 'h':
            {
                printf("Usage: %s [--start-mode|-m MODE] [--lock|-l] [--help] [--fuzz [--fuzz-mode-timer SECONDS]] [--nvs-file|-f FILE]\n", executableName);
                printf("\n");
                printf("\t--start-mode MODE\tStarts the emulator in the mode named MODE, instead of the main menu\n");
                printf("\t--lock\t\t\tLocks the emulator in the start mode. Start + Select will do nothing, and if --start-mode is used, it will replace the main menu.\n");
                printf("\t--fuzz\t\t\tEnables fuzzing mode, which will trigger rapid random button presses and randomly switch modes, unless --lock is passed.\n");
                printf("\t--fuzz-mode-timer SECONDS\tSets the number of seconds before the fuzzer will switch to a different random mode.\n");

                printf("\t--fuzz-buttons BUTTONS\tSets the list of buttons the fuzzer can press. Defaults to all buttons.\n");
                printf("\t--fuzz-buttons-p2 YES|NO\tSets whether the P2 buttons will be fuzzed also. Defaults to YES.\n");
                printf("\t--fuzz-button-delay MILLIS\tSets the number of milliseconds between each possible fuzzer keypress. Defaults to 100.\n");
                printf("\t--fuzz-button-prob NUM\tSets the probability that a keypress will happen, from 0 to 100. Defaults to 100.\n");

                printf("\t--nvs-file FILE\tSets the name of the JSON file used to store NVS data. Defaults to 'nvs.json' in the current directory.\n");
                printf("\t--keys KEYBINDINGS\tSets one or more keybindings, in the format of `<key>:<button>,<key>:<button>,...`, where <key> is a single character,\n"
                            "\t\t\tand <button> is one of UP, DOWN, LEFT, RIGHT, A, B, START, SELECT, X, Y, 1, 2, 3, 4, or 5, with X, Y, and 1-5 being the touchpad segments.\n"
                            "\t\tWhitespace is ignored. To use ',', ':', ' ', or '\\' as the keybinding, prefix them with a backslash, e.g. `--keys '\ :A, \\:B, \,:UP, \::DOWN`\n");
                printf("\t--keys-p2 KEYBINDINGS\tSets keybindings for player 2. Requires the same format as in --keys.\n");
                printf("\t--dvorak\t\tSets keybindings for the Dvorak layout which are equivalent to the default QWERTY keybinings.\n");
                printf("\n");
                exit(0);
                return;
            }

            case '?':
            default:
            {
                // getopt already printed error message
                exit(1);
                return;
            }
        }
    }

    // Handle keybindings
    // P1
    if (p1Keys != NULL)
    {
        if (!parseKeyConfig(p1Keys, keyButtonsP1, keyTouchP1))
        {
            // parseKeyConfig already printed error message
            exit(1);
            return;
        }
    }

    // P2
    if (p2Keys != NULL)
    {
        if (!parseKeyConfig(p2Keys, keyButtonsP2, keyTouchP2))
        {
            // parseKeyConfig already printed error message
            exit(1);
            return;
        }
    }

    // Handle fuzzer keys
    if (fuzzButtons)
    {
        if (!handleFuzzButtons(fuzzButtons))
        {
            exit(1);
            return;
        }
    }
    else
    {
        // Set defaults
        memcpy(fuzzKeysP1, keyButtonsP1, sizeof(keyButtonsP1) / sizeof(keyButtonsP1[0]));
        memcpy(fuzzKeysP2, keyButtonsP2, sizeof(keyButtonsP2) / sizeof(keyButtonsP2[0]));
        fuzzKeyCount = sizeof(keyButtonsP1) / sizeof(keyButtonsP1);

        memcpy(fuzzTouchP1, keyTouchP1, sizeof(keyTouchP1) / sizeof(keyTouchP1[0]));
        memcpy(fuzzTouchP2, keyTouchP2, sizeof(keyTouchP2) / sizeof(keyTouchP2[0]));
        fuzzTouchCount = sizeof(keyTouchP1) / sizeof(keyTouchP1[0]);
    }

    if (startMode != NULL)
    {
        bool found = false;
        for (uint8_t i = 0; i < sizeof(allModes) / sizeof(*allModes); i++)
        {
            swadgeMode* mode = allModes[i];
            if (!strcmp(mode->modeName, startMode) || (!strcmp(startMode, "Pi-cross") && mode == &modePicross))
            {
                found = true;
                // Set the initial mode to this one
                switchToSwadgeMode(mode);

                // If --lock was passed, also replace the main menu with the initial mode
                if (lockMode)
                {
                    memcpy(&modeMainMenu, mode, sizeof(swadgeMode));
                }
                break;
            }
        }

        if (!found)
        {
            fprintf(stderr, "ERROR: No mode named '%s'\n", startMode);
            fprintf(stderr, "Possible modes:\n");
            for (uint8_t i = 0; i < sizeof(allModes) / sizeof(*allModes); i++)
            {
                fprintf(stderr, "  - %s\n", (&modePicross == allModes[i]) ? "Pi-cross" : allModes[i]->modeName);
            }
            exit(1);
            return;
        }
    }
}

/**
 * @brief The main emulator function. This initializes rawdraw and calls
 * app_main(), then spins in a loop updating the rawdraw UI
 *
 * @param argc unused
 * @param argv unused
 * @return 0 on success, a nonzero value for any errors
 */
int main(int argc, char** argv)
{
#ifdef __linux__
    init_crashSignals();
#endif

    handleArgs(argc, argv);

    // First initialize rawdraw
    // Screen-specific configurations
    // Save window dimensions from the last loop
    CNFGSetup( "SQUAREWAVEBIRD Simulator", (TFT_WIDTH * 2) + (MIN_LED_WIDTH * 4) + 2, (TFT_HEIGHT * 2));

    // This is the 'main' that gets called when the ESP boots. It does not return
    app_main();
}

/**
 * Handle key input and drawing the display
 */
void emu_loop(void)
{
    // These are persistent!
    static short lastWindow_w = 0;
    static short lastWindow_h = 0;
    static int16_t led_w = MIN_LED_WIDTH;

    if (monkeyAround)
    {
        // A list of all keys to randomly press or release, and their stat
        static bool keyState[sizeof(keyButtonsP1) / sizeof(keyButtonsP1[0]) + sizeof(keyTouchP1) / sizeof(keyTouchP1[0])] = {false};

        // Time keeping
        static int64_t tLastCall = 0;
        if(0 == tLastCall)
        {
            tLastCall = esp_timer_get_time();
        }
        int64_t tNow = esp_timer_get_time();
        int64_t tElapsed = (tNow - tLastCall);

        if (fuzzKeyCount + fuzzTouchCount > 0)
        {
            // Randomly press or release keys every 100ms
            static int64_t keyTimer = 0;
            keyTimer += tElapsed;
            while(keyTimer >= fuzzButtonDelay)
            {
                keyTimer -= fuzzButtonDelay;
                if ((esp_random() % 100) > fuzzButtonProbability)
                {
                    continue;
                }

                int keyIdx = esp_random() % (fuzzKeyCount + fuzzTouchCount);
                keyState[keyIdx] = !keyState[keyIdx];
                if (keyIdx < fuzzKeyCount)
                {
                    emuSensorHandleKey(fuzzKeysP1[keyIdx], keyState[keyIdx]);
                }
                else
                {
                    emuSensorHandleKey(fuzzTouchP1[keyIdx - fuzzKeyCount], keyState[keyIdx]);
                }

                // Only handle non-touchpads for p2
                if(keyIdx < fuzzKeyCount)
                {
                    emuSensorHandleKey(fuzzKeysP2[keyIdx], keyState[keyIdx]);
                }
            }
        }

        // Change the swadge mode two minutes
        resetToMenuTimer += tElapsed;
        while(resetToMenuTimer >= fuzzerModeTestTime)
        {
            resetToMenuTimer -= fuzzerModeTestTime;
            static int modeIdx = (sizeof(allModes) / sizeof(allModes[0])) - 1;
            modeIdx = (modeIdx + 1) % (sizeof(allModes) / sizeof(allModes[0]));
            switchToSwadgeModeFuzzer(allModes[modeIdx]);
        }

        // Timekeeping
        tLastCall = tNow;
    }

    // Always handle inputs
    CNFGHandleInput();

    // If not running anymore, don't handle graphics
    // Must be checked after handling input, before graphics
    if(!isRunning)
    {
        exit(0);
        return;
    }

    // Grey Background
    CNFGBGColor = BG_COLOR;
    CNFGClearFrame();

    // Get the current window dimensions
    short window_w, window_h;
    CNFGGetDimensions(&window_w, &window_h);

    // If the dimensions changed
    if((lastWindow_h != window_h) || (lastWindow_w != window_w))
    {
        // Figure out how much the TFT should be scaled by
        uint8_t widthMult = (window_w - (4 * MIN_LED_WIDTH) - 2) / TFT_WIDTH;
        if(0 == widthMult)
        {
            widthMult = 1;
        }
        uint8_t heightMult = window_h / TFT_HEIGHT;
        if(0 == heightMult)
        {
            heightMult = 1;
        }
        uint8_t screenMult = MIN(widthMult, heightMult);

        // LEDs take up the rest of the horizontal space
        led_w = (window_w - 2 - (screenMult * TFT_WIDTH)) / 4;

        // Set the multiplier
        setDisplayBitmapMultiplier(screenMult);

        // Save for the next loop
        lastWindow_w = window_w;
        lastWindow_h = window_h;
    }

    // Get the LED memory
    uint8_t numLeds;
    led_t* leds = getLedMemory(&numLeds);

    // Where LEDs are drawn, kinda
    const int16_t ledOffsets[8][2] =
    {
        {1, 2},
        {0, 3},
        {0, 1},
        {1, 0},
        {2, 0},
        {3, 1},
        {3, 3},
        {2, 2},
    };

    // Draw simulated LEDs
    if (numLeds > 0 && NULL != leds)
    {
        short led_h = window_h / (numLeds / 2);
        for(int i = 0; i < numLeds; i++)
        {
            CNFGColor( (leds[i].r << 24) | (leds[i].g << 16) | (leds[i].b << 8) | 0xFF);

            int16_t xOffset = 0;
            if(ledOffsets[i][0] < 2)
            {
                xOffset = ledOffsets[i][0] * (led_w / 2);
            }
            else
            {
                xOffset = window_w - led_w - ((4 - ledOffsets[i][0]) * (led_w / 2));
            }

            int16_t yOffset = ledOffsets[i][1] * led_h;

            // Draw the LED
            CNFGTackRectangle(
                xOffset, yOffset,
                xOffset + (led_w * 3) / 2, yOffset + led_h);
        }
    }

    // Draw dividing lines
    CNFGColor( DIV_COLOR );
    CNFGTackSegment(led_w * 2, 0, led_w * 2, window_h);
    CNFGTackSegment(window_w - (led_w * 2), 0, window_w - (led_w * 2), window_h);

    // Get the display memory
    uint16_t bitmapWidth, bitmapHeight;
    uint32_t* bitmapDisplay = getDisplayBitmap(&bitmapWidth, &bitmapHeight);

    if((0 != bitmapWidth) && (0 != bitmapHeight) && (NULL != bitmapDisplay))
    {
#if defined(CONFIG_GC9307_240x280)
        plotRoundedCorners(bitmapDisplay, bitmapWidth, bitmapHeight, (bitmapWidth / TFT_WIDTH) * 40, BG_COLOR);
#endif
        // Update the display, centered
        CNFGBlitImage(bitmapDisplay,
                        (led_w * 2) + 1, (window_h - bitmapHeight) / 2,
                        bitmapWidth, bitmapHeight);
    }

    //Display the image and wait for time to display next frame.
    CNFGSwapBuffers();
}

#ifdef __linux__

/**
 * @brief Initialize a crash handler, only for Linux
 */
void init_crashSignals(void)
{
    const int sigs[] = {SIGSEGV, SIGBUS, SIGILL, SIGSYS, SIGABRT, SIGFPE, SIGIOT, SIGTRAP};
    for(int i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++)
    {
        struct sigaction action;
        memset(&action, 0, sizeof(struct sigaction));
        action.sa_flags = SA_SIGINFO;
        action.sa_sigaction = signalHandler_crash;
        sigaction(sigs[i], &action, NULL);
    }
}

/**
 * @brief Print a backtrace when a crash is caught, only for Linux
 * 
 * @param signum 
 * @param si 
 * @param vcontext 
 */
void signalHandler_crash(int signum, siginfo_t* si, void* vcontext)
{
	char msg[128] = {'\0'};
	ssize_t result;

    char fname[64] = {0};
    sprintf(fname, "crash-%ld.txt", time(NULL));
    int dumpFileDescriptor = open(fname, O_RDWR | O_CREAT | O_TRUNC,
									S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	if(-1 != dumpFileDescriptor)
	{
		snprintf(msg, sizeof(msg), "Signal %d received!\nsigno: %d, errno %d\n, code %d\n", signum, si->si_signo, si->si_errno, si->si_code);
		result = write(dumpFileDescriptor, msg, strnlen(msg, sizeof(msg)));
		(void)result;

        memset(msg, 0, sizeof(msg));
        for(int i = 0; i < __SI_PAD_SIZE; i++)
        {
            char tmp[8];
            sprintf(tmp, "%02X", si->_sifields._pad[i]);
            strcat(msg, tmp);
        }
        strcat(msg, "\n");
		result = write(dumpFileDescriptor, msg, strnlen(msg, sizeof(msg)));
		(void)result;
        
        // Print backtrace
        void *array[128];
		size_t size = backtrace(array, (sizeof(array) / sizeof(array[0])));
		backtrace_symbols_fd(array, size, dumpFileDescriptor);
		close(dumpFileDescriptor);
	}

	// Exit
	_exit(1);
}
#endif
