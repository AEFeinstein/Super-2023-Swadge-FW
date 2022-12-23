#include "markdown_parser.h"

#include <stddef.h>
#include <malloc.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <string.h>
#include <stdio.h>

#include "palette.h"
#include "display.h"
#include "bresenham.h"

#ifdef DEBUG
#define MDLOG(...) printf(__VA_ARGS__)
#else
#define MDLOG(...)
#endif

//ESP_LOGD("Markdown", __VA_ARGS__)
#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))

#define MD_MALLOC(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
//#define MD_MALLOC(size) malloc(size)

#define NO_BACKTRACK UINT32_MAX

#define DEBUG

typedef enum
{
    ADD_SIBLING = 1,
    ADD_CHILD = 2,
    GO_TO_PARENT = 4,
} nodeAction_t;

typedef struct
{
    const char* start;
    const char* end;
} mdText_t;

typedef enum
{
    STYLE,
    ALIGN,
    BREAK,
    COLOR,
    FONT,
} mdOptType_t;

typedef struct
{
    mdOptType_t type;
    union {
        textStyle_t style;
        textAlign_t align;
        textBreak_t breakMode;
        paletteColor_t color;
        mdText_t font;
    };
} mdOpt_t;

typedef enum
{
    HORIZONTAL_RULE,
    WORD_BREAK,
    LINE_BREAK,
    PARAGRAPH_BREAK,
    PAGE_BREAK,
    BULLET,
    HEADER,
} mdDec_t;

typedef enum
{
    TEXT,
    OPTION,
    DECORATION,
    IMAGE,
    CUSTOM,
    EMPTY,
} mdNodeType_t;

typedef struct mdNode
{
    struct mdNode* parent;
    struct mdNode* child;
    struct mdNode* next;
    uint32_t index;

    mdNodeType_t type;

    union {
        mdText_t text;
        mdOpt_t option;
        mdDec_t decoration;
        mdText_t image;
        void* custom;
    };
} mdNode_t;

typedef struct
{
    const char* text;
    mdNode_t* tree;
} _markdownText_t;

typedef struct _markdownContinue_t
{
    size_t textPos;
    uint32_t treeIndex;
} _markdownContinue_t;

typedef struct
{
    // The node to print
    // This may either be TEXT, DECORATION.WORD_BREAK, or IMAGE (eventually)
    const mdNode_t* node;

    // we can technically figure out the style from the tree, but we have plenty of SPIRAM and not a lot of speed
    textStyle_t textStyle;
    paletteColor_t color;
    font_t* font;
} mdLinePartInfo_t;

typedef struct
{
    // The total width of all parts
    int16_t totalWidth;

    // In case the first node doesn't start at the beginning of the line,
    // this is the starting offset for that text
    size_t firstTextOffset;

    // The alignment of the first node of the line
    // If this changes while we're printing the line, oh well!
    // Maybe in the future, we can support
    textAlign_t align;

    // The maximum height of any text within this line
    int16_t lineHeight;

    mdLinePartInfo_t* parts;
    size_t partCount;
    size_t partAlloc;

    bool pending;
} mdLinePlan_t;

typedef struct
{
    markdownParams_t defaults;
    markdownParams_t params;
    mdLinePlan_t linePlan;

    int16_t x, y;
    font_t* font;

    size_t textPos;
    uint32_t backtrackIndex;

    // for storing pointers to arbitrary data
    // in case we need to load a new font or something
    void* data[16];
    uint8_t next;
} mdPrintState_t;


static void parseMarkdownInner(const char* text, _markdownText_t* out);

/// @brief Returns a pointer to the next node, allocating more space if necessary, and sets up the pointers
/// If `parent` is not NULL, it will be set as the returned node's `parent` pointer.
/// And, if `parent->child` is NULL, `parent->child` will be set to the returned node.
/// If `prev` is not NULL, `prev->next` will be set to the returned node.
/// @param data The _markdownText_t containing the tree root and the children backing array
/// @param parent The node to set as the parent of the new node
/// @param prev The node's previous sibling, if this is not the first node
/// @return
static mdNode_t* newNode(_markdownText_t* data, mdNode_t* parent, mdNode_t* prev);
static void freeTree(mdNode_t*);
static bool drawMarkdownNode(display_t* disp, const mdNode_t* node, const mdNode_t* prev, mdPrintState_t* state);

static mdNode_t* newNode(_markdownText_t* data, mdNode_t* parent, mdNode_t* prev)
{
    if (parent == NULL && prev == NULL)
    {
        ESP_LOGE("Markdown", "Allocating root node");
    }

    mdNode_t* result = MD_MALLOC(sizeof(mdNode_t));
    result->type = EMPTY;
    result->child = NULL;
    result->next = NULL;
    result->parent = NULL;

    if (parent != NULL)
    {
        result->parent = parent;

        if (parent->child == NULL)
        {
            parent->child = result;
        }
    }

    if (prev != NULL)
    {
        prev->next = result;
    }

    return result;
}

static const char* strndebug(const char* start, const char* end) {
#ifdef DEBUG
    static char buffer[64];

    if (start == NULL || end == NULL || end < start)
    {
        sprintf(buffer, "start=0x%08x, end=0x%08x", (int)start, (int)end);
        return buffer;
    }

    // if the start is: "hi there\0"
    // and end is:          "here\0" (so the actual string should be "hi t", strlen=4)
    // then end - start is going to be 4, which is correct
    size_t len = (end - start);

    if (len >= sizeof(buffer)) {
        len = (sizeof(buffer) - 5) / 2;
        strncpy(buffer, start, len);
        buffer[len] = '.';
        buffer[len+1] = '.';
        buffer[len+2] = '.';
        strncpy(buffer + len + 3, end - (sizeof(buffer) - len - 3 - 1), sizeof(buffer) - len - 3);
    }
    else
    {
        strncpy(buffer, start, sizeof(buffer));
        buffer[end - start] = '\0';
    }
    buffer[sizeof(buffer) - 1] = '\0';
    return buffer;
#else
    return "";
#endif
}

#define PRINT_INDENT(...) for(uint8_t i = 0; i < indent; i++) printf("  "); printf(__VA_ARGS__)

