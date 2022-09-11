#ifndef _SWADGE_UTIL_H
#define _SWADGE_UTIL_H

// My intent is for this to be a genreal utility functions file, for things that would
// need to be the same between emu and swadge.

#include "led_util.h"

uint32_t EHSVtoHEXhelper( uint8_t hue, uint8_t sat, uint8_t val, bool applyGamma);
led_t SafeEHSVtoHEXhelper( int16_t hue, int16_t sat, int16_t val, bool applyGamma );

#endif
