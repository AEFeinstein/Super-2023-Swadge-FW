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
#define GALLERY_INFO_Y_MARGIN 6

paintGallery_t* paintGallery;

void paintGallerySetup(display_t* disp, bool screensaver)
{
    paintGallery = calloc(sizeof(paintGallery_t), 1);
    paintGallery->disp = disp;
    paintGallery->canvas.disp = disp;
    paintGallery->galleryTime = 0;
    paintGallery->galleryLoadNew = true;
    paintGallery->screensaverMode = screensaver;
    loadFont("radiostars.font", &paintGallery->infoFont);

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
}

void paintGalleryAddInfoText(const char* text, int16_t yOffset)
{
    uint16_t width = textWidth(&paintGallery->infoFont, text);
    uint16_t padding = 3;

    if (yOffset < 0)
    {
        yOffset = paintGallery->disp->h - paintGallery->infoFont.h - padding * 2 + yOffset;
    }

    fillDisplayArea(paintGallery->disp, 0, yOffset, paintGallery->disp->w, yOffset + padding * 2 + paintGallery->infoFont.h, c555);
    drawText(paintGallery->disp, &paintGallery->infoFont, c000, text, (paintGallery->disp->w - width) / 2, yOffset + padding);

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
    bool updateTimeText = false, updateDanceText = false;
    char text[32];
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
                updateTimeText = true;
                paintGalleryDecreaseSpeed();
                break;
            }

            case DOWN:
            {
                updateTimeText = true;
                paintGalleryIncreaseSpeed();
                break;
            }

            case LEFT:
            {
                portableDancePrev(paintGallery->portableDances);
                updateDanceText = true;
                break;
            }

            case RIGHT:
            {
                portableDanceNext(paintGallery->portableDances);
                updateDanceText = true;
                break;
            }

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

            case START:
            // Do nothing
            break;
        }
    }

    if (updateTimeText)
    {
        if (paintGallery->gallerySpeed == 0)
        {
            paintGalleryAddInfoText(transitionOff, GALLERY_INFO_Y_MARGIN);
        }
        else
        {
            snprintf(text, sizeof(text), transitionTime, (1.0 * paintGallery->gallerySpeed / US_PER_SEC));
            paintGalleryAddInfoText(text, GALLERY_INFO_Y_MARGIN);
        }
    }

    if (updateDanceText)
    {
        snprintf(text, sizeof(text), "LEDs: %s", portableDanceGetName(paintGallery->portableDances));
        paintGalleryAddInfoText(text, -GALLERY_INFO_Y_MARGIN);
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
            PAINT_LOGD("Changing LED brightness from %d to %d", getLedBrightness(), curTouchSegment);
            setAndSaveLedBrightness(curTouchSegment);
        }
    }
}

bool paintGalleryDoLoad(void)
{
    if(paintLoadDimensions(&paintGallery->canvas, paintGallery->gallerySlot))
    {
        uint8_t maxScale = paintGetMaxScale(paintGallery->canvas.disp, paintGallery->canvas.w, paintGallery->canvas.h, 0, 0);

        if (paintGallery->galleryScale >= maxScale)
        {
            paintGallery->galleryScale = 0;
        }

        if (paintGallery->galleryScale == 0)
        {
            paintGallery->canvas.xScale = maxScale;
            paintGallery->canvas.yScale = maxScale;
        }
        else
        {
            paintGallery->canvas.xScale = paintGallery->galleryScale;
            paintGallery->canvas.yScale = paintGallery->galleryScale;
        }

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