static void _printNode(const mdNode_t* node, int indent, bool detailed)
{
#ifdef DEBUG
    if (node == NULL)
    {
        PRINT_INDENT("NULL\n");
    }
    else
    {
        switch(node->type)
        {
            case TEXT:
            {
                PRINT_INDENT("Text (0x%08x): %s\n", (int)node, strndebug(node->text.start, node->text.end));
                break;
            }

            case OPTION:
            {
                switch (node->option.type)
                {
                    case STYLE:
                    {
                        if (node->option.style == STYLE_NORMAL)
                        {
                            PRINT_INDENT("Style: Normal (0x%08x)\n", (int)node);
                        }
                        else
                        {
                            PRINT_INDENT("Style: ");
                            if (node->option.style & STYLE_ITALIC)
                            {
                                printf("Italic ");
                            }

                            if (node->option.style & STYLE_UNDERLINE)
                            {
                                printf("Underline ");
                            }

                            if (node->option.style & STYLE_STRIKE)
                            {
                                printf("Strikethrough ");
                            }

                            if (node->option.style & STYLE_BOLD)
                            {
                                printf("Bold ");
                            }


                            printf("(0x%08x)\n", (int)node);
                        }
                        break;
                    }

                    case ALIGN:
                    {
                        PRINT_INDENT("Align:  ");

                        if ((node->option.align & VALIGN_CENTER) == VALIGN_CENTER)
                        {
                            printf("Middle ");
                        }
                        else if (node->option.align & VALIGN_TOP)
                        {
                            printf("Top ");
                        }
                        else if (node->option.align & VALIGN_BOTTOM)
                        {
                            printf("Right ");
                        }


                        if ((node->option.align & ALIGN_CENTER) == ALIGN_CENTER)
                        {
                            printf("Center ");
                        }
                        else if (node->option.align & ALIGN_LEFT)
                        {
                            printf("Left ");
                        }
                        else if (node->option.align & ALIGN_RIGHT)
                        {
                            printf("Right ");
                        }

                        printf("(0x%08x)\n", (int)node);
                        break;
                    }

                    case BREAK:
                    {
                        PRINT_INDENT("Break... (0x%08x)\n", (int)node);
                        break;
                    }

                    case COLOR:
                    {
                        PRINT_INDENT("Color: c%d%d%d (%d) (0x%08x)\n", node->option.color / 36, node->option.color / 6 % 6, node->option.color % 6, node->option.color, (int)node);
                        break;
                    }

                    case FONT:
                    {
                        PRINT_INDENT("Font: '%s' (0x%08x)\n", strndebug(node->option.font.start, node->option.font.end), (int)node);
                        break;
                    }

                    default:
                    {
                        PRINT_INDENT("???? (0x%08x)\n", (int)node);
                        break;
                    }
                }
                break;
            }

            case DECORATION:
            {
                switch (node->decoration)
                {
                    case WORD_BREAK:
                    PRINT_INDENT("Word Break (0x%08x)\n", (int)node);
                    break;

                    case LINE_BREAK:
                    PRINT_INDENT("Line Break (0x%08x)\n", (int)node);
                    break;

                    case PARAGRAPH_BREAK:
                    PRINT_INDENT("Paragraph Break (0x%08x)\n", (int)node);
                    break;

                    case PAGE_BREAK:
                    PRINT_INDENT("Page break (0x%08x)\n", (int)node);
                    break;

                    case HORIZONTAL_RULE:
                    PRINT_INDENT("Horizontal rule (0x%08x)\n", (int)node);
                    break;

                    case BULLET:
                    PRINT_INDENT("Bullet (0x%08x)\n", (int)node);
                    break;

                    case HEADER:
                    PRINT_INDENT("Header (0x%08x)\n", (int)node);
                    break;

                    default:
                    PRINT_INDENT("Something else (0x%08x)\n", (int)node);
                    break;
                }
                break;
            }

            case IMAGE:
            {
                PRINT_INDENT("Image: '%s' (0x%08x)\n", strndebug(node->image.start, node->image.end), (int)node);
                break;
            }

            case CUSTOM:
            {
                PRINT_INDENT("Custom... (0x%08x)\n", (int)node);
                break;
            }

            case EMPTY:
            {
                PRINT_INDENT("EMPTY (0x%08x)\n", (int)node);
                break;
            }

            default:
            {
                PRINT_INDENT("!!! SOMETHING ELSE (%d) !!! (0x%08x)\n", node->type, (int)node);
                break;
            }
        }

        if (detailed)
        {
            PRINT_INDENT("  Parent: 0x%08x\n", (int)node->parent);
            PRINT_INDENT("  Child:  0x%08x\n", (int)node->child);
            PRINT_INDENT("  Next:   0x%08x\n", (int)node->next);
        }
    }
#endif
}

static void printNode(const mdNode_t* node, int indent)
{
    _printNode(node, indent, false);
}

static void printNodeDetailed(const mdNode_t* node, int indent)
{
    _printNode(node, indent, true);
}

// after curNode is set up, checks if you can just merge it with lastNode instead
static bool mergeTextNodes(mdNode_t** curNode, mdNode_t** lastNode)
{
#ifdef DEBUG
    char buf1[64];
    char buf2[64];
    char buf3[64];
#endif
    if ((*curNode)->type == TEXT && *lastNode != NULL && (*lastNode)->type == TEXT && (*lastNode)->text.end == (*curNode)->text.start)
    {
#ifdef DEBUG
        strncpy(buf1, strndebug((*lastNode)->text.start, (*lastNode)->text.end), sizeof(buf1)-1);
        strncpy(buf2, strndebug((*curNode)->text.start, (*curNode)->text.end), sizeof(buf2)-1);
        strncpy(buf3, strndebug((*lastNode)->text.start, (*curNode)->text.end), sizeof(buf3)-1);
        buf1[63] = '\0';
        buf2[63] = '\0';
        buf3[63] = '\0';

        MDLOG("Merging text nodes '%s' and '%s' into '%s'\n", buf1, buf2, buf3);
#endif
        // these nodes are contiguous, so just add onto them
        (*lastNode)->text.end = (*curNode)->text.end;

        // mark the current node as empty
        (*curNode)->type = EMPTY;
        return true;
    }

    return false;
}

