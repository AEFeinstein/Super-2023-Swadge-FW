#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "swadgeMode.h"
#include "hdw-led/led_util.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_wdt.h"
#include "soc/soc.h"
#include "soc/system_reg.h"

#include "meleeMenu.h"
#include "mode_main_menu.h"

int global_i = 100;
display_t * disp;


//#define REBOOT_TEST
//#define PROFILE_TEST
//#define MENU_TEST
#define PROFILE_GRAPHICS

#ifdef MENU_TEST
const char * menu_MainMenu = "Main Menu";
const char * menu_Bootload = "Bootloader";
meleeMenu_t * menu;
font_t meleeMenuFont;
#endif

#ifdef PROFILE_GRAPHICS
wsg_t example_sprite;
extern const int16_t sin1024[];
extern const uint16_t tan1024[];


#endif

// Example to do true inline assembly.  This will actually compile down to be
// included in the code, itself, and "should" (does in all the tests I've run)
// execute in one clock cycle since there is no function call and rsr only
// takes one cycle to complete. 
static inline uint32_t get_ccount()
{
	uint32_t ccount;
	asm volatile("rsr %0,ccount":"=a" (ccount));
	return ccount;
}

// External functions defined in .S file for you assembly people.
void minimal_function();
uint32_t test_function( uint32_t x );
uint32_t asm_read_gpio();

#ifdef MENU_TEST
void menuCb(const char* opt)
{
	if( opt == menu_MainMenu )
	{
		switchToSwadgeMode( &modeMainMenu );
	}
	else if( opt == menu_Bootload )
	{
		// Uncomment this to reboot the chip into the bootloader.
		// This is to test to make sure we can call ROM functions.
		REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
		void software_reset( uint32_t x );
		software_reset( 0 );
	}
}
#endif

void sandbox_main(display_t * disp_in)
{
	disp = disp_in;
	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );

	REG_WRITE( GPIO_FUNC7_OUT_SEL_CFG_REG,4 ); // select ledc_ls_sig_out0

#ifdef MENU_TEST
	loadFont("mm.font", &meleeMenuFont);
	menu = initMeleeMenu("USB Sandbox", &meleeMenuFont, menuCb);
	addRowToMeleeMenu(menu, menu_MainMenu);
	addRowToMeleeMenu(menu, menu_Bootload);
#endif

#ifdef PROFILE_GRAPHICS
	loadWsg("sprite005.wsg", &example_sprite);
#endif
	ESP_LOGI( "sandbox", "Loaded" );
}

void sandbox_exit()
{
	ESP_LOGI( "sandbox", "Exit" );
#ifdef MENU_TEST
	if( menu )
	{
		deinitMeleeMenu(menu);
		freeFont(&meleeMenuFont);
	}
#endif
	ESP_LOGI( "sandbox", "Exit" );
}

int gx, gy;

#ifdef PROFILE_GRAPHICS
uint8_t marker[10*10];
uint8_t marker2d[10][10];
#endif

#define CLAMP(x,l,u) ((x) < l ? l : ((x) > u ? u : (x)))

void fillDisplayAreaIRAM(display_t* disp, int16_t x1, int16_t y1, int16_t x2,
                     int16_t y2, paletteColor_t c)
{
    // Only draw on the display
    int16_t xMin = CLAMP(x1, 0, disp->w);
    int16_t xMax = CLAMP(x2, 0, disp->w);
    int16_t yMin = CLAMP(y1, 0, disp->h);
    int16_t yMax = CLAMP(y2, 0, disp->h);

    // Set each pixel
    for (int y = yMin; y < yMax; y++)
    {
        for (int x = xMin; x < xMax; x++)
        {
            SET_PIXEL(disp, x, y, c);
        }
    }
}

