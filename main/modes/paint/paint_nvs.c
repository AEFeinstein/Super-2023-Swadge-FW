#include "paint_nvs.h"

#include "paint_common.h"
#include "paint_draw.h"
#include "paint_ui.h"
#include "paint_util.h"


void paintLoadIndex(void)
{
    //       SFX?
    //     BGM | Lights?
    //        \|/
    // |xxxxxvvv  |Recent?|  |Inuse? |
    // 0000 0000  0000 0000  0000 0000

    if (!readNvs32("pnt_idx", &paintState->index))
    {
        PAINT_LOGW("No metadata! Setting defaults");
        paintState->index = PAINT_DEFAULTS;
        paintSaveIndex();
    }
}

void paintSaveIndex(void)
{
    if (writeNvs32("pnt_idx", paintState->index))
    {
        PAINT_LOGD("Saved index: %04x", paintState->index);
    }
    else
    {
        PAINT_LOGE("Failed to save index :(");
    }
}

void paintResetStorage(void)
{
    paintState->index = PAINT_DEFAULTS;
    paintSaveIndex();
}

bool paintGetSlotInUse(uint8_t slot)
{
    paintLoadIndex();
    return (paintState->index & (1 << slot)) != 0;
}

void paintSetSlotInUse(uint8_t slot)
{
    paintState->index |= (1 << slot);
    paintSaveIndex();
}

bool paintGetAnySlotInUse(void)
{
    return (paintState->index & ((1 << PAINT_SAVE_SLOTS) - 1)) != 0;
}

uint8_t paintGetRecentSlot(void)
{
    paintLoadIndex();
    return (paintState->index >> PAINT_SAVE_SLOTS) & 0b111;
}

void paintSetRecentSlot(uint8_t slot)
{
    // TODO if we change the number of slots this will totally not work anymore
    // I mean, we could just do & 0xFF and waste 5 bits
    paintState->index = (paintState->index & PAINT_MASK_NOT_RECENT) | ((slot & 0b111) << PAINT_SAVE_SLOTS);
    paintSaveIndex();
}