static void parseMarkdownInner(const char* text, _markdownText_t* out)
{
    int index = 0;
    mdNode_t* curNode = out->tree = newNode(out, NULL, NULL);
    curNode->index = index++;
    mdNode_t* lastNode = NULL;

    #define START_OF_LINE() (lastNode == NULL || (lastNode->type == DECORATION && lastNode->decoration != BULLET && lastNode->decoration != HEADER && (lastNode->decoration != WORD_BREAK || lastNode->parent == NULL)))
    #define PARENT_IS_SAME_OPTION(valtype) (curNode->parent != NULL && curNode->parent->type == curNode->type && curNode->parent->option.type == curNode->option.type && curNode->parent->option.valtype == curNode->option.valtype)

    // Temporary pointer to the text at the start of the loop so we can backtrack a tiny bit if needed
    const char* textStart;
    nodeAction_t action;

    while (*text)
    {
        // Most actions will just add a sibling, so do that by default
        action = ADD_SIBLING;

        MDLOG("Parsing at %c (\\x%02x)\n", *text, *text);
        textStart = text;

        switch (*text)
        {
            case '\\':
            {
                // is this a simple escape or a control sequence?
                // escapable chars: \ _ * ~ # -
                // reserved escape chars: abefnrtuUvx0123456789
                // escape sequence chars:
                //   c: change color, e.g. "\c000", "\c300", "\c555"
                //   C: revert color
                //   l: alignment, e.g. "\lR" or "\lC", options are L, R, C (left, right, center) and T, B, M (top, bottom, middle (v-center))
                //   L: revert alignment
                //   d: font, e.g. "\d(mm.font)" or "\d(tom_thumb.font)"
                //   D: revert font
                switch (*(++text))
                {
                    case '\\':
                    case '_':
                    case '*':
                    case '~':
                    case '#':
                    case '-':
                    case '!':
                    {
                        // regular escape
                        // create a text node for the single character after the escape
                        MDLOG("Escaping regular character %c (\\x%02x)\n", *text, *text);
                        curNode->type = TEXT;
                        curNode->text.start = text;
                        curNode->text.end = ++text;
                        break;
                    }

                    case 'f':
                    {
                        curNode->type = DECORATION;
                        curNode->decoration = PAGE_BREAK;
                        ++text;

                        break;
                    }

                    case 'c':
                    {
                        bool colorOk = true;
                        int colorVal = 0;
                        for (uint8_t i = 0; i < 3; i++)
                        {
                            ++text;
                            if (*text >= '0' && *text <= '5')
                            {
                                colorVal *= 6;
                                colorVal += ((*text) - '0');
                            }
                            else
                            {
                                colorOk = false;
                            }
                        }

                        if (colorOk)
                        {
                            curNode->type = OPTION;
                            curNode->option.type = COLOR;
                            curNode->option.color = (paletteColor_t)colorVal;
                            ++text;

                            // continue with the next node as a child
                            action = ADD_CHILD;
                        }
                        else
                        {
                            // format doesn't match so just mark the backslash as text
                            curNode->type = TEXT;
                            curNode->text.start = textStart;
                            curNode->text.end = textStart + 1;

                            // continue parsing just after the escape char
                            text = textStart + 1;
                        }

                        break;
                    }

                    case 'C':
                    {
                        curNode->type = OPTION;
                        curNode->option.type = COLOR;
                        // pass type so we don't actually check the value
                        if (PARENT_IS_SAME_OPTION(type))
                        {
                            ++text;
                            // this is correct, there is a color to be reverted
                            // we don't need a new node, but we do need to move the current node from being a child of the color node
                            //    to being a sibling

                            // change the parent's next node to this
                            // we know here that the parent will have no other children, because we only move forward
                            action = GO_TO_PARENT;
                        }
                        else
                        {
                            // we have no color to revert, so just treat this as text
                            curNode->type = TEXT;
                            curNode->text.start = textStart;
                            curNode->text.end = textStart + 1;
                        }
                        break;
                    }

                    // alignment (LCR TMB)
                    // alignment (<|> ^-v)
                    case 'l':
                    {
                        curNode->type = OPTION;
                        curNode->option.type = ALIGN;
                        action = ADD_CHILD;

                        switch (*(++text))
                        {
                            case 'L':
                            case '<':
                            curNode->option.align = ALIGN_LEFT;
                            break;

                            case 'C':
                            case '|':
                            curNode->option.align = ALIGN_CENTER;
                            break;

                            case 'R':
                            case '>':
                            curNode->option.align = ALIGN_RIGHT;

                            case 'T':
                            case '^':
                            curNode->option.align = VALIGN_TOP;
                            break;

                            case 'M':
                            case '-':
                            curNode->option.align = VALIGN_CENTER;
                            break;

                            case 'B':
                            case 'v':
                            curNode->option.align = VALIGN_BOTTOM;
                            break;

                            default:
                            {
                                curNode->type = TEXT;
                                curNode->text.start = textStart;
                                curNode->text.end = text + 1;
                                action = ADD_SIBLING;
                                break;
                            }
                        }

                        ++text;
                        break;
                    }

                    case 'L':
                    {
                        curNode->type = OPTION;
                        curNode->option.type = ALIGN;
                        if (PARENT_IS_SAME_OPTION(type))
                        {
                            ++text;
                            action = GO_TO_PARENT;
                        }
                        else
                        {
                            curNode->type = TEXT;
                            curNode->text.start = textStart;
                            curNode->text.end = ++text;
                            action = ADD_SIBLING;
                        }
                        break;
                    }

                    case 'd':
                    {
                        bool valid = false;
                        if (*(++text) == '(')
                        {
                            do {
                                ++text;
                            } while (*text != ')' && *text != '\0' && *text != '\n');

                            if (*text == ')')
                            {
                                valid = true;
                            }
                        }

                        if (valid)
                        {
                            // found matching paren
                            curNode->type = OPTION;
                            curNode->option.type = FONT;
                            // textStart is '\', textStart+1 is 'd', textStart+2 is '(', so textStart+3 is our font name
                            curNode->option.font.start = textStart + 3;

                            curNode->option.font.end = text;

                            ++text;
                            action = ADD_CHILD;
                        }
                        else
                        {
                            curNode->type = TEXT;
                            curNode->text.start = textStart;
                            curNode->text.end = ++text;
                            action = ADD_SIBLING;
                        }
                        break;
                    }

                    case 'D':
                    {
                        curNode->type = OPTION;
                        curNode->option.type = FONT;
                        if (PARENT_IS_SAME_OPTION(type))
                        {
                            // be responsible and clean up this mess
                            curNode->type = EMPTY;
                            ++text;
                            action = GO_TO_PARENT;
                        }
                        else
                        {
                            curNode->type = TEXT;
                            curNode->text.start = textStart;
                            curNode->text.end = ++text;
                        }
                        break;
                    }

                    // not sure what they're trying to escape
                    // so, just treat it as regular text
                    default:
                    {
                        curNode->type = TEXT;
                        curNode->text.start = textStart;
                        curNode->text.end = ++text;
                        break;
                    }
                }

                break;
            }

            case '#':
            {
                if (START_OF_LINE())
                {
                    MDLOG("Start of line, using header");
                    curNode->type = DECORATION;
                    curNode->decoration = HEADER;
                    ++text;

                    action = ADD_CHILD;
                }
                else
                {
                    MDLOG("Not at start of line, marking as text");
                    curNode->type = TEXT;
                    curNode->text.start = text;
                    curNode->text.end = ++text;
                }
                break;
            }

            case '_':
            case '*':
            case '~':
            {
                while (*(++text) == *textStart);

                curNode->type = OPTION;
                curNode->option.type = STYLE;

                // (check if *text == '\n' and lastNode is LINE_BREAK or PARAGRAPH_BREAK)
                if (text - textStart > 2 && *textStart == '~')
                {
                    text = textStart + 2;
                }

                // ONE CHAR
                //  - Underscore, Asterisk: Italics
                //  - Tilde: Underline (*** Not Standard ***)
                // TWO CHAR
                //  - Underscore, Asterisk: Bold
                //  - Tilde: Strikethrough
                // THREE CHAR
                //  - Underscore, Asterisk: Bold & Italics

                if (text - textStart == 1)
                {
                    if (*textStart == '~')
                    {
                        curNode->option.style = STYLE_UNDERLINE;
                    }
                    else
                    {
                        curNode->option.style = STYLE_ITALIC;
                    }
                }
                else if (text - textStart == 2)
                {
                    if (*textStart == '~')
                    {
                        curNode->option.style = STYLE_STRIKE;
                    }
                    else
                    {
                        curNode->option.style = STYLE_BOLD;
                    }
                }
                else if (text - textStart == 3)
                {
                    // if we're here, *textStart cannot be '~'
                    if (*text == '\n' && START_OF_LINE())
                    {
                        curNode->type = DECORATION;
                        curNode->decoration = HORIZONTAL_RULE;
                    }
                    else
                    {
                        curNode->option.style = STYLE_ITALIC | STYLE_BOLD;
                    }
                }

                if (curNode->type == OPTION && PARENT_IS_SAME_OPTION(style))
                {
                    curNode->type = EMPTY;
                    action = GO_TO_PARENT;
                }
                else if (curNode->type == DECORATION)
                {
                    action = ADD_SIBLING;
                }
                else
                {
                    action = ADD_CHILD;
                }
                break;
            }

            case '-':
            {
                while (*(++text) == *textStart);

                if (text - textStart >= 3 && *text == '\n' && START_OF_LINE())
                {
                    curNode->type = DECORATION;
                    curNode->decoration = HORIZONTAL_RULE;
                }
                else
                {
                    // TODO: Support underlined headers
                    curNode->type = TEXT;
                    curNode->text.start = textStart;
                    curNode->text.end = text;
                }
                break;
            }

            case '!':
            {
                bool valid = false;

                // TODO replace this with something that's not incredibly slow
                if (*(++text) == '[')
                {
                    do {
                        ++text;
                    } while ((*text != ']' || *(text - 1) == '\\') && *text != '\0' && *text != '\n');

                    if (*text == ']')
                    {
                        // that was the alt text
                        if (*(++text) == '(')
                        {
                            curNode->image.start  = text + 1;

                            do {
                                ++text;
                            } while ((*text != ')' || *(text - 1) == '\\') && *text != '\0' && *text != '\n');

                            if (*text == ')')
                            {
                                valid = true;
                                curNode->image.end = text;
                                text++;
                            }
                        }
                    }
                }

                if (valid)
                {
                    curNode->type = IMAGE;
                }
                if (!valid)
                {
                    curNode->type = TEXT;
                    curNode->text.start = textStart;
                    curNode->text.end = textStart + 1;
                    text = textStart + 1;
                }

                break;
            }

            case '\n':
            {
                // If there's one newline, pretend it's a space
                // If there's more than one newline, collapse them all into max two
                int nls = 1;
                while (*(++text) == '\n')
                {
                    nls++;
                }

                curNode->type = DECORATION;
                if (nls > 1)
                {
                    curNode->decoration = PARAGRAPH_BREAK;
                }
                else
                {
                    // check if the last line ended with two spaces
                    if (lastNode != NULL && lastNode->type == TEXT && (lastNode->text.end - lastNode->text.start) >= 2
                        && !strncmp(lastNode->text.end - 2, "  ", 2))
                    {
                        curNode->decoration = LINE_BREAK;
                    }
                    else
                    {
                        curNode->decoration = WORD_BREAK;
                    }
                }

                // TODO: Check if the last token was actually a newline

                if (curNode->parent != NULL
                    && curNode->parent->type == DECORATION
                    && curNode->parent->decoration == HEADER)
                {
                    // if last node was a header, break out of the header and also add the newline
                    action = GO_TO_PARENT | ADD_SIBLING;
                }

                break;
            }


            default:
            {
                // this is regular text, produce a text node
                curNode->type = TEXT;
                curNode->text.start = text;

                // Advance to the next char we need to handle
                text = strpbrk(text + 1, "\\_*~#-\n!");

                if (text == NULL)
                {
                    // end of string!
                    text = curNode->text.start + strlen(curNode->text.start);
                }

                // end is exclusive so set it to the next char pointer
                curNode->text.end = text;
                break;
            }
        }

        if (action & GO_TO_PARENT)
        {
            // so what should happen here...
            // is, instead of allocating a new node and attaching it as a child/sibling
            // we will keep curNode the same, and move it so that it becomes the first sibling
            // of its parent
            // also, we need to make sure that the previous node's next is not set to us anymore
            // and we aren't doing that, so that's probably why Bad Things are happening
            // this effectively "pops out" a level of the parse stack

            MDLOG("Moving to parent\n");

            MDLOG("Old parent:\n");

            printNode(curNode->parent, 2);

            MDLOG("Old parent next:\n");
            printNode(curNode->parent->next, 4);

            MDLOG("Current node:\n");
            printNode(curNode, 2);
            if (curNode == curNode->parent)
            {
                MDLOG("This node is its own child, oh dear\n");
            }

            if (curNode->parent->child == curNode)
            {
                // we are the first child of our parent
                // this means our parent will no longer have a child
                // man these comments are getting weird
                MDLOG("Child removed");
                curNode->parent->child = NULL;
            }

            if (lastNode->next == curNode)
            {
                // we are someone's sibling
                // we should not be, because we are now their... aunt/uncle......
                lastNode->next = NULL;
            }

            // okay, now make our
            lastNode = curNode->parent;
            lastNode->next = curNode;

            // finally, set our parent to our new parent's parent
            if (curNode->parent != NULL)
            {
                curNode->parent = curNode->parent->parent;
            }

#ifdef DEBUG
            MDLOG("New parent:\n");
            printNode(curNode->parent, 2);

            if (curNode->parent != NULL)
            {
                MDLOG("New parent next:\n");
                printNode(curNode->parent->next, 4);
            }

            if (lastNode != NULL)
            {
                // MDLOG("Old parent next:\n");
                printNode(lastNode->next, 2);
            }
#endif
        }

        if (action & ADD_SIBLING)
        {
            // continue with next node as a sibling
            if (!mergeTextNodes(&curNode, &lastNode))
            {
                curNode = newNode(out, curNode->parent, lastNode = curNode);
                curNode->index = ++index;
            }
        }
        else if (action & ADD_CHILD)
        {
            curNode = newNode(out, curNode, lastNode = NULL);
            curNode->index = ++index;
        }
    }

    MDLOG("Dangling(?) node: \n");

    printNodeDetailed(curNode, 0);

    // The last node is dangling -- we already set up the references when it was created
    // but it is no longer needed and should be freed now
    // remove it from its previous sibling
    if (curNode->type == EMPTY)
    {
        if (lastNode != NULL && lastNode->next == curNode)
        {
            lastNode->next = NULL;
        }

        // remove it from its parent, if any
        if (curNode->parent != NULL && curNode->parent->child == curNode)
        {
            curNode->parent->child = NULL;
        }

        if (curNode->child != NULL && curNode->child->parent == curNode)
        {
            curNode->child->parent = NULL;
        }

        MDLOG("Freeing dangling node\n");
        free(curNode);
    }
}

