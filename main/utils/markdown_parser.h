#ifndef _MARKDOWN_PARSER_H_
#define _MARKDOWN_PARSER_H_

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "palette.h"

typedef enum
{
    STYLE_NORMAL = 0,
    STYLE_ITALIC = 1,
    STYLE_BOLD = 2,
    STYLE_UNDERLINE = 4,
    STYLE_STRIKE = 8,
} textStyle_t;

typedef enum
{
    ALIGN_LEFT = 1,
    ALIGN_RIGHT = 2,
    ALIGN_CENTER = 3,

    VALIGN_TOP = 4,
    VALIGN_BOTTOM = 8,
    VALIGN_CENTER = 12,
} textAlign_t;

/// @brief Defines the strategy to be used when the text to be printed does not fit on a single line
typedef enum
{
    /// @brief Text will be broken onto the next line at word boundaries (' ' or '-') if possible
    BREAK_WORD,
    /// @brief Text will be broken onto the next line immediately after the last character that fits
    BREAK_CHAR,
    /// @brief Text will be printed up to the last character that fits, and the rest will be ignored.
    BREAK_TRUNCATE,
    /// @brief Text will be printed up to the last word that fits, and the rest will be ignored
    BREAK_TRUNCATE_WORD,
    /// @brief Text will not be drawn unless it fits completely on a single line
    BREAK_NONE,
} textBreak_t;

typedef struct _markdownText_t *markdownText_t;
typedef struct _markdownContinue_t *markdownContinue_t;

typedef struct
{
    int16_t xMin, yMin, xMax, yMax;
    textStyle_t style;
    textAlign_t align;
    textBreak_t breakMode;
    paletteColor_t color;
    font_t* bodyFont;
    font_t* headerFont;
} markdownParams_t;

markdownText_t* parseMarkdown(const char* text);
void freeMarkdown(markdownText_t* markdown);
bool drawMarkdown(display_t* disp, const markdownText_t* markdown, const markdownParams_t* params, markdownContinue_t** pos, bool savePos);
markdownContinue_t* copyContinue(const markdownContinue_t* pos);

#endif
