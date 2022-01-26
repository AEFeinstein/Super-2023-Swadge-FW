#include <stdbool.h>
#include <unistd.h>

#include "emu_main.h"
#include "esp_emu.h"
#include "esp_log.h"
#include "swadge_esp32.h"

//Make it so we don't need to include any other C files in our build.
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

/// Display memory
volatile uint32_t * bitmapDisplay = NULL; //0xRRGGBBAA
int bitmapWidth = 0;
int bitmapHeight = 0;
volatile bool shouldDrawTft = false;

/**
 * @brief
 *
 * @param keycode
 * @param bDown
 */
void HandleKey( int keycode, int bDown )
{
	WARN_UNIMPLEMENTED();
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
    WARN_UNIMPLEMENTED();
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
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Free memory on exit
 */
void HandleDestroy()
{
	// Upon exit, stop all tasks
	joinThreads();

	// Then free display memory
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
int main(int argc UNUSED, char ** argv UNUSED)
{
	// First initialize rawdraw
	// Screen-specific configurations
#if defined(CONFIG_ST7735_160x80)
    #define TFT_WIDTH         160
    #define TFT_HEIGHT         80
#elif defined(CONFIG_ST7789_240x135)
    #define TFT_WIDTH         240
    #define TFT_HEIGHT        135
#elif defined(CONFIG_ST7789_240x240)
    #define TFT_WIDTH         240
    #define TFT_HEIGHT        240
#else
    #error "Please pick a screen size"
#endif
	CNFGSetup( "Swadge S2 Emulator", TFT_WIDTH, TFT_HEIGHT);

	// ARGB pixels
	bitmapDisplay = malloc(sizeof(uint32_t) * TFT_WIDTH * TFT_HEIGHT);
	memset(bitmapDisplay, 0, sizeof(uint32_t) * TFT_WIDTH * TFT_HEIGHT);
	bitmapWidth = TFT_WIDTH;
	bitmapHeight = TFT_HEIGHT;

	// This is the 'main' that gets called when the ESP boots
    app_main();

	// Spin around waiting for a program exit afterwards
    while(1)
    {
		// Always handle inputs
		CNFGHandleInput();
		
		// Only redraw the screen when requested by the thread
		if(shouldDrawTft)
		{
			// Lower flag
			shouldDrawTft = false;

			// Black Background
			CNFGBGColor = 0x000000FF;
			CNFGClearFrame();

			// Draw bitmap to screen
			CNFGUpdateScreenWithBitmap(bitmapDisplay, bitmapWidth, bitmapHeight);

			//Display the image and wait for time to display next frame.
			CNFGSwapBuffers();
		}

		// Sleep for a ms
        usleep(1000);
    }

	return 0;
}

/**
 * @brief TODO
 * 
 * @param x 
 * @param y 
 * @param px 
 */
void emuSetPxTft(int16_t x, int16_t y, rgba_pixel_t px)
{
	// Convert from 15 bit to 24 bit color
	if(PX_OPAQUE == px.a)
	{
		uint8_t r8 = ((px.r * 0xFF) / 0x1F) & 0xFF;
		uint8_t g8 = ((px.g * 0xFF) / 0x1F) & 0xFF;
		uint8_t b8 = ((px.b * 0xFF) / 0x1F) & 0xFF;
		bitmapDisplay[(bitmapWidth * y) + x] = (0xFF << 24) | (r8 << 16) | (g8 << 8) | (b8);
	}
}

/**
 * @brief TODO
 * 
 * @param x 
 * @param y 
 * @return rgba_pixel_t 
 */
rgba_pixel_t emuGetPxTft(int16_t x, int16_t y)
{
	uint32_t argb = bitmapDisplay[(bitmapWidth * y) + x];
	rgba_pixel_t px;
	px.r = (((argb & 0xFF0000) >> 16) * 0x1F) / 0xFF; // 5 bit
	px.g = (((argb & 0x00FF00) >>  8) * 0x1F) / 0xFF; // 5 bit
	px.b = (((argb & 0x0000FF) >>  0) * 0x1F) / 0xFF; // 5 bit
	return px;
}

/**
 * @brief TODO
 * 
 */
void emuClearPxTft(void)
{
	memset(bitmapDisplay, 0x00, sizeof(uint32_t) * bitmapWidth * bitmapHeight);
}

/**
 * @brief TODO
 * 
 * Note, this is called from a pthread
 * 
 * @param drawDiff 
 */
void emuDrawDisplayTft(bool drawDiff)
{
	/* Rawdraw is initialized on the main thread, so the draw calls must come 
	 * from there too. Raise a flag to do so
	 */
	shouldDrawTft = true;
}

/**
 * @brief TODO
 * 
 * @param x 
 * @param y 
 * @param px 
 */
void emuSetPxOled(int16_t x, int16_t y, rgba_pixel_t px)
{
	WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param x 
 * @param y 
 * @return rgba_pixel_t 
 */
rgba_pixel_t emuGetPxOled(int16_t x, int16_t y)
{
	WARN_UNIMPLEMENTED();
	rgba_pixel_t px = {.r=0x00, .g = 0x00, .b = 0x00, .a = PX_OPAQUE};
	return px;
}

/**
 * @brief TODO
 * 
 */
void emuClearPxOled(void)
{
	WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 * 
 * @param drawDiff 
 */
void emuDrawDisplayOled(bool drawDiff)
{
	WARN_UNIMPLEMENTED();
}

/**
 * @brief When the main task yields, rest
 * 
 */
void onTaskYield(void)
{
	// Just sleep for a ms
	usleep(1000);
}
