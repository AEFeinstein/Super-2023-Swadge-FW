#ifndef _SWADGE_UTIL_H
#define _SWADGE_UTIL_H

// My intent is for this to be a genreal utility functions file, for things that would
// need to be the same between emu and swadge.

#include "led_util.h"
#include "display.h"

#define IS_ARRAY(arr)   ((void*)&(arr) == &(arr)[0])
#define STATIC_EXP(e)   (0 * sizeof (struct { int ARRAY_SIZE_FAILED:(2 * (e) - 1);}))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + STATIC_EXP(IS_ARRAY(arr)))


/**
 * @brief Get current cycle count of processor for profiling.  It is in 1/F_CPU units.
 *   This will actually compile down to be included in the code, itself, and
 *   "should" (does in all the tests I've run) execute in one clock cycle since
 *   there is no function call and rsr only takes one cycle to complete. 
 *
 * @return Number of processor cycles.
 */
static inline uint32_t getCycleCount()
{
    uint32_t ccount;
    asm volatile("rsr %0,ccount":"=a" (ccount));
    return ccount;
}

/**
 * @brief Special ROM function for printing one char to the UART with very low
 *    overhead; useful for profiling.
 */
void uart_tx_one_char( char c );

uint32_t EHSVtoHEXhelper( uint8_t hue, uint8_t sat, uint8_t val, bool applyGamma);
led_t SafeEHSVtoHEXhelper( int16_t hue, int16_t sat, int16_t val, bool applyGamma );
paletteColor_t paletteHsvToHex( int16_t hue, int16_t sat, int16_t val);
paletteColor_t RGBtoPalette( uint32_t rgb ); // takes RGB where R is rgb&0xff G, 0xff00, and B, 0xff0000

#endif
