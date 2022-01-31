#ifndef _EMU_MAIN_H_
#define _EMU_MAIN_H_

#include <stdint.h>
#include <stdbool.h>
#include "display.h"
#include "btn.h"
#include "led_util.h"

void onTaskYield(void);

void setInputKeys(uint8_t numButtons, char * keyOrder);
bool checkInputKeys(buttonEvt_t * evt);

#endif