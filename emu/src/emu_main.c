#include <stdbool.h>

#include "emu_main.h"
#include "esp_emu.h"
#include "esp_log.h"

//Make it so we don't need to include any other C files in our build.
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

/// Display memory
uint32_t * bitmapDisplay = NULL; //0xRRGGBBAA
int bitmapWidth = 0;
int bitmapHeight = 0;

/**
 * @brief
 *
 * @param keycode
 * @param bDown
 */
void HandleKey( int keycode, int bDown )
{
    ESP_LOGE("RAW", "%s NOT IMPLEMENTED (%d %d)", __func__, keycode, bDown);
}

/**
 * @brief
 *
 * @param x
 * @param y
 * @param button
 * @param bDown
 */
void HandleButton( int x, int y, int button, int bDown )
{
    ESP_LOGE("RAW", "%s NOT IMPLEMENTED (%d %d %d %d)", __func__, x, y, button, bDown);
}

/**
 * @brief
 *
 * @param x
 * @param y
 * @param mask
 */
void HandleMotion( int x, int y, int mask )
{
    ESP_LOGE("RAW", "%s NOT IMPLEMENTED (%d %d %d)", __func__, x, y, mask);
}

/**
 * @brief Free memory on exit
 */
void HandleDestroy()
{
	if(NULL != bitmapDisplay)
	{
		free(bitmapDisplay);
	}
}

/**
 * @brief TODO
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char ** argv)
{
    app_main();

    while(true)
    {
        runAllTasks();
    }
    return 0;
}

/**
 * @brief TODO
 * 
 * @param w 
 * @param h 
 */
void initRawDraw(int w, int h)
{
	CNFGSetup( "Swadge S2 Emulator", w, h);

	// ARGB pixels
	bitmapDisplay = malloc(sizeof(uint32_t) * w * h);
	bitmapWidth = w;
	bitmapHeight = h;
}

/**
 * @brief TODO
 * 
 * @param x 
 * @param y 
 * @param px 
 */
void emuSetPx(int16_t x, int16_t y, rgba_pixel_t px)
{
	// Convert from 15 bit to 24 bit color
	uint8_t r8 = ((px.rgb.c.r * 0xFF) / 0x1F) & 0xFF;
	uint8_t g8 = ((px.rgb.c.g * 0xFF) / 0x3F) & 0xFF;
	uint8_t b8 = ((px.rgb.c.b * 0xFF) / 0x1F) & 0xFF;
	bitmapDisplay[(bitmapWidth * y) + x] = (px.a << 24) | (r8 << 16) | (g8 << 8) | (b8);
}

/**
 * @brief TODO
 * 
 * @param x 
 * @param y 
 * @return rgb_pixel_t 
 */
rgb_pixel_t emuGetPx(int16_t x, int16_t y)
{
	uint32_t argb = bitmapDisplay[(bitmapWidth * y) + x];
	rgb_pixel_t px;
	px.c.r = (((argb & 0xFF0000) >> 16) * 0x1F) / 0xFF; // 5 bit
	px.c.g = (((argb & 0x00FF00) >>  8) * 0x3F) / 0xFF; // 5 bit
	px.c.b = (((argb & 0x0000FF) >>  0) * 0x1F) / 0xFF; // 5 bit
	return px;
}

/**
 * @brief TODO
 * 
 */
void emuClearPx(void)
{
	memset(bitmapDisplay, 0x00, sizeof(uint32_t) * bitmapWidth * bitmapHeight);
}

/**
 * @brief TODO
 * 
 * @param drawDiff 
 */
void emuDrawDisplay(bool drawDiff)
{
	// Black Background
	CNFGBGColor = 0x000000FF;
	CNFGClearFrame();

	// Draw bitmap to screen
	CNFGUpdateScreenWithBitmap(bitmapDisplay, bitmapWidth, bitmapHeight);

	//Display the image and wait for time to display next frame.
	CNFGSwapBuffers();
}

/**
 * @brief When the main task yields, handle inputs
 * 
 */
void onTaskYield(void)
{
	CNFGHandleInput();
}
