#include "paint_gallery.h"

#include "mode_dance.h"
#include "settingsManager.h"

#include "mode_paint.h"
#include "paint_common.h"
#include "paint_nvs.h"
#include "paint_util.h"

#include <string.h>

static const char transitionTime[] = "Slideshow: %g sec";
static const char transitionOff[] = "Slideshow: Off";
static const char transitionTimeNvsKey[] = "paint_gal_time";
static const char danceIndexKey[] = "paint_dance_idx";


// Possible transition times, in ms
static const uint16_t transitionTimeMap[] = {
    0, // off
    500,
    1000,
    2000,
    5000,
    10000,
    15000,
    30000,
    45000,
    60000,
};

// Denotes 5s
#define DEFAULT_TRANSITION_INDEX 4

#define US_PER_SEC 1000000
#define US_PER_MS 1000

// 3s for info text to stay up
#define GALLERY_INFO_TIME 3000000
#define GALLERY_INFO_Y_MARGIN 13
#define GALLERY_INFO_X_MARGIN 13
#define GALLERY_ARROW_MARGIN 6

paintGallery_t* paintGallery;

static int16_t arrowCharToRot(char dir);

void paintGallerySetup(display_t* disp, bool screensaver)
{
    paintGallery = calloc(sizeof(paintGallery_t), 1);
    paintGallery->disp = disp;
    paintGallery->canvas.disp = disp;
    paintGallery->galleryTime = 0;
    paintGallery->galleryLoadNew = true;
    paintGallery->screensaverMode = screensaver;
    // Show the UI at the start if we're a screensaver
    paintGallery->showUi = !screensaver;
    loadFont("radiostars.font", &paintGallery->infoFont);
    loadWsg("arrow12.wsg", &paintGallery->arrow);

    // Recolor the arrow to black
    colorReplaceWsg(&paintGallery->arrow, c555, c000);

    paintLoadIndex(&paintGallery->index);

    if (paintGetAnySlotInUse(paintGallery->index) && paintGetRecentSlot(paintGallery->index) != PAINT_SAVE_SLOTS)
    {
        paintGallery->gallerySlot = paintGetRecentSlot(paintGallery->index);
        PAINT_LOGD("Using the most recent gallery slot: %d", paintGallery->gallerySlot);
    }
    else
    {
        paintGallery->gallerySlot = paintGetNextSlotInUse(paintGallery->index, PAINT_SAVE_SLOTS - 1);
        PAINT_LOGD("Using the first slot: %d", paintGallery->gallerySlot);
    }

    if (!readNvs32(transitionTimeNvsKey, &paintGallery->gallerySpeedIndex))
    {
        // Default of 5s if not yet set
        paintGallery->gallerySpeedIndex = DEFAULT_TRANSITION_INDEX;
    }

    if (paintGallery->gallerySpeedIndex < 0 || paintGallery->gallerySpeedIndex >= sizeof(transitionTimeMap) / sizeof(*transitionTimeMap))
    {
        paintGallery->gallerySpeedIndex = DEFAULT_TRANSITION_INDEX;
    }

    paintGallery->gallerySpeed = US_PER_MS * transitionTimeMap[paintGallery->gallerySpeedIndex];

    paintGallery->portableDances = initPortableDance(danceIndexKey);

    if (!(paintGallery->index & PAINT_ENABLE_LEDS))
    {
        portableDanceSetByName(paintGallery->portableDances, "None");
    }


    // clear LEDs, which might still be set by menu
    led_t leds[NUM_LEDS];
    memset(leds, 0, sizeof(led_t) * NUM_LEDS);
    setLeds(leds, NUM_LEDS);
}

void paintGalleryCleanup(void)
{
    freeFont(&paintGallery->infoFont);
    freeWsg(&paintGallery->arrow);
    freePortableDance(paintGallery->portableDances);
    free(paintGallery);
}

