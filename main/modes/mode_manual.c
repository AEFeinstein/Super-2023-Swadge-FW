#include "mode_manual.h"

#include <stdlib.h>
#include <esp_log.h>
#include <string.h>

#include "swadgeMode.h"
#include "mode_main_menu.h"
#include "spiffs_txt.h"
#include "bresenham.h"
#include "linked_list.h"

#include "markdown_parser.h"

#define MANUAL_FONT_UI "ibm_vga8.font"
#define MANUAL_FONT_HEADER "mm.font"
#define MANUAL_TOP_MARGIN 13
#define MANUAL_BOTTOM_MARGIN 24
#define MANUAL_SIDE_MARGIN 13

void manualEnterMode(display_t* disp);
void manualExitMode(void);
void manualMainLoop(int64_t elapsedUs);
void manualButtonCb(buttonEvt_t* evt);
void manualBgDrawCb(display_t* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum);

void manualNextPage(void);
void manualPrevPage(void);

typedef enum
{
    NONE,
    NEXT_PAGE,
    PREV_PAGE,
    RELOAD_PAGE,
} nav_t;

typedef struct
{
    char* file;
    char* title;
} chapter_t;

typedef struct
{
    const chapter_t* chapter;
    markdownContinue_t* mdPos;
} page_t;

const chapter_t chapters[] =
{
    {.file = "manual_intro.txt", .title = "Introduction"},
    {.file = "manual_stresstest.txt", .title = "Test page please ignore"},
    {.file = "manual_lipsum.txt", .title = "Lorem Ipsum"},
    {.file = "halfApress.txt", .title = "Half A-Press"},
};

const chapter_t* lastChapter = chapters + sizeof(chapters) / sizeof(chapters[0]) - 1;

swadgeMode modeManual =
{
    .modeName = "Manual",
    .fnEnterMode = manualEnterMode,
    .fnExitMode = manualExitMode,
    .fnMainLoop = manualMainLoop,
    .fnButtonCallback = manualButtonCb,
    .fnBackgroundDrawCallback = NULL, //manualBgDrawCb,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL,
    .overrideUsb = false
};

typedef struct
{
    display_t* disp;
    font_t uiFont, bodyFont, headerFont;
    wsg_t arrowWsg;

    paletteColor_t bgColor, fgColor;

    markdownText_t* markdown;
    markdownContinue_t* mdPosition;
    markdownParams_t mdParams;

    const chapter_t* chapter;

    char* text;
    list_t pages;
    node_t* curPage;

    nav_t navigate;
    bool reload;
} manual_t;

manual_t* manual;

node_t* paginateText(markdownText_t* markdown, list_t* container)
{
    node_t* firstPage = NULL;
    page_t* newPage = NULL;
    markdownContinue_t* nextPos = NULL;

    do
    {
        newPage = malloc(sizeof(page_t));
        newPage->chapter = manual->chapter;

        // copyContinue will return a NULL if passed a NULL
        newPage->mdPos = copyContinue(nextPos);
        push(&manual->pages, newPage);

        if (firstPage == NULL)
        {
            firstPage = manual->pages.last;
        }
    }
    while (drawMarkdown(NULL, markdown, &manual->mdParams, &nextPos, true));

    if (nextPos != NULL)
    {
        free(nextPos);
    }

    return firstPage;
}

void manualLoadText(bool reverse)
{
    ESP_LOGD("Manual", "Loading text");
    if (manual->markdown != NULL)
    {
        freeMarkdown(manual->markdown);
        manual->markdown = NULL;
    }

    if (manual->text != NULL)
    {
        free(manual->text);
        manual->text = NULL;
    }

    manual->text = loadTxt(manual->chapter->file);
    if (manual->text == NULL)
    {
        manual->text = "# Could not load text :(";
    }

    manual->markdown = parseMarkdown(manual->text);

    // if the page we're trying to use hasn't been created yet, we need to create it
    if (manual->curPage == NULL)// || ((page_t*)manual->curPage->val)->chapter != manual->chapter)
    {
        manual->curPage = paginateText(manual->markdown, &manual->pages);

        if (reverse)
        {
            manual->curPage = manual->pages.last;
        }
    }
}

