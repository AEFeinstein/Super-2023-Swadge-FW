#ifndef _SECRET_TEXT_H_
#define _SECRET_TEXT_H_

#include <stdint.h>

typedef struct {
    const char** text;
    uint16_t lines;
} textOption;

const textOption* getTextOpts(uint8_t * numTexts);

#endif