void paintGalleryMainLoop(int64_t elapsedUs)
{
    paintGalleryModePollTouch();
    portableDanceMainLoop(paintGallery->portableDances, elapsedUs);

    if (paintGallery->infoTimeRemaining > 0)
    {
        paintGallery->infoTimeRemaining -= elapsedUs;

        if (paintGallery->infoTimeRemaining <= 0)
        {
            paintGallery->infoTimeRemaining = 0;
            paintGallery->galleryLoadNew = true;
            paintGallery->showUi = false;
        }
    }
    else
    {
        if (paintGallery->gallerySpeed != 0 && paintGallery->galleryTime >= paintGallery->gallerySpeed)
        {
            uint8_t prevSlot = paintGallery->gallerySlot;
            paintGallery->gallerySlot = paintGetNextSlotInUse(paintGallery->index, paintGallery->gallerySlot);
            // Only load the next image if it's actually a different image
            paintGallery->galleryLoadNew = (paintGallery->gallerySlot != prevSlot);
            paintGallery->galleryTime %= paintGallery->gallerySpeed;

            // reset info time if we're going to transition and clear the screen
            paintGallery->infoTimeRemaining = 0;
        }

        paintGallery->galleryTime += elapsedUs;
    }

    if (paintGallery->galleryLoadNew)
    {
        paintGallery->galleryLoadNew = false;
        if (!paintGalleryDoLoad())
        {
            return;
        }
    }

    if (paintGallery->showUi)
    {
        paintGallery->showUi = false;
        paintGalleryDrawUi();
    }
}

static int16_t arrowCharToRot(char dir)
{
    switch (dir)
    {
        case 'L':
        case 'l':
            return 270;

        case 'R':
        case 'r':
            return 90;

        case 'D':
        case 'd':
            return 180;

        case 'U':
        case 'u':
        default:
            return 0;
    }
}

void paintGalleryDrawUi(void)
{
    char text[32];

    snprintf(text, sizeof(text), "A: Next Slide      B: Exit");
    paintGalleryAddInfoText(text, 0, false, 0, 0);

    snprintf(text, sizeof(text), "Select: Scale: %dx", paintGallery->galleryScale);
    paintGalleryAddInfoText(text, 1, false, 0, 0);

    // Draw the controls
    snprintf(text, sizeof(text), "X~Y: LED Brightness: %d", getLedBrightness());
    paintGalleryAddInfoText(text, 2, false, 0, 0);


    // Draw speed 2 rows from the bottom
    if (paintGallery->gallerySpeed == 0)
    {
        paintGalleryAddInfoText(transitionOff, -3, true, 'U', 0);
    }
    else
    {
        snprintf(text, sizeof(text), transitionTime, (1.0 * paintGallery->gallerySpeed / US_PER_SEC));
        paintGalleryAddInfoText(text, -3, true, (paintGallery->gallerySpeedIndex + 1 < sizeof(transitionTimeMap) / sizeof(*transitionTimeMap)) ? 'U' : 0,'D');
    }

    // Draw the LED dance at the bottom
    snprintf(text, sizeof(text), "LEDs: %s", portableDanceGetName(paintGallery->portableDances));
    paintGalleryAddInfoText(text, -1, true, 'L', 'R');
}

void paintGalleryAddInfoText(const char* text, int8_t row, bool center, char leftArrow, char rightArrow)
{
    uint16_t width = textWidth(&paintGallery->infoFont, text);
    uint16_t padding = 3;
    int16_t yOffset;
    int16_t xOffset = center ? ((paintGallery->disp->w - width) / 2) : GALLERY_INFO_X_MARGIN;

    if (row < 0)
    {
        yOffset = paintGallery->disp->h + ((row) * (paintGallery->infoFont.h + padding * 2)) - GALLERY_INFO_Y_MARGIN;
    }
    else
    {
        yOffset = GALLERY_INFO_Y_MARGIN + row * (paintGallery->infoFont.h + padding * 2);
    }

    fillDisplayArea(paintGallery->disp, 0, yOffset, paintGallery->disp->w, yOffset + padding * 2 + paintGallery->infoFont.h, c555);

    if (leftArrow != 0)
    {
        // assumes arrows are always square, flip between W and H if that's ever the case
        drawWsg(paintGallery->disp, &paintGallery->arrow,
                xOffset - GALLERY_ARROW_MARGIN - paintGallery->arrow.w,
                yOffset + padding + (paintGallery->infoFont.h - paintGallery->arrow.h) / 2,
                false, false, arrowCharToRot(leftArrow));
    }

    if (rightArrow != 0)
    {
        // assumes arrows are always square, flip between W and H if that's ever the case
        drawWsg(paintGallery->disp, &paintGallery->arrow,
                xOffset + width + GALLERY_ARROW_MARGIN,
                yOffset + padding + (paintGallery->infoFont.h - paintGallery->arrow.h) / 2,
                false, false, arrowCharToRot(rightArrow));
    }

    drawText(paintGallery->disp, &paintGallery->infoFont, c000, text, xOffset, yOffset + padding);

    // start the timer to clear the screen
    paintGallery->infoTimeRemaining = GALLERY_INFO_TIME;
}