markdownText_t* parseMarkdown(const char* text)
{
    MDLOG("Preparing to parse markdown\n");

    _markdownText_t* result = calloc(1, sizeof(_markdownText_t));

    result->text = text;

    MDLOG("Allocated stuff\n");
    parseMarkdownInner(text, result);

    return result;
}

static int stackFrames = 0;

static void freeSiblings(mdNode_t* tree)
{
    stackFrames++;

    MDLOG("Stack level: %d\n", stackFrames);

    mdNode_t* tmp;
    while (tree != NULL)
    {
        if (tree->child != NULL)
        {
            freeTree(tree->child);
        }

        tmp = tree->next;
        free(tree);
        tree = tmp;
    }
    stackFrames--;
}

static void freeTree(mdNode_t* tree)
{
    stackFrames++;

    MDLOG("Stack level: %d\n", stackFrames);

    // don't use any local variables to hopefully prevent stack overflow
    // otherwise this function would be a pain to implement

    //printNode(tree, indent);

    // first, recursively delete all this node's children
    if (tree->child != NULL)
    {
        freeTree(tree->child);

    }

    // then, iteratively delete this node's next node
    // but only if this is the first child of the parent

    if (tree->next != NULL)
    {
        freeSiblings(tree->next);
    }

    // finally, if the node isn't the root node, delete itself
    free(tree);

    stackFrames--;
}