void paintSave(const paintCanvas_t* canvas, uint8_t slot)
{
    // palette in reverse for quick transformation
    uint8_t paletteIndex[256];

    // NVS blob key name
    char key[16];

    // pointer to converted image segment
    uint8_t* imgChunk = NULL;

    // Save the palette map, this lets us compact the image by 50%
    snprintf(key, 16, "paint_%02d_pal", slot);
    PAINT_LOGD("paletteColor_t size: %zu, max colors: %d", sizeof(paletteColor_t), PAINT_MAX_COLORS);
    PAINT_LOGD("Palette will take up %zu bytes", sizeof(canvas->palette));
    if (writeNvsBlob(key, canvas->palette, sizeof(canvas->palette)))
    {
        PAINT_LOGD("Saved palette to slot %s", key);
    }
    else
    {
        PAINT_LOGE("Could't save palette to slot %s", key);
        return;
    }

    // Save the canvas dimensions
    uint32_t packedSize = canvas->w << 16 | canvas->h;
    snprintf(key, 16, "paint_%02d_dim", slot);
    if (writeNvs32(key, packedSize))
    {
        PAINT_LOGD("Saved dimensions to slot %s", key);
    }
    else
    {
        PAINT_LOGE("Couldn't save dimensions to slot %s",key);
        return;
    }

    // Build the reverse-palette map
    for (uint16_t i = 0; i < PAINT_MAX_COLORS; i++)
    {
        paletteIndex[((uint8_t)canvas->palette[i])] = i;
    }

    // Allocate space for the chunk
    uint32_t totalPx = canvas->h * canvas->w;
    uint32_t finalChunkSize = totalPx % PAINT_SAVE_CHUNK_SIZE / 2;

    // don't skip the entire last chunk if it falls on a boundary
    if (finalChunkSize == 0)
    {
        finalChunkSize = PAINT_SAVE_CHUNK_SIZE;
    }

    // Add double the chunk size (pixels per chunk, really), minus one, so that if there are e.g. 2049 pixels,
    // it would become 4096 and round up to 2 chunks instead of 1
    uint8_t chunkCount = (totalPx + (PAINT_SAVE_CHUNK_SIZE * 2) - 1) / (PAINT_SAVE_CHUNK_SIZE * 2);
    if ((imgChunk = malloc(sizeof(uint8_t) * PAINT_SAVE_CHUNK_SIZE)) != NULL)
    {
        PAINT_LOGD("Allocated %d bytes for image chunk", PAINT_SAVE_CHUNK_SIZE);
    }
    else
    {
        PAINT_LOGE("malloc failed for %d bytes", PAINT_SAVE_CHUNK_SIZE);
        return;
    }

    PAINT_LOGD("We will use %d chunks of size %dB (%d), plus one of %dB == %dB to save the image", chunkCount - 1, PAINT_SAVE_CHUNK_SIZE, (chunkCount - 1) * PAINT_SAVE_CHUNK_SIZE, finalChunkSize, (chunkCount - 1) * PAINT_SAVE_CHUNK_SIZE + finalChunkSize);
    PAINT_LOGD("The image is %d x %d px == %dpx, at 2px/B that's %dB", canvas->w, canvas->h, totalPx, totalPx / 2);

    disableCursor();

    uint16_t x0, y0, x1, y1;
    // Write all the chunks
    for (uint32_t i = 0; i < chunkCount; i++)
    {
        // build the chunk
        for (uint16_t n = 0; n < PAINT_SAVE_CHUNK_SIZE; n++)
        {
            // exit the last chunk early if needed
            if (i + 1 == chunkCount && n == finalChunkSize)
            {
                break;
            }

            // calculate the real coordinates given the pixel indices
            // (we store 2 pixels in each byte)
            // that's 100% more pixel, per pixel!
            x0 = PAINT_CANVAS_X_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2)) % canvas->w * PAINT_CANVAS_SCALE;
            y0 = PAINT_CANVAS_Y_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2)) / canvas->w * PAINT_CANVAS_SCALE;
            x1 = PAINT_CANVAS_X_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2 + 1)) % canvas->w * PAINT_CANVAS_SCALE;
            y1 = PAINT_CANVAS_Y_OFFSET + ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2 + 1)) / canvas->w * PAINT_CANVAS_SCALE;

            // we only need to save the top-left pixel of each scaled pixel, since they're the same unless something is very broken
            imgChunk[n] = paletteIndex[(uint8_t)canvas->disp->getPx(x0, y0)] << 4 | paletteIndex[(uint8_t)canvas->disp->getPx(x1, y1)];
        }

        // save the chunk
        snprintf(key, 16, "paint_%02dc%05d", slot, i);
        if (writeNvsBlob(key, imgChunk, (i + 1 < chunkCount) ? PAINT_SAVE_CHUNK_SIZE : finalChunkSize))
        {
            PAINT_LOGD("Saved blob %d of %d", i+1, chunkCount);
        }
        else
        {
            PAINT_LOGE("Unable to save blob %d of %d", i+1, chunkCount);
            free(imgChunk);

            enableCursor();
            return;
        }
    }

    paintSetSlotInUse(slot);
    paintSetRecentSlot(slot);
    paintSaveIndex();

    free(imgChunk);
    imgChunk = NULL;

    enableCursor();
}

