#include <stdio.h>
#include "esp_system.h"
#include "swadgeMode.h"
#include "hdw-led/led_util.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_wdt.h"
#include "soc/soc.h"

int global_i = 100;

//#define REBOOT_TEST
#define PROFILE_TEST

// Example to do true inline assembly.  This will actually compile down to be
// included in the code, itself, and "should" (does in all the tests I've run)
// execute in one clock cycle since there is no function call.
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


void sandbox_main(display_t * disp)
{
	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );


#ifdef REBOOT_TEST
	// Uncomment this to reboot the chip into the bootloader.
	// This is to test to make sure we can call ROM functions.
	REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
	void software_reset( uint32_t x );
	software_reset( 0 );
#endif

	ESP_LOGI( "sandbox", "Loaded" );
}

void sandbox_exit()
{
	ESP_LOGI( "sandbox", "Exit" );
}

void sandbox_tick()
{
#ifdef PROFILE_TEST
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




	ESP_LOGI( "sandbox", "global_i: %d %d %d %d %d %d %d clock cycles; tf ret: %08x / %08x", profiles[0], profiles[1], profiles[2], profiles[3], profiles[4], profiles[5], profiles[6], tfret, tfret2 );
#else
	ESP_LOGI( "sandbox", "global_i: %d", global_i++ );
#endif

}

swadgeMode sandbox_mode =
{
    .modeName = "sandbox",
    .fnEnterMode = sandbox_main,
    .fnExitMode = sandbox_exit,
    .fnMainLoop = sandbox_tick,
    .fnButtonCallback = NULL,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};