void freeMarkdown(markdownText_t* markdown)
{
    _markdownText_t* ptr = markdown;

    MDLOG("Freeing Markdown\n------------\n\n");
    if (ptr != NULL)
    {
        freeTree(ptr->tree);
        free(ptr);
    }
}

static const mdOpt_t* findPreviousOption(const mdNode_t* node, mdOptType_t type)
{
    while (node->parent != NULL)
    {
        node = node->parent;
        if (node->type == OPTION && node->option.type == type)
        {
            return &(node->option);
        }
    }

    return NULL;
}

static const textStyle_t findPreviousStyles(const mdNode_t* node, mdPrintState_t* state)
{
    textStyle_t style = state->defaults.style;
    while (node->parent != NULL)
    {
        node = node->parent;
        if (node->type == OPTION && node->option.type == STYLE)
        {
            style |= node->option.style;
        }
    }

    return style;
}

static const font_t* findPreviousFont(const mdNode_t* node, mdPrintState_t* state)
{
    mdText_t* font = NULL;
    while (node->parent != NULL)
    {
        node = node->parent;

        if (node->type == OPTION && node->option.type == FONT)
        {
            font = &(node->option.font);
            break;
        }
    }

    if (font != NULL)
    {
        MDLOG("I don't know what font to return!!!\n");
        // TODO
    }

    return state->defaults.bodyFont;
}