void paintLoad(paintCanvas_t* canvas, uint8_t slot)
{
    // NVS blob key name
    char key[16];

    // pointer to converted image segment
    uint8_t* imgChunk = NULL;

    // read the palette and load it into the recentColors
    // read the dimensions and do the math
    // read the pixels

    size_t paletteSize;

    if (!paintGetSlotInUse(slot))
    {
        PAINT_LOGW("Attempted to load from uninitialized slot %d", slot);
        return;
    }

    // Load the palette map
    snprintf(key, 16, "paint_%02d_pal", slot);

    if (!readNvsBlob(key, NULL, &paletteSize))
    {
        PAINT_LOGE("Couldn't read size of palette in slot %s", key);
        return;
    }

    // TODO Move this outside of this function
    if (readNvsBlob(key, canvas->palette, &paletteSize))
    {
        paintState->fgColor = canvas->palette[0];
        paintState->bgColor = canvas->palette[1];
        PAINT_LOGD("Read %zu bytes of palette from slot %s", paletteSize, key);
    }
    else
    {
        PAINT_LOGE("Could't read palette from slot %s", key);
        return;
    }

    // Read the canvas dimensions
    PAINT_LOGD("Reading dimensions");
    int32_t packedSize;
    snprintf(key, 16, "paint_%02d_dim", slot);
    if (readNvs32(key, &packedSize))
    {
        canvas->h = (uint32_t)packedSize & 0xFFFF;
        canvas->w = (((uint32_t)packedSize) >> 16) & 0xFFFF;
        PAINT_LOGD("Read dimensions from slot %s: %d x %d", key, canvas->w, canvas->h);
    }
    else
    {
        PAINT_LOGE("Couldn't read dimensions from slot %s",key);
        return;
    }

    // Allocate space for the chunk
    uint32_t totalPx = canvas->h * canvas->w;
    uint8_t chunkCount = (totalPx + (PAINT_SAVE_CHUNK_SIZE * 2) - 1) / (PAINT_SAVE_CHUNK_SIZE * 2);
    if ((imgChunk = malloc(PAINT_SAVE_CHUNK_SIZE)) != NULL)
    {
        PAINT_LOGD("Allocated %d bytes for image chunk", PAINT_SAVE_CHUNK_SIZE);
    }
    else
    {
        PAINT_LOGE("malloc failed for %d bytes", PAINT_SAVE_CHUNK_SIZE);
        return;
    }

    disableCursor();
    paintClearCanvas();

    size_t lastChunkSize;
    uint16_t x0, y0, x1, y1;
    // Read all the chunks
    for (uint32_t i = 0; i < chunkCount; i++)
    {
        snprintf(key, 16, "paint_%02dc%05d", slot, i);

        // get the chunk size
        if (!readNvsBlob(key, NULL, &lastChunkSize))
        {
            PAINT_LOGE("Unable to read size of blob %d in slot %s", i, key);
            continue;
        }

        // read the chunk
        if (readNvsBlob(key, imgChunk, &lastChunkSize))
        {
            PAINT_LOGD("Read blob %d of %d (%zu bytes)", i+1, chunkCount, lastChunkSize);
        }
        else
        {
            PAINT_LOGE("Unable to read blob %d of %d", i+1, chunkCount);
            // don't panic if we miss one chunk, maybe it's ok...
            continue;
        }


        // build the chunk
        for (uint16_t n = 0; n < lastChunkSize; n++)
        {
            // no need for logic to exit the final chunk early, since each chunk's size is given to us
            // calculate the canvas coordinates given the pixel indices
            x0 = ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2)) % canvas->w;
            y0 = ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2)) / canvas->w;
            x1 = ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2 + 1)) % canvas->w;
            y1 = ((i * PAINT_SAVE_CHUNK_SIZE * 2) + (n * 2 + 1)) / canvas->w;

            setPxScaled(canvas->disp, x0, y0, canvas->palette[imgChunk[n] >> 4], canvas->x, canvas->y, canvas->xScale, canvas->yScale);
            setPxScaled(canvas->disp, x1, y1, canvas->palette[imgChunk[n] & 0xF], canvas->x, canvas->y, canvas->xScale, canvas->yScale);
        }
    }

    // This should't be necessary in the final version but whenever I change the canvas dimensions it breaks stuff
    // TODO: Actually remove this?
    if (canvas->h > PAINT_CANVAS_HEIGHT || canvas->w > PAINT_CANVAS_WIDTH)
    {
        canvas->h = PAINT_CANVAS_HEIGHT;
        canvas->w = PAINT_CANVAS_WIDTH;

        PAINT_LOGW("Loaded image had invalid bounds. Resetting to %d x %d", canvas->w, canvas->h);
    }

    paintSetRecentSlot(slot);

    free(imgChunk);
    imgChunk = NULL;

    enableCursor();
}
