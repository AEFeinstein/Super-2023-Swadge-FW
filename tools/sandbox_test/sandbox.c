#include <stdio.h>
#include "esp_system.h"
#include "swadgeMode.h"
#include "hdw-led/led_util.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_wdt.h"
#include "soc/soc.h"  // for WRITE_PERI_REG

int global_i = 100;

// Example to do true inline assembly.  This will actually compile down to be
// included in the code, itself. 
static inline uint32_t get_ccount()
{
	uint32_t ccount;
	__asm__ __volatile__("rsr %0,ccount":"=a" (ccount));
	return ccount;
}
void test_function();

void sandbox_main(display_t * disp)
{
	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );


#if REBOOT_TEST
	// Uncomment this to reboot the chip into the bootloader.
	// This is to test to make sure we can call ROM functions.
	REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
	void software_reset( uint32_t x );
	software_reset( 0 );
#endif
}

void sandbox_exit()
{
	ESP_LOGI( "sandbox", "Exit" );
}

void sandbox_tick()
{
	uint32_t start_time = get_ccount();
	test_function();
	uint32_t end_time = get_ccount();
	uint32_t diff = end_time - start_time;
	ESP_LOGI( "sandbox", "global_i: %d %d", global_i++, diff );
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