static void leavingNode(const mdNode_t* node, mdPrintState_t* state)
{
    // handle any actions we need to take when leaving the current node
    // i.e. if we're leaving an option node, search up the tree to find the previous value for whatever option
    switch (node->type)
    {
        case DECORATION:
        {
            switch (node->decoration)
            {
                case HEADER:
                {
                    //state->y += textLineHeight(state->font, state->params.style);
                    //state->x = state->params.xMin;
                    state->font = findPreviousFont(node, state);
                    mdOpt_t* newOption = findPreviousOption(node, ALIGN);
                    state->params.align = (newOption != NULL) ? newOption->align : state->defaults.align;
                    break;
                }

                default:
                break;
            }
            break;
        }

        case OPTION:
        {
            if (node->option.type == FONT)
            {
                state->font = findPreviousFont(node, state);
            }
            else if (node->option.type == STYLE)
            {
                // Just set the new style
                state->params.style = findPreviousStyles(node, state);
            }
            else
            {
                mdOpt_t* option = findPreviousOption(node, node->option.type);
                switch (node->option.type)
                {
                    case ALIGN:
                    state->params.align = option ? option->align : state->defaults.align;
                    break;

                    case BREAK:
                    state->params.breakMode = option ? option->breakMode : state->defaults.breakMode;
                    break;

                    case COLOR:
                    state->params.color = option ? option->color : state->defaults.color;
                    break;

                    case STYLE:
                    case FONT:
                    // already handled
                    break;
                }
            }
        }

        default:
        break;
    }
}

static mdLinePartInfo_t* pushLinePart(mdPrintState_t* state)
{
    // add a new mdLinePlan_t onto state->linePlan.parts
    if (state->linePlan.partCount >= state->linePlan.partAlloc)
    {
        state->linePlan.partAlloc += 4;
        state->linePlan.parts = realloc(state->linePlan.parts, sizeof(mdLinePartInfo_t) * state->linePlan.partAlloc);
    }

    state->linePlan.pending = true;
    return state->linePlan.parts + (state->linePlan.partCount)++;
}

static void navigateToNode(const mdNode_t* tree, size_t index, const mdNode_t** nodeOut, const mdNode_t** prevOut)
{
    *prevOut = NULL;
    *nodeOut = tree;

    while ((*nodeOut) != NULL && (*nodeOut)->index != index)
    {
        if ((*nodeOut)->next != NULL && (*nodeOut)->next->index <= index)
        {
            *prevOut = *nodeOut;
            *nodeOut = (*nodeOut)->next;
        }
        else if ((*nodeOut)->child != NULL)
        {
            *prevOut = NULL;
            *nodeOut = (*nodeOut)->child;
        }
        else
        {
            *nodeOut = NULL;
            *prevOut = NULL;
        }
    }

    return;
}

static void resetPlan(mdLinePlan_t* plan)
{
    plan->totalWidth = 0;
    plan->partCount = 0;
    plan->pending = false;
    plan->lineHeight = 0;
    plan->firstTextOffset = 0;
}

static bool drawPlannedLine(display_t* disp, mdPrintState_t* state)
{
    if (state->linePlan.pending && state->linePlan.partCount > 0)
    {
        MDLOG("\nDrawing planned line with %zu parts, width %d, and height %d!\n", state->linePlan.partCount, state->linePlan.totalWidth, state->linePlan.lineHeight);
        state->linePlan.pending = false;

        int16_t startX = state->x;
        int16_t startY = state->y;

        // Set up the initial X depending on the alignment setting
        if ((state->linePlan.align & ALIGN_CENTER) == ALIGN_CENTER)
        {
            startX = (state->params.xMax - state->linePlan.totalWidth) / 2 + state->params.xMin;
            MDLOG("Centering text with offset %d\n", startX);
        }
        else if ((state->linePlan.align & ALIGN_RIGHT) == ALIGN_RIGHT)
        {
            startX = state->params.xMax - state->linePlan.totalWidth;
            MDLOG("Right-aligning text with offset %d\n", startX);
        }
        else
        {
            MDLOG("Left-aligning text with offset %d\n", startX);
        }

        int16_t x = startX;
        int16_t y = startY;
        int16_t yOffset;

        for (size_t i = 0; i < state->linePlan.partCount; i++)
        {
            mdLinePartInfo_t* part = state->linePlan.parts + i;

            switch (part->node->type)
            {
                case TEXT:
                {
                    // Center the text verticlaly within the line
                    // To align the text along the baseline insead, we can simply subtract the part height from the line height
                    yOffset = (state->linePlan.lineHeight - textLineHeight(part->font, part->textStyle)) / 2;

                    if (i > 0 && ((part - 1)->textStyle & STYLE_ITALIC) && !(part->textStyle & STYLE_ITALIC))
                    {
                        // We have switched from italics to non-italics!
                        // We must finally pay for those extra pixels we were borrowing from the next character...
                        // TODO we also have to do this when we measure..
                        x += textWidthAttrs((part - 1)->font, "", (part - 1)->textStyle);
                    }

                    // Account for the offset in Y
                    y = startY + yOffset;

                    // Draw the text. No need to save the return value as the text will definitely 100% fit for sure
                    drawTextWordWrapExtra(disp, part->font, part->color,
                                          // If this is the first node in the line, we may need to start drawing at an offset
                                          // due to partial printing on the previous line
                                          (i == 0 ? state->linePlan.firstTextOffset : 0) + part->node->text.start,
                                          // Actual X and Y, since we're really drawing
                                          &x, &y,
                                          state->params.xMin, state->params.yMin,
                                          // Actually, the last line won't fit! So only draw to the end of the line
                                          // We still don't need to save the return value though, because we double checked and are sure!
                                          state->params.xMax, y + state->linePlan.lineHeight,
                                          part->textStyle, part->node->text.end);

                    // Undo the yOffset so y is the start of the line again
                    // TODO: Do we actually need to keep the Y value? It shouldn't change since we won't actually wrap lines. Or should it?
                    y -= yOffset;

                    break;
                }

                case DECORATION:
                {
                    if (part->node->decoration == WORD_BREAK)
                    {
                        if (x != startX)
                        {
                            x += part->font->chars[0].w + 1;
                            if (x >= state->params.xMax)
                            {
                                x = state->params.xMin;
                                y += state->linePlan.lineHeight;
                            }
                        }
                    }
                    break;
                }

                case IMAGE:
                {

                    break;
                }

                default:
                break;
            }
        }

        state->x = state->params.xMin;
        state->y += state->linePlan.lineHeight;

        resetPlan(&state->linePlan);

        // Return whether we actually drew a line
        return true;
    }

    // Return false, indicating no line needed to be drawn
    // (maybe the caller needs to know if they should add a newline?)
    return false;
}

