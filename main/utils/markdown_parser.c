#include "markdown_parser.h"

#include <stddef.h>
#include <malloc.h>
#include <esp_log.h>
#include <string.h>

#include "palette.h"
#include "display.h"
#include "rich_text.h"

#define MDLOG(...) printf(__VA_ARGS__)
//ESP_LOGD("Markdown", __VA_ARGS__)
#define MIN(x,y) ((x)<(y)?(x):(y))

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
        font_t* font;
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
} mdDec_t;

typedef struct
{
    wsg_t wsg;
} mdImg_t;

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
    // Is parent necessary?
    struct mdNode* parent;
    struct mdNode* child;
    struct mdNode* next;

    mdNodeType_t type;

    union {
        mdText_t text;
        mdOpt_t option;
        mdDec_t decoration;
        mdImg_t image;
        void* custom;
    };
} mdNode_t;

typedef struct
{
    const char* text;
    mdNode_t tree;
} _markdownText_t;

typedef struct _markdownContinue_t
{
    const char* textPos;
    mdNode_t* treePos;
} _markdownContinue_t;

typedef struct
{
    int16_t x, y;
    textStyle_t style;
    textAlign_t align;
    textBreak_t breakMode;
    paletteColor_t color;
    font_t* font;

    void* data[16];
} mdPrintState_t;

static void parseMarkdownInner(const char* text, _markdownText_t* out);

// Returns a pointer to a new node, allocating more space if necessary, and sets up pointers to the parent and from

///
///

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
static void drawMarkdownNode(const mdNode_t* cur, const mdNode_t* last, mdPrintState_t* state);


static mdNode_t* newNode(_markdownText_t* data, mdNode_t* parent, mdNode_t* prev)
{
    if (parent == NULL && prev == NULL)
    {
        ESP_LOGE("Markdown", "ERR - Attempt to add unreachable node");
        return NULL;
    }

    mdNode_t* result = malloc(sizeof(mdNode_t));
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
    static char buffer[64];

    size_t len = (end - start);

    if (len >= sizeof(buffer)) {
        len = (sizeof(buffer) - 5) / 2;
        strncpy(buffer, start, len);
        buffer[len] = '.';
        buffer[len+1] = '.';
        buffer[len+2] = '.';
        strncpy(buffer + len + 3, end - len, sizeof(buffer) - len - 3);
    }
    else
    {
        strncpy(buffer, start, sizeof(buffer));
        buffer[end - start] = '\0';
    }
    buffer[sizeof(buffer) - 1] = '\0';
    return buffer;
}

#define PRINT_INDENT(...) for(uint8_t i = 0; i < indent; i++) printf("  "); printf(__VA_ARGS__)

static void _printNode(const mdNode_t* node, int indent, bool detailed)
{
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
                        PRINT_INDENT("ALign... (0x%08x)\n", (int)node);
                        break;
                    }

                    case BREAK:
                    {
                        PRINT_INDENT("Break... (0x%08x)\n", (int)node);
                        break;
                    }

                    case COLOR:
                    {
                        PRINT_INDENT("Color: c%d%d%d (0x%08x)\n", node->option.color / 36, node->option.color / 6 % 6, node->option.color % 6, (int)node);
                        break;
                    }

                    case FONT:
                    {
                        PRINT_INDENT("Font... (0x%08x)\n", (int)node);
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

                    default:
                    PRINT_INDENT("Something else (0x%08x)\n", (int)node);
                    break;
                }
                break;
            }

            case IMAGE:
            {
                PRINT_INDENT("Image... (0x%08x)\n", (int)node);
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
    if ((*curNode)->type == TEXT && *lastNode != NULL && (*lastNode)->type == TEXT && (*lastNode)->text.end == (*curNode)->text.start)
    {
        MDLOG("Merging text nodes '%s' and '%s' into '%s'", strndebug((*lastNode)->text.start, (*lastNode)->text.end), strndebug((*curNode)->text.start, (*curNode)->text.end), strndebug((*lastNode)->text.start, (*curNode)->text.end));
        // these nodes are contiguous, so just add onto them
        (*lastNode)->text.end = (*curNode)->text.end;
        return true;
    }

    return false;
}