void manualEnterMode(display_t* disp)
{
    manual = calloc(1, sizeof(manual_t));

    manual->disp = disp;

    manual->fgColor = c555;
    manual->bgColor = c000;

    manual->navigate = RELOAD_PAGE;
    manual->reload = true;

    loadFont(MANUAL_FONT_UI, &manual->uiFont);
    manual->bodyFont = manual->uiFont;
    loadFont(MANUAL_FONT_HEADER, &manual->headerFont);

    loadWsg("arrow12.wsg", &manual->arrowWsg);

    markdownParams_t params =
    {
        .xMin = MANUAL_SIDE_MARGIN, .yMin = MANUAL_TOP_MARGIN,
        .xMax = manual->disp->w - MANUAL_SIDE_MARGIN, .yMax = manual->disp->h - MANUAL_BOTTOM_MARGIN,
        .bodyFont = &manual->bodyFont,
        .headerFont = &manual->headerFont,
        .color = manual->fgColor,
    };

    memcpy(&manual->mdParams, &params, sizeof(markdownParams_t));

    manual->chapter = chapters;
}

void manualExitMode(void)
{
    freeFont(&manual->bodyFont);
    freeFont(&manual->headerFont);

    freeWsg(&manual->arrowWsg);

    page_t* page;
    while (NULL != (page = pop(&manual->pages)))
    {
        free(page->mdPos);
        free(page);
    }
    // maybe redundant but oh well
    clear(&manual->pages);

    if (manual->markdown != NULL)
    {
        freeMarkdown(manual->markdown);
    }

    if (manual->text != NULL)
    {
        free(manual->text);
    }
    free(manual);
}