static const char* planLine(display_t* disp, const mdNode_t* node, mdPrintState_t* state)
{
    // How it works:
    // - First, we measure one line of the text
    // - If 100% of the text fits on a line, we don't draw it
    // - Instead, we add it to a list of (text, textAttrs, width, lineHeight) in the state
    // - UGH, what to do about word break tokens... I guess also keep those in there
    // - Then, once we reach the end of the line, we can print!
    // - If we're handling alignment, we can measure the length of the entire line at once and
    // God, this is going to make the printing even slower... that's 2+ word wrap calls per text...

    MDLOG("Planning line with initial offset %zu\n", state->textPos);

    // Check if the previous part was in italics, and this one isn't
    if (state->linePlan.partCount > 0 && (state->linePlan.parts[state->linePlan.partCount - 1].textStyle & TEXT_ITALIC) && !(state->params.style & TEXT_ITALIC))
    {
        int16_t extraWidth = textWidthAttrs(state->linePlan.parts[state->linePlan.partCount - 1].font, "", state->linePlan.parts[state->linePlan.partCount - 1].textStyle);
        state->linePlan.totalWidth += extraWidth;
        MDLOG("Adding %d extra space to line plan for italics, total=%d\n", extraWidth, state->linePlan.totalWidth);
    }

    // Handle word breaks
    if (node->type == DECORATION && node->decoration == WORD_BREAK)
    {
        // For a word break, we can just ignore it if we're at the start of the line
        if (state->linePlan.pending)
        {
            state->linePlan.totalWidth += state->font->chars[0].w + 1;
        }

        // We really don't care if this makes the width exceed the actual max just yet
        // The next time we try to plan a line, we will immediately find that the text
        // does not fit and draw the planned line. Also, we can't really

        return NULL;
    }
    else if (node->type == TEXT)
    {
        // TODO: I think there's something wrong with state->textPos here...
        // We can discard textPos as soon as we initialize it here
        // But then we use it to set firstTextOffset =
        const char* remainingText = node->text.start + state->textPos;
        state->textPos = 0;

        // TODO: OPTIMIZATION!
        // - Measure the entire text using `drawTextWordWrapExtra(NULL, ...)`
        // - Because the height is uniform, we can just determine the number of lines to draw
        // - Measure and add the remainder to the plan
        // - Draw the planned line as usual
        // - Then draw all but the last remaining line without involving the line planner
        // - (but then we can't do centering... worry about thta later, it's not too much harder)
        // - Add the remainder to the plan as usual

        while (remainingText != NULL)
        {
            int16_t lineHeight = textLineHeight(state->font, state->params.style);
            const char* textStart = remainingText;

            // We're drawing after all the existing text, so take that into account
            // If there's no pending line, this will be zero
            int16_t x = state->linePlan.totalWidth;
            // We're just measuring the height here
            int16_t y = 0;

            MDLOG("Measuring text of length %zu with Y bounds (0, %d)\n", node->text.end - node->text.start, lineHeight);

            // We need to check if the text we're planning to draw will go past the end of the existing line
            // After this, remainingText will point to all text that did not fit on the line
            // And `x` will either point to the
            remainingText = drawTextWordWrapExtra(NULL, state->font, state->params.color, remainingText,
                                                &x, &y, 0, 0,
                                                state->params.xMax - state->params.xMin, lineHeight,
                                                state->params.style, node->text.end);

            MDLOG("Measured %zu characters: '%s'\n", (remainingText == NULL ? node->text.end - textStart : remainingText - textStart),
                  strndebug(textStart, remainingText == NULL ? node->text.end : remainingText));

            // If this will be the first part of the line, set the initial parameters
            if (!state->linePlan.pending)
            {
                MDLOG("First part of line, setting firstTextOffset=%zu and align=%d\n", state->textPos, state->params.align);
                state->linePlan.firstTextOffset = textStart - node->text.start;
                state->linePlan.align = state->params.align;
            }

            // We need a new line part either way
            mdLinePartInfo_t* part = pushLinePart(state);

            // Attach this node to the part
            part->node = node;
            part->textStyle = state->params.style;
            part->color = state->params.color;
            part->font = state->font;

            // Since we started measuring at the original totalWidth, x will already be set to the new value
            // TODO We should be able to use state->linePlan.totalWidth instead of x
            MDLOG("Planned width is %d, original is %d (added %d)\n", x, state->linePlan.totalWidth, x - state->linePlan.totalWidth);
            MDLOG("Y shouldn't have changed, it was %d and is now %d\n", 0, y);
            if (x > state->linePlan.totalWidth)
            {
                state->linePlan.totalWidth = x;
            }
            else
            {
                state->linePlan.totalWidth += textWidthExtra(state->font, textStart, state->params.style, remainingText);
                MDLOG("Adding partial line length, now %d\n", state->linePlan.totalWidth);
            }

            // Check if some or all of the text fits
            if (remainingText == NULL || (remainingText != node->text.start))
            {
                // More text fits, now we need to update the line height!
                if (lineHeight > state->linePlan.lineHeight)
                {
                    MDLOG("Increasing planned line height from %d to %d\n", state->linePlan.lineHeight, lineHeight);
                    state->linePlan.lineHeight = lineHeight;

                    // Check if the line still fits...
                    if (state->y + state->linePlan.lineHeight > state->params.yMax)
                    {
                        // The line no longer fits! WE HAVE TO GO BACK
                        // We know there's a first part since we have just added ourselves
                        state->backtrackIndex = state->linePlan.parts[0].node->index;

                        MDLOG("Line must be backtracked to node %d, offset %zu due to height change (%d + %d > %d)\n",
                            state->backtrackIndex, state->linePlan.firstTextOffset,
                            state->y, state->linePlan.lineHeight, state->params.yMax);

                        // I don't think we actually change textPos, so this may be completely unnecessary
                        // (we also don't really use the actual return value anymore either)
                        return node->text.start + (state->textPos = state->linePlan.firstTextOffset);
                    }
                }
            }

            // If any of the text didn't fit on this line, the line is done
            if (remainingText != NULL)
            {
                MDLOG("Line is complete! Drawing\n\n");
                drawPlannedLine(disp, state);
            }

            if (state->y + lineHeight - 1 > state->params.yMax)
            {
                MDLOG("Returning early from planLine because height exceeds bounds: %d + %d > %d\n", state->y, lineHeight, state->params.yMax);
                state->textPos = remainingText - node->text.start;
                return remainingText;
            }
        }

        // Return NULL to indicate that all this node's text was successfully draw
        return NULL;
    }
    else if (node->type == IMAGE)
    {
        // TODO
    }

    return NULL;
}