void fillDisplayAreaFast(display_t* disp, int16_t x1, int16_t y1, int16_t x2,
                     int16_t y2, paletteColor_t c)
{
	// Note: int16_t vs int data types tested for speed.
	
    // Only draw on the display
    int xMin = CLAMP(x1, 0, disp->w);
    int xMax = CLAMP(x2, 0, disp->w);
    int yMin = CLAMP(y1, 0, disp->h);
    int yMax = CLAMP(y2, 0, disp->h);

	paletteColor_t * pxs = disp->pxFb[0];

	int copyLen = xMax - xMin;

    // Set each pixel
    for (int y = yMin; y < yMax; y++)
    {
		uint8_t * line = pxs + y * disp->w + xMin;
		memset( line, c, copyLen );
    }
}

void drawWsgTileLocal(display_t* disp, wsg_t* wsg, int32_t xOff, int32_t yOff)
{
    // Check if there is framebuffer access
    if(NULL != disp->pxFb)
    {
        if(xOff > disp->w)
        {
            return;
        }

        // Bound in the Y direction
        int32_t yStart = (yOff < 0) ? 0 : yOff;
        int32_t yEnd   = ((yOff + wsg->h) > disp->h) ? disp->h : (yOff + wsg->h);

        // Bound in the X direction
        int32_t copyLen = wsg->w;
        if(xOff < 0)
        {
            copyLen += xOff;
            xOff = 0;
        }

        if(xOff + copyLen > disp->w)
        {
            copyLen = disp->w - xOff;
        }

        // copy each row
        for(int32_t y = yStart; y < yEnd; y++)
        {
            // Copy the row
            // TODO probably faster if we can guarantee copyLen is a multiple of 4
            memcpy(&disp->pxFb[y][xOff], &wsg->px[(wsg->h - (yEnd - y)) * wsg->w], copyLen);
        }
    }
    else
    {
        // No framebuffer access, draw the WSG simply
        drawWsgSimple(disp, wsg, xOff, yOff);
    }
}


void drawWsgSimpleLocal(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff)
{
    if(NULL == wsg->px)
    {
        return;
    }

    // Only draw in bounds
    int16_t xMin = CLAMP(xOff, 0, disp->w);
    int16_t xMax = CLAMP(xOff + wsg->w, 0, disp->w);
    int16_t yMin = CLAMP(yOff, 0, disp->h);
    int16_t yMax = CLAMP(yOff + wsg->h, 0, disp->h);

    // Draw each pixel
    for (int y = yMin; y < yMax; y++)
    {
        for (int x = xMin; x < xMax; x++)
        {
            int16_t wsgX = x - xOff;
            int16_t wsgY = y - yOff;
            int16_t wsgIdx = (wsgY * wsg->w) + wsgX;
            if (cTransparent != wsg->px[wsgIdx])
            {
                SET_PIXEL(disp, x, y, wsg->px[wsgIdx]);
            }
        }
    }
}

void drawWsgSimpleFast(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff)
{
    if(NULL == wsg->px)
    {
        return;
    }

    // Only draw in bounds
    int xMin = CLAMP(xOff, 0, disp->w);
    int xMax = CLAMP(xOff + wsg->w, 0, disp->w);
    int yMin = CLAMP(yOff, 0, disp->h);
    int yMax = CLAMP(yOff + wsg->h, 0, disp->h);
	paletteColor_t * px = disp->pxFb[0];
	int xpx = xMax - xMin;
	
    // Draw each pixel
    for (int y = yMin; y < yMax; y++)
    {
		int wsgY = (y - yOff);
		int wsgX = (xMin - xOff);
		paletteColor_t * lineout = &px[(y * disp->w) + xMin];
		paletteColor_t * linein = &wsg->px[wsgY * wsg->w + wsgX];
		
        for (int x = 0; x < xpx; x++)
        {
			int px = linein[x];
			if( px != cTransparent )
			{
				lineout[x] = px;
			}
        }
    }
}

