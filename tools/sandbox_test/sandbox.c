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
meleeMenu_t * menu;
font_t meleeMenuFont;
display_t * disp;

const char * menu_MainMenu = "Main Menu";
const char * menu_Bootload = "Bootloader";

//#define REBOOT_TEST
//#define PROFILE_TEST
#define MODE7_TEST

#ifdef MODE7_TEST
int mode7timing;
#endif

// External functions defined in .S file for you assembly people.
void minimal_function();
uint32_t test_function( uint32_t x );
uint32_t asm_read_gpio();

wsg_t example_sprite;

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

void sandbox_main(display_t * disp_in)
{
#ifdef REBOOT_TEST
	// Uncomment this to reboot the chip into the bootloader.
	// This is to test to make sure we can call ROM functions.
	REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
	void software_reset( uint32_t x );
	software_reset( 0 );
#endif

	menu = 0; disp = disp_in;
	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );

	REG_WRITE( GPIO_FUNC7_OUT_SEL_CFG_REG,4 ); // select ledc_ls_sig_out0

	loadFont("mm.font", &meleeMenuFont);
	menu = initMeleeMenu("USB Sandbox", &meleeMenuFont, menuCb);
	addRowToMeleeMenu(menu, menu_MainMenu);
	addRowToMeleeMenu(menu, menu_Bootload);

	loadWsg("sprite005.wsg", &example_sprite);

	ESP_LOGI( "sandbox", "Loaded" );
}

void sandbox_exit()
{
	ESP_LOGI( "sandbox", "Exit" );
	if( menu )
	{
		deinitMeleeMenu(menu);
		freeFont(&meleeMenuFont);
	}
	if( example_sprite.px )
	{
		freeWsg( &example_sprite ); 
	}
	ESP_LOGI( "sandbox", "Exit" );
}

void sandbox_tick()
{
#ifdef PROFILE_TEST
	volatile uint32_t profiles[7];  // Use of volatile here to force compiler to order instructions and not cheat.
	uint32_t start, end;

	// Profile function call into assembly land
	// Mostly used to understand function call overhead.
	start = getCycleCount();
	minimal_function();
	end = getCycleCount();
	profiles[0] = end-start-1;

	// Profile a nop (Should be 1, because profiling takes 1 cycle)
	start = getCycleCount();
	asm volatile( "nop" );
	end = getCycleCount();
	profiles[1] = end-start-1;

	// Profile reading a register (will be slow)
	start = getCycleCount();
	READ_PERI_REG( GPIO_ENABLE_W1TS_REG );
	end = getCycleCount();
	profiles[2] = end-start-1;

	// Profile writing a regsiter (will be fast)
	// The ESP32-S2 can "write" to memory and keep executing
	start = getCycleCount();
	WRITE_PERI_REG( GPIO_ENABLE_W1TS_REG, 0 );
	end = getCycleCount();
	profiles[3] = end-start-1;

	// Profile subsequent writes (will be slow)
	// The ESP32-S2 can only write once in a buffered write.
	start = getCycleCount();
	WRITE_PERI_REG( GPIO_ENABLE_W1TS_REG, 0 );
	WRITE_PERI_REG( GPIO_ENABLE_W1TS_REG, 0 );
	end = getCycleCount();
	profiles[4] = end-start-1;

	// Profile a more interesting assembly instruction
	start = getCycleCount();
	uint32_t tfret = test_function( 0xaaaa );
	end = getCycleCount();
	profiles[5] = end-start-1;

	// Profile a more interesting assembly instruction
	start = getCycleCount();
	uint32_t tfret2 = asm_read_gpio( );
	end = getCycleCount();
	profiles[6] = end-start-1;

	vTaskDelay(1);

	ESP_LOGI( "sandbox", "global_i: %d %d %d %d %d %d %d clock cycles; tf ret: %08x / %08x", profiles[0], profiles[1], profiles[2], profiles[3], profiles[4], profiles[5], profiles[6], tfret, tfret2 );
#else
	ESP_LOGI( "sandbox", "global_i: %d", global_i++ );
#endif

#ifndef MODE7_TEST
	if( menu )
	    drawMeleeMenu(disp, menu);
#endif

	uint32_t start = getCycleCount();
	drawWsg( disp, &example_sprite, 100, 100, 0, 0, 0); // 19600-20400
	uint32_t end = getCycleCount();

#ifdef MODE7_TEST
	for( int mode = 0; mode < 8; mode++ )
	{
		drawWsg( disp, &example_sprite, 50+mode*20, (global_i%20)-10, !!(mode&1), !!(mode & 2), (mode & 4)*10);
		drawWsg( disp, &example_sprite, 50+mode*20, (global_i%20)+230, !!(mode&1), !!(mode & 2), (mode & 4)*10);
		drawWsg( disp, &example_sprite, (global_i%20)-10, 50+mode*20, !!(mode&1), !!(mode & 2), (mode & 4)*10);
		drawWsg( disp, &example_sprite, (global_i%20)+270, 50+mode*20, !!(mode&1), !!(mode & 2), (mode & 4)*10);
	}

	ESP_LOGI( "sandbox", "SPROF: %d / Mode7: %d", end-start, mode7timing );
#endif
}

void sandboxBackgroundDrawCallback(display_t* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum )
{
#ifdef MODE7_TEST
	int i;

	uint32_t start = getCycleCount();
	fillDisplayArea(disp, x, y, x+w, y+h, 0 );
	for( i = 0; i < 16; i++ )
		fillDisplayArea(disp, i*16+8, y, i*16+16+8, y+16, up*16+i );
	mode7timing = getCycleCount() - start;
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
                meleeMenuButton(menu, evt->button);
                break;
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
	.fnBackgroundDrawCallback = sandboxBackgroundDrawCallback,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};
