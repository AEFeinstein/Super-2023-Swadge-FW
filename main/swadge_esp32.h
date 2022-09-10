#ifndef _SWADGE_ESP32_H_
#define _SWADGE_ESP32_H_

#include <stdint.h>

#define IS_ARRAY(arr)   ((void*)&(arr) == &(arr)[0])
#define STATIC_EXP(e)   (0 * sizeof (struct { int ARRAY_SIZE_FAILED:(2 * (e) - 1);}))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + STATIC_EXP(IS_ARRAY(arr)))

void app_main(void);
void setFrameRateUs(uint32_t frameRate);

#endif