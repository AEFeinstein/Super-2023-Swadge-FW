#include <stdio.h>
#include "esp_system.h"
#include "swadgeMode.h"
#include "hdw-led/led_util.h"
#include "hal/gpio_types.h"
#include "esp_log.h"

int global_i = 100;

esp_err_t esp_flash_read(void *chip, void *buffer, uint32_t address, uint32_t length);

void sandbox_main(display_t * disp)
{
	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );
}

void sandbox_exit()
{
	ESP_LOGI( "sandbox", "Exit" );
}

void sandbox_tick()
{
	uint32_t buffer[4] = { 0 };
	int r = esp_flash_read( 0, buffer, 0x3f0000, sizeof( buffer ) );

	ESP_LOGI( "sandbox", "global_i: %d", global_i++ );
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

