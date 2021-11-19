#ifndef _LCD_CONF_H_
#define _LCD_CONF_H_

// Using SPI2 in the example, as it aslo supports octal modes on some targets
#define LCD_HOST       SPI2_HOST
// To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many.
// More means more memory use, but less overhead for setting up / finishing transfers. Make sure 240
// is dividable by this.
#define PARALLEL_LINES 16

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  0
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8

// The pixel number in horizontal and vertical
#define _0_96
#if defined(_0_96)
    #define EXAMPLE_LCD_PIXEL_CLOCK_HZ (40 * 1000*1000)
    #define EXAMPLE_LCD_H_RES 160
    #define EXAMPLE_LCD_V_RES  80
    #define X_OFFSET            1
    #define Y_OFFSET           26
    #define SWAP_XY          true
    #define MIRROR_X        false
    #define MIRROR_Y         true
#elif defined(_1_14)
    #define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)
    #define EXAMPLE_LCD_H_RES 240
    #define EXAMPLE_LCD_V_RES 135
    #define X_OFFSET           40
    #define Y_OFFSET           52
    #define SWAP_XY          true
    #define MIRROR_X        false
    #define MIRROR_Y         true
#elif defined(_1_3)
    #define EXAMPLE_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)
    #define EXAMPLE_LCD_H_RES 240
    #define EXAMPLE_LCD_V_RES 240
    #define X_OFFSET            0
    #define Y_OFFSET            0
    #define SWAP_XY         false
    #define MIRROR_X        false
    #define MIRROR_Y        false
#else
    #error "Please pick a screen size"
#endif

#endif