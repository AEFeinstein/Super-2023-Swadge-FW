#ifndef _BTN_H_
#define _BTN_H_

#include <stdbool.h>

typedef struct
{
    uint16_t state;
    uint8_t button;
    bool down;
} buttonEvt_t;

void initButtons(uint8_t numButtons, ...);
void deinitializeButtons(void);
bool checkButtonQueue(buttonEvt_t*);

#endif