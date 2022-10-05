#include "paint_gallery.h"

#include "paint_common.h"
#include "paint_nvs.h"
#include "paint_util.h"

// 1s
#define GALLERY_MIN_TIME 5000000
// 60s
#define GALLERY_MAX_TIME 60000000
// 5s
#define GALLERY_TIME_STEP 5000000

void paintGallerySetup(void)
{
    paintState->gallerySpeed = GALLERY_MIN_TIME;
    paintState->galleryTime = 0;
    paintState->galleryLoadNew = true;
    paintState->screen = PAINT_GALLERY;
}

void paintGalleryMainLoop(int64_t elapsedUs)
{
    if (paintState->gallerySpeed != 0 && paintState->galleryTime >= paintState->gallerySpeed)
    {
        paintState->gallerySlot = paintGetNextSlotInUse(paintState->gallerySlot);
        paintState->galleryLoadNew = true;
        paintState->galleryTime -= paintState->gallerySpeed;
    }

    if (paintState->galleryLoadNew)
    {
        paintState->galleryLoadNew = false;
        paintGalleryDoLoad();
    }

    paintState->galleryTime += elapsedUs;
}

void paintGalleryModeButtonCb(buttonEvt_t* evt)
{
    if (evt->down)
    {
        switch (evt->button)
        {
            case UP:
            {
                if (paintState->gallerySpeed <= GALLERY_MIN_TIME)
                {
                    paintState->gallerySpeed = 0;
                }
                else
                {
                    paintState->gallerySpeed -= GALLERY_TIME_STEP;
                    if (paintState->gallerySpeed < GALLERY_MIN_TIME)
                    {
                        paintState->gallerySpeed = GALLERY_MIN_TIME;
                    }
                }
                paintState->galleryTime = 0;
                break;
            }

            case DOWN:
            {
                if (paintState->gallerySpeed != GALLERY_MAX_TIME)
                {
                    paintState->gallerySpeed += GALLERY_TIME_STEP;
                    if (paintState->gallerySpeed > GALLERY_MAX_TIME)
                    {
                        paintState->gallerySpeed = GALLERY_MAX_TIME;
                    }
                }
                paintState->galleryTime = 0;
                break;
            }

            case LEFT:
            paintState->gallerySlot = paintGetPrevSlotInUse(paintState->gallerySlot);
            paintState->galleryLoadNew = true;
            paintState->galleryTime = 0;
            break;

            case RIGHT:
            paintState->gallerySlot = paintGetNextSlotInUse(paintState->gallerySlot);
            paintState->galleryLoadNew = true;
            paintState->galleryTime = 0;
            break;

            case BTN_B:
            // Exit
                paintState->screen = PAINT_MENU;
            break;

            case BTN_A:
            // Increase size
            paintState->galleryScale++;
            paintState->galleryLoadNew = true;
            break;

            case SELECT:
            case START:
            // Do nothing
            break;
        }
    }
    else
    {
        // button up? don't care
    }
}

void paintGalleryDoLoad(void)
{
    paintLoadDimensions(&paintState->canvas, paintState->gallerySlot);

    uint8_t maxScale = paintGetMaxScale(paintState->canvas.disp, paintState->canvas.w, paintState->canvas.h, 0, 0);

    if (paintState->galleryScale >= maxScale)
    {
        paintState->galleryScale = 0;
    }

    if (paintState->galleryScale == 0)
    {
        paintState->canvas.xScale = maxScale;
        paintState->canvas.yScale = maxScale;
    }
    else
    {
        paintState->canvas.xScale = paintState->galleryScale;
        paintState->canvas.yScale = paintState->galleryScale;
    }

    paintState->canvas.x = (paintState->disp->w - paintState->canvas.w * paintState->canvas.xScale) / 2;
    paintState->canvas.y = (paintState->disp->h - paintState->canvas.h * paintState->canvas.yScale) / 2;

    paintState->disp->clearPx();

    paintLoad(&paintState->canvas, paintState->gallerySlot);
}