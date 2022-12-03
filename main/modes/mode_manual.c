#include "mode_manual.h"

#include <stdlib.h>
#include <esp_log.h>

#include "swadgeMode.h"
#include "mode_main_menu.h"
#include "spiffs_txt.h"
#include "rich_text.h"
#include "bresenham.h"
#include "linked_list.h"

#include "markdown_parser.h"

#define MANUAL_FONT_UI "ibm_vga8.font"
#define MANUAL_FONT_HEADER "mm.font"
#define MANUAL_BOTTOM_MARGIN 24

void manualEnterMode(display_t* disp);
void manualExitMode(void);
void manualMainLoop(int64_t elapsedUs);
void manualButtonCb(buttonEvt_t* evt);
void manualBgDrawCb(display_t* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum);

typedef struct
{
    char* file;
    char* title;
} chapter_t;

typedef struct
{
    chapter_t* chapter;
    size_t offset;
} page_t;

const chapter_t chapters[] =
{
    {.file = "manual_intro.txt", .title = "Introduction"},
    {.file = "manual_stresstest.txt", .title = "Test page please ignore"},
    {.file = "alice.txt", .title = "Alice"},
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

    richText_t richText;

    chapter_t* chapter;

    char* text;
    list_t pages;
    node_t* curPage;
} manual_t;

manual_t* manual;

void manualLoadText(void)
{
    ESP_LOGD("Manual", "Loading text");
    if (manual->text != NULL)
    {
        free(manual->text);
    }

    manual->text = loadTxt(manual->chapter->file);
    if (manual->text == NULL)
    {
        manual->text = "# Could not load text :(";
    }

    if (manual->curPage == NULL || ((page_t*)manual->curPage->val)->chapter != manual->chapter)
    {
        page_t* newPage = malloc(sizeof(page_t));
        newPage->chapter = manual->chapter;
        newPage->offset = 0;

        push(&manual->pages, newPage);
        manual->curPage = manual->pages.last;
    }

    markdownText_t* result = parseMarkdown(manual->text);
    drawMarkdown(result, NULL);
    freeMarkdown(result);
}

void manualEnterMode(display_t* disp)
{
    manual = calloc(1, sizeof(manual_t));

    manual->disp = disp;

    manual->fgColor = c555;
    manual->bgColor = c000;

    loadFont(MANUAL_FONT_UI, &manual->uiFont);
    manual->bodyFont = manual->uiFont;
    loadFont(MANUAL_FONT_HEADER, &manual->headerFont);

    loadWsg("arrow12.wsg", &manual->arrowWsg);

    manual->chapter = chapters;
    manualLoadText();

    richTextInit(&manual->richText, manual->disp, &manual->bodyFont, manual->fgColor, ALIGN_LEFT, BREAK_WORD, STYLE_NORMAL);
    richTextSetBounds(&manual->richText, 13, 13, manual->disp->w - 13, manual->disp->h - MANUAL_BOTTOM_MARGIN);
    manual->richText.headerFont = &manual->headerFont;
}

void manualExitMode(void)
{
    freeFont(&manual->bodyFont);
    freeFont(&manual->headerFont);

    freeWsg(&manual->arrowWsg);

    richTextFree(&manual->richText);

    page_t* page;
    while (NULL != (page = pop(&manual->pages)))
    {
        free(page);
    }
    // maybe redundant but oh well
    clear(&manual->pages);

    if (manual->text != NULL)
    {
        free(manual->text);
    }
    free(manual);
}

void manualMainLoop(int64_t elapsedUs)
{
    fillDisplayArea(manual->disp, 0, 0, manual->disp->w, manual->disp->h, manual->bgColor);

    page_t* curPage = manual->curPage->val;
    richTextReset(&manual->richText);

    if (curPage->offset > 0)
    {
        richTextSkip(&manual->richText, manual->text, manual->text + curPage->offset);
    }

    const char* remainingText = richTextDraw(&manual->richText, manual->text + curPage->offset);

    plotLine(manual->disp, 0, manual->disp->h - MANUAL_BOTTOM_MARGIN + 1, manual->disp->w, manual->disp->h - MANUAL_BOTTOM_MARGIN + 1, manual->fgColor, 0);

    //  || manual->curPage->next != NULL || curPage->chapter < lastChapter
    if (remainingText != NULL)
    {
        // Store the page for later if we haven't already done that
        if (manual->curPage->next == NULL)
        {
            page_t* newPage = malloc(sizeof(page_t));
            newPage->chapter = curPage->chapter;
            newPage->offset = remainingText - manual->text;
            push(&manual->pages, newPage);
        }

        // Draw a "next page" arrow
        drawWsg(manual->disp, &manual->arrowWsg, manual->disp->w - 40 - manual->arrowWsg.w, manual->disp->h - (MANUAL_BOTTOM_MARGIN - manual->arrowWsg.h) / 2 - manual->arrowWsg.h, false, false, 90);
    }

    if (manual->curPage->prev != NULL)
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
        manualLoadText();
    }
}

void manualPrevChapter(void)
{
    if (manual->chapter > chapters)
    {
        manual->chapter--;
        manual->curPage = manual->curPage->prev;
        manualLoadText();
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
                manualPrevPage();
            break;

            case RIGHT:
                manualNextPage();
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