#ifndef _PAINT_COMMON_H_
#define _PAINT_COMMON_H_

#include <stddef.h>

#include "esp_log.h"
#include "swadgeMode.h"
#include "meleeMenu.h"
#include "led_util.h"
#include "p2pConnection.h"

#include "px_stack.h"
#include "paint_type.h"
#include "paint_brush.h"

#define PAINT_LOGV(...) ESP_LOGV("Paint", __VA_ARGS__)
#define PAINT_LOGD(...) ESP_LOGD("Paint", __VA_ARGS__)
#define PAINT_LOGI(...) ESP_LOGI("Paint", __VA_ARGS__)
#define PAINT_LOGW(...) ESP_LOGW("Paint", __VA_ARGS__)
#define PAINT_LOGE(...) ESP_LOGE("Paint", __VA_ARGS__)


// The total number of save slots available
#define PAINT_SAVE_SLOTS 8

// Whether to have the LEDs show the current colors
#define PAINT_ENABLE_LEDS (0x0001 << (PAINT_SAVE_SLOTS * 2))

// Whether to play SFX on drawing, etc.
#define PAINT_ENABLE_SFX  (0x0002 << (PAINT_SAVE_SLOTS * 2))

// Whether to play background music
#define PAINT_ENABLE_BGM  (0x0004 << (PAINT_SAVE_SLOTS * 2))

// Default to LEDs, SFX, and music on, with slot 0 marked as most recent
#define PAINT_DEFAULTS (PAINT_ENABLE_LEDS | PAINT_ENABLE_SFX | PAINT_ENABLE_BGM)

// Mask for the index that includes everything except the most-recent index
#define PAINT_MASK_NOT_RECENT (PAINT_ENABLE_LEDS | PAINT_ENABLE_SFX | PAINT_ENABLE_BGM | ((1 << PAINT_SAVE_SLOTS) - 1))

// The size of the buffer for loading/saving the image. Each chunk is saved as a separate blob in NVS
#define PAINT_SAVE_CHUNK_SIZE 1024

#define PAINT_SHARE_PX_PACKET_LEN (P2P_MAX_DATA_LEN - 3 - 11)
#define PAINT_SHARE_PX_PER_PACKET PAINT_SHARE_PX_PACKET_LEN * 2

#define PAINT_CANVAS_SCALE 3
#define PAINT_CANVAS_WIDTH 70
#define PAINT_CANVAS_HEIGHT 60

#define PAINT_CANVAS_X_OFFSET ((paintState->disp->w - PAINT_CANVAS_WIDTH * PAINT_CANVAS_SCALE) / 2)
#define PAINT_CANVAS_Y_OFFSET 26


#define PAINT_TOOLBAR_BG c333

// Set to 1 for horizontal colorboxes
#define PAINT_COLORBOX_HORIZONTAL 0

// Dimensions of the color boxes in the palette
#define PAINT_COLORBOX_W 9
#define PAINT_COLORBOX_H 9

#define PAINT_COLORBOX_SHADOW_TOP c444
#define PAINT_COLORBOX_SHADOW_BOTTOM c222

#define PAINT_COLORBOX_MARGIN_TOP 2
#define PAINT_COLORBOX_MARGIN_LEFT 2

// X and Y position of the active color boxes (foreground/background color)
#define PAINT_ACTIVE_COLOR_X ((PAINT_CANVAS_X_OFFSET - PAINT_COLORBOX_W - PAINT_COLORBOX_W / 2) / 2)
#define PAINT_ACTIVE_COLOR_Y (PAINT_CANVAS_Y_OFFSET - PAINT_COLORBOX_H / 2)

// X and Y position of the first palette color box
#define PAINT_COLORBOX_X ((PAINT_CANVAS_X_OFFSET - PAINT_COLORBOX_W) / 2)
#define PAINT_COLORBOX_Y (PAINT_ACTIVE_COLOR_Y + PAINT_COLORBOX_H * 2)


// Convert from canvas coordinates to screen coordinates
#define CNV2SCR_X(x) (PAINT_CANVAS_X_OFFSET + PAINT_CANVAS_SCALE * x)
#define CNV2SCR_Y(y) (PAINT_CANVAS_Y_OFFSET + PAINT_CANVAS_SCALE * y)

// Convert from world coordinates to canvas coordinates
#define SCR2CNV_X(x) ((x - PAINT_CANVAS_X_OFFSET) / PAINT_CANVAS_SCALE)
#define SCR2CNV_Y(y) ((y - PAINT_CANVAS_Y_OFFSET) / PAINT_CANVAS_SCALE)


