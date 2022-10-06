#include "px_stack.h"

#include <malloc.h>

#include "paint_common.h"
#include "paint_util.h"

void initPxStack(pxStack_t* pxStack)
{
    pxStack->size = PIXEL_STACK_MIN_SIZE;
    PAINT_LOGD("Allocating pixel stack with size %zu", pxStack->size);
    pxStack->data = malloc(sizeof(pxVal_t) * pxStack->size);
    pxStack->index = -1;
}

void freePxStack(pxStack_t* pxStack)
{
    if (pxStack->data != NULL)
    {
        free(pxStack->data);
        PAINT_LOGD("Freed pixel stack");
        pxStack->size = 0;
        pxStack->index = -1;
    }
}

void maybeGrowPxStack(pxStack_t* pxStack)
{
    if (pxStack->index >= pxStack->size)
    {
        pxStack->size *= 2;
        PAINT_LOGD("Expanding pixel stack to size %zu", pxStack->size);
        pxStack->data = realloc(pxStack->data, sizeof(pxVal_t) * pxStack->size);
    }
}

void maybeShrinkPxStack(pxStack_t* pxStack)
{
    // If the stack is at least 4 times bigger than it needs to be, shrink it by half
    // (but only if the stack is bigger than the minimum)
    if (pxStack->index >= 0 && pxStack->index * 4 <= pxStack->size && pxStack->size > PIXEL_STACK_MIN_SIZE)
    {
        pxStack->size /= 2;
        PAINT_LOGD("Shrinking pixel stack to %zu", pxStack->size);
        pxStack->data = realloc(pxStack->data, sizeof(pxVal_t) * pxStack->size);
        PAINT_LOGD("Done shrinking pixel stack");
    }
}

/**
 * The color at the given pixel coordinates is pushed onto the pixel stack,
 * along with its coordinates. If the pixel stack is uninitialized, it will
 * be allocated. If the pixel stack is full, its size will be doubled.
 *
 * @brief Pushes a pixel onto the pixel stack so that it can be restored later
 * @param x The screen X coordinate of the pixel to save
 * @param y The screen Y coordinate of the pixel to save
 *
 */
void pushPx(pxStack_t* pxStack, display_t* disp, uint16_t x, uint16_t y)
{
    pxStack->index++;

    maybeGrowPxStack(pxStack);

    pxStack->data[pxStack->index].x = x;
    pxStack->data[pxStack->index].y = y;
    pxStack->data[pxStack->index].col = disp->getPx(x, y);
}

/**
 * Removes the pixel from the top of the stack and draws its color at its coordinates.
 * If the stack is already empty, no pixels will be drawn. If the pixel stack's size is
 * at least 4 times its number of entries, its size will be halved, at most to the minimum size.
 * Returns `true` if a value was popped, and `false` if the stack was empty and no value was popped.
 *
 * @brief Pops a pixel from the stack and restores it to the screen
 * @return `true` if a pixel was popped, and `false` if the stack was empty.
 */
bool popPx(pxStack_t* pxStack, display_t* disp)
{
    // Make sure the stack isn't empty
    if (pxStack->index >= 0)
    {
        // Draw the pixel from the top of the stack
        disp->setPx(pxStack->data[pxStack->index].x, pxStack->data[pxStack->index].y, pxStack->data[pxStack->index].col);
        pxStack->index--;

        // Is this really necessary? The stack empties often so maybe it's better not to reallocate constantly
        //maybeShrinkPxStack(pxStack);

        return true;
    }

    return false;
}

bool peekPx(const pxStack_t* pxStack, pxVal_t* dest)
{
    if (pxStack->index >= 0)
    {
        *dest = pxStack->data[pxStack->index];
        return true;
    }

    return false;
}

bool getPx(const pxStack_t* pxStack, size_t pos, pxVal_t* dest)
{
    if (pos <= pxStack->index)
    {
        *dest = pxStack->data[pos];
        return true;
    }

    return false;
}

bool dropPx(pxStack_t* pxStack)
{
    if (pxStack->index >= 0)
    {
        pxStack->index--;
        return true;
    }

    return false;
}

size_t pxStackSize(const pxStack_t* pxStack)
{
    return pxStack->index + 1;
}

void pushPxScaled(pxStack_t* pxStack, display_t* disp, int x, int y, int xTr, int yTr, int xScale, int yScale)
{
    pushPx(pxStack, disp, xTr + x * xScale, yTr + y * yScale);
}

bool popPxScaled(pxStack_t* pxStack, display_t* disp, int xScale, int yScale)
{
    pxVal_t px;
    if (peekPx(pxStack, &px))
    {
        plotRectFilled(disp, px.x, px.y, px.x + xScale, px.y + yScale, px.col);
        dropPx(pxStack);

        return true;
    }

    return false;
}
