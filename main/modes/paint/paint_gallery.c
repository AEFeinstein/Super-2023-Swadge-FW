#include "paint_gallery.h"

#include "mode_paint.h"
#include "paint_common.h"
#include "paint_nvs.h"
#include "paint_util.h"

// 1s
#define GALLERY_MIN_TIME 5000000
// 60s
#define GALLERY_MAX_TIME 60000000
// 5s
#define GALLERY_TIME_STEP 5000000

paintGallery_t* paintGallery;

void paintGallerySetup(display_t* disp)
{
    paintGallery = calloc(sizeof(paintGallery_t), 1);
    paintGallery->disp = disp;
    paintGallery->canvas.disp = disp;
    paintGallery->gallerySpeed = GALLERY_MIN_TIME;
    paintGallery->galleryTime = 0;
    paintGallery->galleryLoadNew = true;

    paintLoadIndex(&paintGallery->index);
}

void paintGalleryCleanup(void)
{
    free(paintGallery);
}

void paintGalleryMainLoop(int64_t elapsedUs)
{
    if (paintGallery->gallerySpeed != 0 && paintGallery->galleryTime >= paintGallery->gallerySpeed)
    {
        paintGallery->gallerySlot = paintGetNextSlotInUse(paintGallery->index, paintGallery->gallerySlot);
        paintGallery->galleryLoadNew = true;
        paintGallery->galleryTime -= paintGallery->gallerySpeed;
    }

    if (paintGallery->galleryLoadNew)
    {
        paintGallery->galleryLoadNew = false;
        paintGalleryDoLoad();
    }

    paintGallery->galleryTime += elapsedUs;
}

void paintGalleryModeButtonCb(buttonEvt_t* evt)
{
    if (evt->down)
    {
        switch (evt->button)
        {
            case UP:
            {
                if (paintGallery->gallerySpeed <= GALLERY_MIN_TIME)
                {
                    paintGallery->gallerySpeed = 0;
                }
                else
                {
                    paintGallery->gallerySpeed -= GALLERY_TIME_STEP;
                    if (paintGallery->gallerySpeed < GALLERY_MIN_TIME)
                    {
                        paintGallery->gallerySpeed = GALLERY_MIN_TIME;
                    }
                }
                paintGallery->galleryTime = 0;
                break;
            }

            case DOWN:
            {
                if (paintGallery->gallerySpeed != GALLERY_MAX_TIME)
                {
                    paintGallery->gallerySpeed += GALLERY_TIME_STEP;
                    if (paintGallery->gallerySpeed > GALLERY_MAX_TIME)
                    {
                        paintGallery->gallerySpeed = GALLERY_MAX_TIME;
                    }
                }
                paintGallery->galleryTime = 0;
                break;
            }

            case LEFT:
            paintGallery->gallerySlot = paintGetPrevSlotInUse(paintGallery->index, paintGallery->gallerySlot);
            paintGallery->galleryLoadNew = true;
            paintGallery->galleryTime = 0;
            break;

            case RIGHT:
            paintGallery->gallerySlot = paintGetNextSlotInUse(paintGallery->index, paintGallery->gallerySlot);
            paintGallery->galleryLoadNew = true;
            paintGallery->galleryTime = 0;
            break;

            case BTN_B:
            // Exit
            paintReturnToMainMenu();
            break;

            case BTN_A:
            // Increase size
            paintGallery->galleryScale++;
            paintGallery->galleryLoadNew = true;
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
    paintLoadDimensions(&paintGallery->canvas, paintGallery->gallerySlot);

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

    paintLoad(&paintGallery->index, &paintGallery->canvas, paintGallery->gallerySlot);
}