// Calculates previous and next items with wraparound
#define PREV_WRAP(i, count) ((i) == 0 ? (count) - 1 : (i - 1))
#define NEXT_WRAP(i, count) ((i + 1) % count)


#define MAX_PICK_POINTS 16

// hold button for .3s to begin repeating
#define BUTTON_REPEAT_TIME 300000


typedef struct
{
    //////// General app data

    // Main Menu Font
    font_t menuFont;
    // Main Menu
    meleeMenu_t* menu;

    // Font for drawing tool info
    // TODO: Use images instead!
    font_t toolbarFont;

    display_t* disp;

    // The screen within paint that the user is in
    paintScreen_t screen;

    paintButtonMode_t buttonMode;

    // Whether or not A is currently pressed
    bool aHeld;

    // The representation of the drawing canvas
    // TODO: Replace canvasW and canvasH with this
    // TODO: Pass this instead of using PAINT_CANVAS_{X,Y}_OFFSET and global paintState->canvas{W,H}
    paintCanvas_t canvas;


    // Color data

    // The foreground color and the background color, which can be swapped with B
    paletteColor_t fgColor, bgColor;

    // The index of the currently selected color, while SELECT is held
    uint8_t paletteSelect;

    led_t leds[NUM_LEDS];


    //////// Brush / Tool data

    // Index of the currently selected brush / tool
    uint8_t brushIndex;

    // A pointer to the currently selected brush's definition for convenience
    const brush_t* brush;

    // The current brush width or variant, depending on the brush
    uint8_t brushWidth;


    //////// Pick Points

    // An array of points that have been selected for the current brush
    // TODO: Replace with a pxStack_t and get rid of the other `pxStack` maybe?
    point_t pickPoints[MAX_PICK_POINTS];

    // The number of points already selected
    size_t pickCount;

    // The saved pixels that are covered up by the pick points
    pxStack_t pxStack;

    // Whether all pick points should be redrawn with the current fgColor, for when the color changes while we're picking
    bool recolorPickPoints;


    //////// Cursor

    // The sprite for the currently selected cursor
    const wsg_t* cursorWsg;

    // The saved pixels thate rae covered up by the cursor
    pxStack_t cursorPxs;

    bool showCursor;

    // The number of canvas pixels to move the cursor this frame
    int8_t moveX, moveY;

    // The current position of the cursor in canvas pixels
    int16_t cursorX, cursorY;

    // The previous position of the cursor in canvas pixels
    // TODO: Remove this and just use `cursorPxs` instead
    int16_t lastCursorX, lastCursorY;

    // When true, this is the initial D-pad button down.
    // If set, the cursor will move by one pixel and then it will be cleared.
    bool firstMove;

    // The time a D-pad button has been held down for, in microseconds
    int64_t btnHoldTime;


    //////// Rendering flags

    // If set, the canvas will be cleared and the screen will be redrawn. Set on startup.
    bool clearScreen;

    // Set to redraw the toolbar on the next loop, when a brush or color is being selected
    bool redrawToolbar;


    //////// Save data flags

    // Whether to perform a save on the next loop
    bool doSave;

    // True when a save has been started but not yet completed. Prevents input while saving.
    bool saveInProgress;

    // The save slot selected when in BTN_MODE_SAVE
    uint8_t selectedSlot;

    paintSaveMenu_t saveMenu;

    // True if "Save" is selected, false if "Load" is selected
    bool isSaveSelected;

    // State for Yes/No in overwrite save menu.
    bool overwriteYesSelected;

    // Index keeping track of which slots are in use and the most recent slot
    int32_t index;


    //////// Share / Receive flags

    // The save slot being displayed / shared
    uint8_t shareSaveSlot;

    paintShareState_t shareState;

    bool shareAcked;
    bool connectionStarted;

    // For the sender, the sequence number of the current packet being sent / waiting for ack
    uint16_t shareSeqNum;

    uint8_t sharePacket[P2P_MAX_DATA_LEN];
    uint8_t sharePacketLen;

    uint8_t sharePaletteMap[256];

    // Set to true when a new packet has been written to sharePacket, either to be sent or to be handled
    bool shareNewPacket;

    // Flag for updating the screen
    bool shareUpdateScreen;

    p2pInfo p2pInfo;

    int64_t shareTime;

} paintMenu_t;


extern paintMenu_t* paintState;


#endif