void transformPixelLocal(int16_t* x, int16_t* y, int16_t transX,
                    int16_t transY, bool flipLR, bool flipUD,
                    int16_t rotateDeg, int16_t width, int16_t height)
{
    // First rotate the sprite around the sprite's center point
    if (0 < rotateDeg && rotateDeg < 360)
    {
        // This solves the aliasing problem, but because of tan() it's only safe
        // to rotate by 0 to 90 degrees. So rotate by a multiple of 90 degrees
        // first, which doesn't need trig, then rotate the rest with shears
        // See http://datagenetics.com/blog/august32013/index.html
        // See https://graphicsinterface.org/wp-content/uploads/gi1986-15.pdf

        // Center around (0, 0)
        (*x) -= (width / 2);
        (*y) -= (height / 2);

        // First rotate to the nearest 90 degree boundary, which is trivial
        if(rotateDeg >= 270)
        {
            // (x, y) -> (y, -x)
            int16_t tmp = (*x);
            (*x) = (*y);
            (*y) = -tmp;
        }
        else if(rotateDeg >= 180)
        {
            // (x, y) -> (-x, -y)
            (*x) = -(*x);
            (*y) = -(*y);
        }
        else if(rotateDeg >= 90)
        {
            // (x, y) -> (-y, x)
            int16_t tmp = (*x);
            (*x) = -(*y);
            (*y) = tmp;
        }
        // Now that it's rotated to a 90 degree boundary, find out how much more
        // there is to rotate by shearing
        rotateDeg = rotateDeg % 90;

        // If there's any more to rotate, apply three shear matrices in order
        // if(rotateDeg > 1 && rotateDeg < 89)
        if(rotateDeg > 0)
        {
            // 1st shear
            (*x) = (*x) - (((*y) * tan1024[rotateDeg / 2]) + 512) / 1024;
            // 2nd shear
            (*y) = (((*x) * sin1024[rotateDeg]) + 512) / 1024 + (*y);
            // 3rd shear
            (*x) = (*x) - (((*y) * tan1024[rotateDeg / 2]) + 512) / 1024;
        }

        // Return pixel to original position
        (*x) = (*x) + (width / 2);
        (*y) = (*y) + (height / 2);
    }

    // Then reflect over Y axis
    if (flipLR)
    {
        (*x) = width - 1 - (*x);
    }

    // Then reflect over X axis
    if(flipUD)
    {
        (*y) = height - 1 - (*y);
    }

    // Then translate
    (*x) += transX;
    (*y) += transY;
}

void drawWsgLocal(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff,
             bool flipLR, bool flipUD, int16_t rotateDeg)
{
    if(NULL == wsg->px)
    {
        return;
    }

    // Draw the image's pixels
    for(int16_t srcY = 0; srcY < wsg->h; srcY++)
    {
        for(int16_t srcX = 0; srcX < wsg->w; srcX++)
        {
            // Draw if not transparent
            uint16_t srcIdx = (srcY * wsg->w) + srcX;
            if (cTransparent != wsg->px[srcIdx])
            {
                // Transform this pixel's draw location as necessary
                int16_t dstX = srcX;
                int16_t dstY = srcY;
                if(flipLR || flipUD || !rotateDeg)
                {
                    transformPixelLocal(&dstX, &dstY, xOff, yOff, flipLR, flipUD,
                                   rotateDeg, wsg->w, wsg->h);
                }
                // Check bounds
                if(0 <= dstX && dstX < disp->w &&
                        0 <= dstY && dstY < disp->h)
                {
                    // Draw the pixel
                    SET_PIXEL(disp, dstX, dstY, wsg->px[srcIdx]);
                }
            }
        }
    }
}





