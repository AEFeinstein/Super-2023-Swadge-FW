#ifndef _SECRET_TEXT_H_
#define _SECRET_TEXT_H_

#include <stdint.h>
#include "palette.h"

typedef struct {
    const char** text;
    uint16_t lines;
    paletteColor_t color;
} textOption;

const textOption* getTextOpts(uint8_t * numTexts);

#endif