static bool drawMarkdownNode(display_t* disp, const mdNode_t* node, const mdNode_t* prev, mdPrintState_t* state)
{
    switch (node->type)
    {
        case TEXT:
        {
            const char* remain = planLine(disp, node, state);

            if (remain != NULL)
            {
                // not everything fit on the screen
                // reset the position and exit
                state->x = state->params.xMin;
                state->y = state->params.yMin;

                // don't save the return value as state->textPos anymore,
                // since we may have to deal with backtracking

                // return true to indicate there's more pages to be drawn
                return true;
            }

            state->textPos = 0;

            break;
        }

        case OPTION:
        {
            switch (node->option.type)
            {
                case STYLE:
                {
                    state->params.style |= node->option.style;
                    break;
                }

                case ALIGN:
                {
                    state->params.align = node->option.align;
                    break;
                }

                case BREAK:
                {
                    state->params.breakMode = node->option.breakMode;
                    break;
                }

                case COLOR:
                {
                    state->params.color = node->option.color;
                    break;
                }

                case FONT:
                {
                    // TODO
                    break;
                }
            }
            break;
        }

        case DECORATION:
        {
            switch (node->decoration)
            {
                case HORIZONTAL_RULE:
                {
                    drawPlannedLine(disp, state);

                    if (disp != NULL)
                    {
                        plotLine(disp, state->params.xMin, state->y, state->params.xMax, state->y, state->params.color, 0);
                    }
                }
                break;

                case WORD_BREAK:
                planLine(disp, node, state);
                break;

                case LINE_BREAK:
                {
                    if (!drawPlannedLine(disp, state))
                    {
                        state->x = state->params.xMin;
                        state->y += textLineHeight(state->font, state->params.style);
                    }
                    break;
                }

                case PARAGRAPH_BREAK:
                {
                    if (drawPlannedLine(disp, state))
                    {
                        // A line was drawn, so we only need to add one extra line
                        state->y += textLineHeight(state->font, state->params.style);
                    }
                    else
                    {
                        // There was no pending line to draw, so add two lines
                        state->x = state->params.xMin;
                        state->y += textLineHeight(state->font, state->params.style) * 2;
                    }
                    break;
                }

                case PAGE_BREAK:
                drawPlannedLine(disp, state);
                state->x = state->params.xMin;
                state->y = state->params.yMin;
                break;

                case BULLET:
                break;

                case HEADER:
                {
                    drawPlannedLine(disp, state);
                    // Add a line break but only if we're not at the beginning of a line
                    // drawPlannedLine()
                    //state->x = state->params.xMin;
                    //state->y += textLineHeight(state->font, state->params.style);
                    state->font = state->params.headerFont;
                    state->params.align = ALIGN_CENTER;
                    break;
                }
            }
        }

        default:
        break;
    }

    return false;
}


bool drawMarkdown(display_t* disp, const markdownText_t* markdown, const markdownParams_t* params, markdownContinue_t** pos, bool savePos)
{
    const mdNode_t* node = ((const _markdownText_t*)markdown)->tree;
    const mdNode_t* prev = NULL;

    uint8_t indent = 0;
    size_t index = 0;

    mdPrintState_t state =
    {
        .x = params->xMin,
        .y = params->yMin,
        .font = NULL,
        .data = { NULL },
        .textPos = 0,
        .backtrackIndex = NO_BACKTRACK,
    };

    memcpy(&state.params, params, sizeof(markdownParams_t));
    if (state.params.align == 0)
    {
        state.params.align = ALIGN_LEFT | VALIGN_TOP;
    }

    memcpy(&state.defaults, &state.params, sizeof(markdownParams_t));

    state.linePlan.partAlloc = 4;
    state.linePlan.parts = MD_MALLOC(sizeof(mdLinePartInfo_t) * state.linePlan.partAlloc);
    resetPlan(&state.linePlan);

    state.font = params->bodyFont;

    if (pos != NULL && *pos != NULL)
    {
        index = ((_markdownContinue_t*)*pos)->treeIndex;
        state.textPos = ((_markdownContinue_t*)*pos)->textPos;

        navigateToNode(node, index, &node, &prev);
    }

    MDLOG("Printing Markdown\n-----------\n\n");

    while (node != NULL)
    {
        printNode(node, indent);

        // drawMarkdownNode() returns true if it has more to draw
        if (drawMarkdownNode(disp, node, prev, &state))
        {
            if (savePos && pos != NULL)
            {
                if (*pos == NULL)
                {
                    *pos = malloc(sizeof(_markdownContinue_t));
                }

                ((_markdownContinue_t*)*pos)->treeIndex = (state.backtrackIndex == NO_BACKTRACK) ? index : state.backtrackIndex;

                // drawMarkdownNode() also writes its partial position to `state.textPos`
                ((_markdownContinue_t*)*pos)->textPos = state.textPos;
            }
            // We drew as much as we could draw! Don't update the node!

            return true;
        }

        ++index;

        if (node->child != NULL)
        {
            prev = NULL;
            node = node->child;
            indent++;
        }
        else if (node->next != NULL)
        {
            leavingNode(node, &state);
            prev = node;
            node = node->next;
        }
        else {
            // move up the parent tree until one of them has a next
            while (node != NULL)
            {
                leavingNode(node, &state);
                indent--;
                node = node->parent;
                if (node != NULL && node->next != NULL)
                {
                    leavingNode(node, &state);
                    prev = node;
                    node = node->next;
                    break;
                }
            }
        }
    }

    if (savePos && pos != NULL)
    {
        if (*pos != NULL)
        {
            free(*pos);
            *pos = NULL;
        }
    }

    free(state.linePlan.parts);

    return false;
}

markdownContinue_t* copyContinue(const markdownContinue_t* pos)
{
    if (pos == NULL)
    {
        return NULL;
    }

    markdownContinue_t* result = malloc(sizeof(_markdownContinue_t));
    memcpy(result, pos, sizeof(_markdownContinue_t));

    return result;
}