void transformPixelLocalFast(int16_t* x, int16_t* y,
                    int16_t rotateDeg, int16_t width, int16_t height)
{
	int wx = *x;
	int wy = *y;
    // First rotate the sprite around the sprite's center point

	// This solves the aliasing problem, but because of tan() it's only safe
	// to rotate by 0 to 90 degrees. So rotate by a multiple of 90 degrees
	// first, which doesn't need trig, then rotate the rest with shears
	// See http://datagenetics.com/blog/august32013/index.html
	// See https://graphicsinterface.org/wp-content/uploads/gi1986-15.pdf

	// Center around (0, 0)
	wx -= (width / 2);
	wy -= (height / 2);

	// First rotate to the nearest 90 degree boundary, which is trivial
	if(rotateDeg >= 270)
	{
		// (x, y) -> (y, -x)
		int16_t tmp = wx;
		wx = wy;
		wy = -tmp;
		rotateDeg -= 270;
	}
	else if(rotateDeg >= 180)
	{
		// (x, y) -> (-x, -y)
		wx = -wx;
		wy = -wy;
		rotateDeg -= 180;
	}
	else if(rotateDeg >= 90)
	{
		// (x, y) -> (-y, x)
		int16_t tmp = wx;
		wx = -wy;
		wy = tmp;
		rotateDeg -= 90;
	}
	// Now that it's rotated to a 90 degree boundary, find out how much more
	// there is to rotate by shearing

	// If there's any more to rotate, apply three shear matrices in order
	// if(rotateDeg > 1 && rotateDeg < 89)

	if(rotateDeg > 0)
	{
		// 1st shear
		wx = wx - ((wy * tan1024[rotateDeg / 2]) + 512) / 1024;
		// 2nd shear
		wy = ((wx * sin1024[rotateDeg]) + 512) / 1024 + wy;
		// 3rd shear
		wx = wx - ((wy * tan1024[rotateDeg / 2]) + 512) / 1024;
	}

	// Return pixel to original position
	wx += (width / 2);
	wy += (height / 2);

	*x = wx;
	*y = wy;
}


void drawWsgLocalFast(display_t* disp, wsg_t* wsg, int16_t xOff, int16_t yOff,
             bool flipLR, bool flipUD, int16_t rotateDeg)
{
    if(NULL == wsg->px)
    {
        return;
    }

	uint32_t w = disp->w;
	uint32_t pxmax = w * disp->h;
	paletteColor_t * px = disp->pxFb[0];
	
	if(rotateDeg)
	{
		uint32_t wsgw = wsg->w;
		uint32_t wsgh = wsg->h;
		for(int16_t srcY = 0; srcY < wsgh; srcY++)
		{
			int usey = srcY;
			
			// Reflect over X axis?
			if(flipUD)
			{
				usey = wsg->h - 1 - usey;
			}

			paletteColor_t * linein = &wsg->px[usey * wsgw];
			
			// Reflect over Y axis?
			int readX = 0;
			int advanceX = 1;
			if (flipLR)
			{
				readX = wsgw;
				advanceX = -1;
			}
			
			int16_t localX = 0;
			for(int16_t srcX = 0; srcX != wsgw; srcX++)
			{
				// Draw if not transparent
				uint8_t color = linein[readX];
				if (cTransparent != color)
				{
					int16_t tx = localX;
					int16_t ty = srcY;
					transformPixelLocalFast(&tx, &ty, rotateDeg, wsgw, wsgh );
					tx += xOff;
					ty += yOff;
					int offset = tx + ty * w;
					if( offset >= 0 && offset < pxmax )
					{
						px[offset] = color;
					}
				}
				localX++;
				readX += advanceX;
			}
		}		
	}
	else
	{
		// Draw the image's pixels
		uint16_t wsgw = wsg->w;
		uint16_t wsgh = wsg->h;
		for(int16_t srcY = 0; srcY < wsgh; srcY++)
		{
			int usey = srcY;
			
			// Reflect over X axis?
			if(flipUD)
			{
				usey = wsgh - 1 - usey;
			}

			paletteColor_t * linein = &wsg->px[usey * wsgw];

			int xstart = 0;
			int xend = wsgw;
			int xinc = 1;
			
			// Reflect over Y axis?
			if (flipLR)
			{
				xstart = wsgw-1;
				xend = -1;
				xinc = -1;
			}

			// Transform this pixel's draw location as necessary
			int16_t dstY = srcY + yOff;
			int dstx = xOff;
			int lineOffset = dstY * w;
			
			for(int16_t srcX = xstart; srcX != xend; srcX+=xinc)
			{
				// Draw if not transparent
				uint8_t color = linein[srcX];
				if (cTransparent != color)
				{
					int32_t pxoffset = dstx + lineOffset;
					if( pxoffset >= 0 && pxoffset < pxmax )
					{
						px[pxoffset] = color;
					}
				}
				dstx++;
			}
		}
	}
}


