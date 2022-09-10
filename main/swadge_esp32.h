#ifndef _SWADGE_ESP32_H_
#define _SWADGE_ESP32_H_

#include <stdint.h>

#define IS_ARRAY(arr)   ((void*)&(arr) == &(arr)[0])
#define STATIC_EXP(e)   (0 * sizeof (struct { int ARRAY_SIZE_FAILED:(2 * (e) - 1);}))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + STATIC_EXP(IS_ARRAY(arr)))

// Get current cycle count of processor for profiling.  It is in 1/F_CPU units.
// This will actually compile down to be included in the code, itself, and
// "should" (does in all the tests I've run) execute in one clock cycle since
// there is no function call and rsr only takes one cycle to complete. 
static inline uint32_t getCycleCount()
{
	uint32_t ccount;
	asm volatile("rsr %0,ccount":"=a" (ccount));
	return ccount;
}

void app_main(void);
void setFrameRateUs(uint32_t frameRate);



#endif