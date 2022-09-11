#include "swadge_util.h"

/**
 * @brief A table that can be used for gamma correction.
 *
 * Maps a uint8_t value to a gamma-corrected uint8_t.
 */
const uint32_t gamma_correction_table[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6,
    6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12,
    12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19,
    20, 20, 21, 22, 22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 29,
    30, 30, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 39, 39, 40, 41,
    42, 43, 43, 44, 45, 46, 47, 48, 49, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
    73, 74, 75, 76, 77, 78, 79, 81, 82, 83, 84, 85, 87, 88, 89, 90,
    91, 93, 94, 95, 97, 98, 99, 100, 102, 103, 105, 106, 107, 109, 110, 111,
    113, 114, 116, 117, 119, 120, 121, 123, 124, 126, 127, 129, 130, 132, 133, 135,
    137, 138, 140, 141, 143, 145, 146, 148, 149, 151, 153, 154, 156, 158, 159, 161,
    163, 165, 166, 168, 170, 172, 173, 175, 177, 179, 181, 182, 184, 186, 188, 190,
    192, 194, 196, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221,
    223, 225, 227, 229, 231, 234, 236, 238, 240, 242, 244, 246, 248, 251, 253, 255
};


/**
 * @brief A full general-purpose HSV to HEX converter
 *
 * @param hue     The hue 0..255
 * @param sat     Saturation 0..255
 * @param val     Value 0..255
 * @param applyGamma Whether or not to apply gamma to the output.
 */
uint32_t EHSVtoHEXhelper( uint8_t hue, uint8_t sat, uint8_t val, bool applyGamma)
{
#define SIXTH1 43
#define SIXTH2 85
#define SIXTH3 128
#define SIXTH4 171
#define SIXTH5 213
    uint16_t or = 0, og = 0, ob = 0;

    // move in rainbow order RYGCBM as hue from 0 to 255

    if( hue < SIXTH1 ) //Ok: Red->Yellow
    {
        or = 255;
        og = (hue * 255) / (SIXTH1);
    }
    else if( hue < SIXTH2 ) //Ok: Yellow->Green
    {
        og = 255;
        or = 255 - (hue - SIXTH1) * 255 / SIXTH1;
    }
    else if( hue < SIXTH3 )  //Ok: Green->Cyan
    {
        og = 255;
        ob = (hue - SIXTH2) * 255 / (SIXTH1);
    }
    else if( hue < SIXTH4 ) //Ok: Cyan->Blue
    {
        ob = 255;
        og = 255 - (hue - SIXTH3) * 255 / SIXTH1;
    }
    else if( hue < SIXTH5 ) //Ok: Blue->Magenta
    {
        ob = 255;
        or = (hue - SIXTH4) * 255 / SIXTH1;
    }
    else //Magenta->Red
    {
        or = 255;
        ob = 255 - (hue - SIXTH5) * 255 / SIXTH1;
    }

    uint16_t rv = val;
    if( rv > 128 )
    {
        rv++;
    }
    uint16_t rs = sat;
    if( rs > 128 )
    {
        rs++;
    }

    //or, og, ob range from 0...255 now.
    //Apply saturation giving OR..OB == 0..65025
    or = or * rs + 255 * (256 - rs);
    og = og * rs + 255 * (256 - rs);
    ob = ob * rs + 255 * (256 - rs);
    or >>= 8;
    og >>= 8;
    ob >>= 8;
    //back to or, og, ob range 0...255 now.
    //Need to apply saturation and value.
    or = (or * val) >> 8;
    og = (og * val) >> 8;
    ob = (ob * val) >> 8;
    //  printf( "  hue = %d r=%d g=%d b=%d rs=%d rv=%d\n", hue, or, og, ob, rs, rv );
    if (applyGamma)
    {
        or = gamma_correction_table[or];
        og = gamma_correction_table[og];
        ob = gamma_correction_table[ob];
    }
    return or | (og << 8) | ((uint32_t)ob << 16);
    //return og | ( or << 8) | ((uint32_t)ob << 16); //grb
}

/**
 * @brief A wrapper for EHSVtoHEXhelper
 *
 * Parameters are the same as EHSVtoHEXhelper except that it returns an LED.
 */
led_t SafeEHSVtoHEXhelper( int16_t hue, int16_t sat, int16_t val, bool applyGamma )
{
	//Don't clamp hue.
	if( sat > 255 ) sat = 255;
	if( sat < 0 ) sat = 0;
	if( val > 255 ) val = 255;
	if( val < 0 ) val = 0;
	uint32_t r = EHSVtoHEXhelper( (uint8_t)hue, sat, val, applyGamma );
	led_t ret;
	ret.g = (r>>8)&0xff;
	ret.r = r&0xff;
	ret.b = (r>>16)&0xff;
	return ret;
}
