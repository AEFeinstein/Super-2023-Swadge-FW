#ifndef _BTN_H_
#define _BTN_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    UP     = 0x01,
    DOWN   = 0x02,
    LEFT   = 0x04,
    RIGHT  = 0x08,
    BTN_A  = 0x10,
    BTN_B  = 0x20,
    START  = 0x40,
    SELECT = 0x80,
} buttonBit_t;

typedef struct
{
    uint16_t state;
    buttonBit_t button;
    bool down;
} buttonEvt_t;

void initButtons(uint8_t numButtons, ...);
void deinitButtons(void);
bool checkButtonQueue(buttonEvt_t*);

#endif