static void parseMarkdownInner(const char* text, _markdownText_t* out)
{
    mdNode_t* curNode = &(out->tree);
    mdNode_t* lastNode = NULL;

    #define NEXT_SIBLING curNode = newNode(out, curNode->parent, lastNode = curNode)
    #define NEXT_CHILD curNode = newNode(out, curNode, lastNode = NULL)

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
                    {
                        // regular escape
                        // create a text node for the single character after the escape
                        MDLOG("Escaping regular character %c (\\x%02x)\n", *text, *text);
                        curNode->type = TEXT;
                        curNode->text.start = text;
                        curNode->text.end = ++text;
                        break;
                    }

                    case 'c':
                    {
                        bool colorOk = true;
                        int colorVal = 0;
                        for (uint8_t i = 0; i < 3; i++)
                        {
                            text++;
                            if (*text >= '0' && *text <= '5')
                            {
                                colorVal *= 6;
                                colorVal += (*text - '0');
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
                        if (curNode->parent != NULL && curNode->parent->type == OPTION && curNode->parent->option.type == COLOR)
                        {
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
                            text = textStart + 1;
                        }
                        break;
                    }

                    case 'l':
                    break;

                    case 'L':
                    break;

                    case 'd':
                    break;
                }

                break;
            }

            case '#':
            {
                if (lastNode == NULL || (lastNode->type == DECORATION && lastNode->decoration != BULLET))
                {
                    curNode->type = OPTION;
                    curNode->option.type = FONT;
                    // TODO use a font!!!
                    curNode->option.font = NULL;
                    text++;

                    action = ADD_CHILD;
                }
                else
                {
                    curNode->type = TEXT;
                    curNode->text.start = text;
                    curNode->text.end = ++text;
                }
                break;
            }

            case '_':
            case '*':
            {
                while (*(++text) == *textStart);

                curNode->type = OPTION;
                curNode->option.type = STYLE;

                if (text - textStart == 1)
                {
                    curNode->option.style = STYLE_ITALIC;
                }
                else if (text - textStart == 2)
                {
                    curNode->option.style = STYLE_BOLD;
                }
                else if (text - textStart == 3)
                {
                    curNode->option.style = STYLE_ITALIC | STYLE_BOLD;
                }

                if (curNode->parent != NULL
                    && curNode->parent->type == OPTION
                    && curNode->parent->option.type == STYLE
                    && curNode->parent->option.style == curNode->option.style)
                {
                    action = GO_TO_PARENT;
                }
                else
                {
                    action = ADD_CHILD;
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


                if (nls > 1)
                {
                    curNode->type = DECORATION;
                    curNode->decoration = PARAGRAPH_BREAK;
                }
                else
                {
                    curNode->type = DECORATION;
                    curNode->decoration = WORD_BREAK;
                }

                break;
            }


            default:
            {
                // this is regular text, produce a text node
                curNode->type = TEXT;
                curNode->text.start = text;

                // Advance to the next char we need to handle
                text = strpbrk(text + 1, "\\_*~#-\n");

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
            curNode->parent = curNode->parent->parent;
            // TODO is this correct

            MDLOG("New parent:\n");
            printNode(curNode->parent, 2);

            MDLOG("New parent next:\n");
            printNode(curNode->parent->next, 4);

            MDLOG("Old parent next:\n");
            printNode(lastNode->next, 2);
        }

        if (action & ADD_SIBLING)
        {
            // continue with next node as a sibling
            if (!mergeTextNodes(&curNode, &lastNode))
            {
                curNode = newNode(out, curNode->parent, lastNode = curNode);
            }
        }
        else if (action & ADD_CHILD)
        {
            curNode = newNode(out, curNode, lastNode = NULL);
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
    result->tree.parent = NULL;

    MDLOG("Allocated stuff\n");
    parseMarkdownInner(text, result);

    return result;
}

static void freeTree(mdNode_t* tree)
{
    static int indent = 0;
    // don't use any local variables to hopefully prevent stack overflow
    // otherwise this function would be a pain to implement

    printNode(tree, indent);

    // first, recursively delete all this node's children
    if (tree->child != NULL)
    {
        indent++;
        //PRINT_INDENT("Freeing children...\n");
        freeTree(tree->child);
        // TODO uncomment this after we fix the actual problem
        tree->child = NULL;

        indent--;
    }

    // then, iteratively delete this node's next node
    // but only if this is the first child of the parent
    if (tree->parent == NULL || tree->parent->child == tree)
    {
        mdNode_t* tmp;
        mdNode_t* sibling = tree->next;
        while (sibling != NULL)
        {
            tmp = sibling->next;

            //PRINT_INDENT("Freeing siblings...\n");
            freeTree(sibling);

            sibling = tmp;
        }
        // just for logging making sense
        tree->next = NULL;
    }

    // finally, if the node isn't the root node, delete itself
    if (tree->parent != NULL)
    {
        if (tree->parent->child == tree)
        {
            tree->parent->child = NULL;
        }

        free(tree);
    }
    else
    {
        MDLOG("Reached node. Done!\n");
    }
}

void freeMarkdown(markdownText_t* markdown)
{
    _markdownText_t* ptr = markdown;

    MDLOG("Freeing Markdown\n------------\n\n");

    if (ptr != NULL)
    {
        freeTree(&ptr->tree);
        free(ptr);
    }
}

static void drawMarkdownNode(const mdNode_t* node, const mdNode_t* prev, mdPrintState_t* state)
{

}

void drawMarkdown(const markdownText_t* markdown, markdownContinue_t* pos)
{

    const mdNode_t* node = &(((const _markdownText_t*)markdown)->tree);
    const mdNode_t* prev = NULL;
    char buf[64];
    uint8_t indent = 0;

    mdPrintState_t state =
    {
        .x = 0,
        .y = 0,
        .style = STYLE_NORMAL,
        .align = ALIGN_LEFT | VALIGN_TOP,
        .breakMode = BREAK_WORD,
        .color = c555,
        .font = NULL,

        .data = { NULL },
    };

    MDLOG("Printing Markdown\n-----------\n\n");
    while (node != NULL)
    {
        printNode(node, indent);
        drawMarkdownNode(node, prev, &state);

        if (node->child != NULL)
        {
            prev = NULL;
            node = node->child;
            indent++;
        }
        else if (node->next != NULL)
        {
            prev = node;
            node = node->next;
        }
        else {
            // move up the parent tree until one of them has a next
            while (node != NULL)
            {
                indent--;
                node = node->parent;
                if (node != NULL && node->next != NULL)
                {
                    prev = node;
                    node = node->next;
                    break;
                }
            }
        }
    }
}