void sandbox_tick()
{
#ifdef PROFILE_GRAPHICS
	static int frame;
	frame++;
	uint32_t start, end;
	int x = gx;
	int y = gy;
	uint8_t ** pxarray = disp->pxFb;
	uint8_t * px = pxarray[0];
	uint16_t w = 240;
	
	fillDisplayAreaFast(disp, 0, 0, disp->w, disp->h, 0);
	
	register uint32_t temp = 0;
	register uint32_t temp2 = 0;
	register uint32_t width = 280;
	register uint32_t val = 5;
	portDISABLE_INTERRUPTS();
	asm volatile( "nop" : [temp]"+a"(temp),[temp2]"+a"(temp2) : [x]"a"(x),[y]"a"(y),[px]"g"(px),[val]"a"(val),[width]"a"(width) :  ); // 6/6 cycles	
	start = get_ccount();
	asm volatile( "nop" );
	// Without mul16 (for some reason compiler is dumb?)
	//pxarray[y][x] = 5; // 8/6 cycles
	//px[y+x*240] = 5; // 7/6 cycles
	//px[y+x*280] = 5; // 9/8 cycles
	//px[y+x*256] = 5; // 6/4 cycles
	//asm volatile( "mul16u %[temp2], %[width], %[y]\nadd %[temp, %[px], %[x]\nadd %[temp], %[temp2], %[px]\ns8i %[val],%[temp], 0" : [temp]"+a"(temp),[temp2]"+a"(temp2) : [x]"a"(x),[y]"a"(y),[px]"g"(px),[val]"a"(val),[width]"a"(width) : ); // 5/4 cycles
	//marker[x+y*10] = 5; //13/9
	//marker2d[y][x] = 5; //13/9
	
	//fillDisplayArea(disp, 0, 0, disp->w, disp->h, 1); // 408537 cycles from flash, when optimized, and IRAM, 407576.
	//fillDisplayAreaIRAM(disp, 0, 0, disp->w, disp->h, 1); // Before optimizations, 408290
	//fillDisplayAreaFast(disp, 0, 0, disp->w, disp->h, 20); // Before optimizations, 48250 cycles (-O3), 541739 (-Os)

	//drawWsgTile(disp, &example_sprite, 0, 0 );	// 1576-2576 (Flash)
	//drawWsgTileLocal(disp, &example_sprite, 0, 0 ); // Always 1029 (IRAM)

	//drawWsgSimple(disp, &example_sprite, 0, 0 );	// 5260-6260 (Flash)
	//drawWsgSimpleLocal(disp, &example_sprite, 0, 0 );	// 5093 (IRAM)
	//drawWsgSimpleFast(disp, &example_sprite, 93, 91 );	// 3632 (IRAM)

	//drawWsg(disp, &example_sprite, 100, 100, 0, 0, 0 );// (Flash) 18322 bool flipLR, bool flipUD, int16_t rotateDeg)
	//drawWsgLocal(disp, &example_sprite, 100, 100, 0, 0, 0 );// (IRAM) 17833 bool flipLR, bool flipUD, int16_t rotateDeg)
	//drawWsgLocalFast(disp, &example_sprite, 100, 100, 0, 0, 0 );// (IRAM) 17417 bool flipLR, bool flipUD, int16_t rotateDeg)
	// General notes:
	//  * 17833->17417 by not using dereferenced pointers in transformPixelLocal
	//  * 17417->15744 by computing pixel offset, and guarding based on that.  And only reading color once.
	//  * 15744->6052 by moving flip outside of the function call.
	//  * 6052->10122 by checking the rotation inside the inner loop.
	//  * 6052->5945 by "caching" parameters relating to wsg as local variables.
	//  * 5945->4929 by avoiding the 2D array lookup when getting the wsg pixel data by computing it per line.
	//    -> Also with the current implementation, flipped X/Y is slightly faster for some reason?
	//  * 4929->4055 by separately running destination and source X/Y
	//  * 4055->4022 by exporting the Y coord.
	
	// Now to fix rotation.
	// Turns out that was pretty simple.
	drawWsgLocalFast(disp, &example_sprite, 100, 100, 0, 1, frame%360 );// (IRAM) 17417 bool flipLR, bool flipUD, int16_t rotateDeg)
	//drawWsg(disp, &example_sprite, 100, 100, 0, 0, frame%360 );// (IRAM) 17417 bool flipLR, bool flipUD, int16_t rotateDeg)

	asm volatile( "nop" );
	end = get_ccount()-3;
	portENABLE_INTERRUPTS();
	
	ESP_LOGI( "sandbox", "cycles: %d", end-start );
	
#elif defined( PROFILE_TEST )
	volatile uint32_t profiles[7];  // Use of volatile here to force compiler to order instructions and not cheat.
	uint32_t start, end;

	// Profile function call into assembly land
	// Mostly used to understand function call overhead.
	start = get_ccount();
	minimal_function();
	end = get_ccount();
	profiles[0] = end-start-1;

	// Profile a nop (Should be 1, because profiling takes 1 cycle)
	start = get_ccount();
	asm volatile( "nop" );
	end = get_ccount();
	profiles[1] = end-start-1;

	// Profile reading a register (will be slow)
	start = get_ccount();
	READ_PERI_REG( GPIO_ENABLE_W1TS_REG );
	end = get_ccount();
	profiles[2] = end-start-1;

	// Profile writing a regsiter (will be fast)
	// The ESP32-S2 can "write" to memory and keep executing
	start = get_ccount();
	WRITE_PERI_REG( GPIO_ENABLE_W1TS_REG, 0 );
	end = get_ccount();
	profiles[3] = end-start-1;

	// Profile subsequent writes (will be slow)
	// The ESP32-S2 can only write once in a buffered write.
	start = get_ccount();
	WRITE_PERI_REG( GPIO_ENABLE_W1TS_REG, 0 );
	WRITE_PERI_REG( GPIO_ENABLE_W1TS_REG, 0 );
	end = get_ccount();
	profiles[4] = end-start-1;

	// Profile a more interesting assembly instruction
	start = get_ccount();
	uint32_t tfret = test_function( 0xaaaa );
	end = get_ccount();
	profiles[5] = end-start-1;

	// Profile a more interesting assembly instruction
	start = get_ccount();
	uint32_t tfret2 = asm_read_gpio( );
	end = get_ccount();
	profiles[6] = end-start-1;

	vTaskDelay(1);

	ESP_LOGI( "sandbox", "global_i: %d %d %d %d %d %d %d clock cycles; tf ret: %08x / %08x", profiles[0], profiles[1], profiles[2], profiles[3], profiles[4], profiles[5], profiles[6], tfret, tfret2 );
#else
	ESP_LOGI( "sandbox", "global_i: %d", global_i++ );
#endif
#ifdef MENU_TEST
	if( menu )
	    drawMeleeMenu(disp, menu);
#endif
}

void sandbox_button(buttonEvt_t* evt)
{
    if(evt->down)
    {
        switch (evt->button)
        {
            case UP:
            case DOWN:
            case LEFT:
            case RIGHT:
            case BTN_A:
            {
#ifdef MENU_TEST
                meleeMenuButton(menu, evt->button);
                break;
#endif
            }
            case START:
            case SELECT:
            case BTN_B:
            default:
            {
                break;
            }
        }
    }
}

swadgeMode sandbox_mode =
{
    .modeName = "sandbox",
    .fnEnterMode = sandbox_main,
    .fnExitMode = sandbox_exit,
    .fnMainLoop = sandbox_tick,
    .fnButtonCallback = sandbox_button,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};