void manualMainLoop(int64_t elapsedUs)
{
#define hasNext (manual->curPage->next != NULL || manual->chapter < lastChapter)
#define hasPrev (manual->curPage->prev != NULL)

    if (manual->navigate != NONE)
    {
        if (manual->reload)
        {
            if (manual->navigate == NEXT_PAGE)
            {
                manualNextPage();
            }
            else if (manual->navigate == PREV_PAGE)
            {
                manualPrevPage();
            }
            else if (manual->navigate == RELOAD_PAGE)
            {
                manualLoadText(false);
            }

            // Clear the screen
            fillDisplayArea(manual->disp, 0, 0, manual->disp->w, manual->disp->h, manual->bgColor);

            page_t* curPage = manual->curPage->val;
            drawMarkdown(manual->disp, manual->markdown, &manual->mdParams, &curPage->mdPos, false);

            manual->navigate = NONE;
            manual->reload = false;
        }
        else
        {
            if (manual->navigate == RELOAD_PAGE
                || (manual->navigate == NEXT_PAGE && hasNext)
                || (manual->navigate == PREV_PAGE && hasPrev))
            {
                // Just draw the waiting icon and set us up to reload on the next frame
                manual->reload = true;

    #define HOURGLASS_W 20
    #define HOURGLASS_H 37

                plotCircleFilled(manual->disp, manual->disp->w / 2, manual->disp->h / 2, HOURGLASS_H / 2 + 6, manual->bgColor);
                plotCircle(manual->disp, manual->disp->w / 2, manual->disp->h / 2, HOURGLASS_H / 2 + 6, manual->fgColor);
                int16_t x0, x1;
                int16_t y0, y1;

                x0 = (manual->disp->w - HOURGLASS_W) / 2;
                x1 = x0 + HOURGLASS_W;
                y0 = (manual->disp->h - HOURGLASS_H) / 2;
                y1 = y0 + HOURGLASS_H;

                // Top-Left -> Bottom Right
                plotLine(manual->disp, x0, y0, x1, y1, manual->fgColor, 0);
                // Top-Left -> Top Right
                plotLine(manual->disp, x0, y0, x1, y0, manual->fgColor, 0);
                // Top-Right -> Bottom Left
                plotLine(manual->disp, x1, y0, x0, y1, manual->fgColor, 0);
                // Bottom-Left -> Bottom-Right
                plotLine(manual->disp, x0, y1, x1, y1, manual->fgColor, 0);
            }
            else
            {
                manual->navigate = NONE;
            }
        }
    }

    fillDisplayArea(manual->disp, 0, 0, manual->disp->w, MANUAL_TOP_MARGIN, manual->bgColor);
    fillDisplayArea(manual->disp, 0, 0, MANUAL_SIDE_MARGIN, manual->disp->h, manual->bgColor);
    fillDisplayArea(manual->disp, manual->disp->w - MANUAL_SIDE_MARGIN + 1, 0, manual->disp->w, manual->disp->h, manual->bgColor);
    fillDisplayArea(manual->disp, 0, manual->disp->h - MANUAL_BOTTOM_MARGIN + 1, manual->disp->w, manual->disp->h, manual->bgColor);

    plotLine(manual->disp, 0, manual->disp->h - MANUAL_BOTTOM_MARGIN + 1, manual->disp->w, manual->disp->h - MANUAL_BOTTOM_MARGIN + 1, manual->fgColor, 0);

    //  || manual->curPage->next != NULL || curPage->chapter < lastChapter
    if (hasNext)
    {
        // Draw a "next page" arrow
        drawWsg(manual->disp, &manual->arrowWsg, manual->disp->w - 40 - manual->arrowWsg.w, manual->disp->h - (MANUAL_BOTTOM_MARGIN - manual->arrowWsg.h) / 2 - manual->arrowWsg.h, false, false, 90);
    }

    if (hasPrev)
    {
        // Draw a "prev page" arrow
        drawWsg(manual->disp, &manual->arrowWsg, 40, manual->disp->h - (MANUAL_BOTTOM_MARGIN - manual->arrowWsg.h) / 2 - manual->arrowWsg.h, false, false, 270);
    }


}

void manualNextChapter(void)
{
    if (manual->chapter != lastChapter)
    {
        manual->chapter++;
        manual->curPage = manual->curPage->next;
        manualLoadText(false);
    }
}

void manualPrevChapter(void)
{
    if (manual->chapter > chapters)
    {
        manual->chapter--;
        manual->curPage = manual->curPage->prev;
        manualLoadText(true);
    }
}

void manualNextPage(void)
{
    // if there is a next page, and it is in the same chapter, go to it
    // if there is not a next page, or it is in a different chapter, go to next chapter
    if (manual->curPage->next != NULL && ((page_t*)manual->curPage->next->val)->chapter == manual->chapter)
    {
        manual->curPage = manual->curPage->next;
    }
    else
    {
        manualNextChapter();
    }
}

void manualPrevPage(void)
{
    if (manual->curPage->prev != NULL && ((page_t*)manual->curPage->prev->val)->chapter == manual->chapter)
    {
        manual->curPage = manual->curPage->prev;
    }
    else
    {
        manualPrevChapter();
    }
}

void manualButtonCb(buttonEvt_t* evt)
{
    if (evt->down)
    {
        switch (evt->button)
        {
            case UP:
            case DOWN:
            break;

            case LEFT:
                manual->navigate = PREV_PAGE;
            break;

            case RIGHT:
                manual->navigate = NEXT_PAGE;
            break;

            case BTN_A:
            break;

            case START:
            case BTN_B:
                switchToSwadgeMode(&modeMainMenu);
            break;

            case SELECT:
            break;
        }
    }
}

void manualBgDrawCb(display_t* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum)
{
    ESP_LOGD("Manual", "manualBgDrawCb(x=%d, y=%d, w=%d, h=%d, up=%d, upNum=%d)", x, y, w, h, up, upNum);
}