void paintGalleryDecreaseSpeed(void)
{
    if (paintGallery->gallerySpeedIndex + 1 < sizeof(transitionTimeMap) / sizeof(*transitionTimeMap))
    {
        paintGallery->gallerySpeedIndex++;
        paintGallery->gallerySpeed = US_PER_MS * transitionTimeMap[paintGallery->gallerySpeedIndex];
        writeNvs32(transitionTimeNvsKey, paintGallery->gallerySpeedIndex);

        paintGallery->galleryTime = 0;
    }
}

void paintGalleryIncreaseSpeed(void)
{
    if (paintGallery->gallerySpeedIndex > 0)
    {
        paintGallery->gallerySpeedIndex--;
        paintGallery->gallerySpeed = US_PER_MS * transitionTimeMap[paintGallery->gallerySpeedIndex];
        writeNvs32(transitionTimeNvsKey, paintGallery->gallerySpeedIndex);

        paintGallery->galleryTime = 0;
    }
}

void paintGalleryModeButtonCb(buttonEvt_t* evt)
{
    uint8_t prevSlot = paintGallery->gallerySlot;

    if (evt->down)
    {
        if (paintGallery->screensaverMode)
        {
            // Return to main menu immediately
            paintReturnToMainMenu();
            return;
        }

        switch (evt->button)
        {
            case UP:
            {
                paintGallery->showUi = true;
                paintGalleryDecreaseSpeed();
                break;
            }

            case DOWN:
            {
                paintGallery->showUi = true;
                paintGalleryIncreaseSpeed();
                break;
            }

            case LEFT:
            {
                portableDancePrev(paintGallery->portableDances);
                paintGallery->showUi = true;
                break;
            }

            case RIGHT:
            {
                portableDanceNext(paintGallery->portableDances);
                paintGallery->showUi = true;
                break;
            }

            case START:
            case BTN_B:
            {
                // Exit
                paintReturnToMainMenu();
                return;
            }

            case SELECT:
            {
                // Increase size
                paintGallery->galleryScale++;
                paintGallery->galleryLoadNew = true;
                paintGallery->showUi = true;
                break;
            }

            case BTN_A:
            {
                paintGallery->gallerySlot = paintGetNextSlotInUse(paintGallery->index, paintGallery->gallerySlot);
                paintGallery->galleryLoadNew = (prevSlot != paintGallery->gallerySlot);
                if (paintGallery->galleryLoadNew)
                {
                    paintSetRecentSlot(&paintGallery->index, paintGallery->gallerySlot);
                }
                paintGallery->galleryTime = 0;
                break;
            }
        }
    }
}

void paintGalleryModeTouchCb(touch_event_t* evt)
{
    paintGalleryModePollTouch();
}

void paintGalleryModePollTouch(void)
{
    int32_t centroid, intensity;
    if (getTouchCentroid(&centroid, &intensity))
    {
        // Bar is touched, convert the centroid into 8 segments (0-7)
        // But also reverse it so up is bright and down is less bright
        uint8_t curTouchSegment = 7 - ((centroid * 7 + 512) / 1024);

        if (curTouchSegment != getLedBrightness())
        {
            setAndSaveLedBrightness(curTouchSegment);
            paintGallery->showUi = true;
        }
    }
}

bool paintGalleryDoLoad(void)
{
    if(paintLoadDimensions(&paintGallery->canvas, paintGallery->gallerySlot))
    {
        uint8_t maxScale = paintGetMaxScale(paintGallery->canvas.disp, paintGallery->canvas.w, paintGallery->canvas.h, 0, 0);

        if (paintGallery->galleryScale > maxScale)
        {
            paintGallery->galleryScale = 1;
        }
        else if (paintGallery->galleryScale == 0)
        {
            paintGallery->galleryScale = maxScale;
        }

        paintGallery->canvas.xScale = paintGallery->galleryScale;
        paintGallery->canvas.yScale = paintGallery->galleryScale;

        paintGallery->canvas.x = (paintGallery->disp->w - paintGallery->canvas.w * paintGallery->canvas.xScale) / 2;
        paintGallery->canvas.y = (paintGallery->disp->h - paintGallery->canvas.h * paintGallery->canvas.yScale) / 2;

        paintGallery->disp->clearPx();

        return paintLoad(&paintGallery->index, &paintGallery->canvas, paintGallery->gallerySlot);
    }
    else
    {
        PAINT_LOGE("Slot %d has 0 dimension! Stopping load and clearing slot", paintGallery->gallerySlot);
        paintClearSlot(&paintGallery->index, paintGallery->gallerySlot);
        paintReturnToMainMenu();
        return false;
    }
}
