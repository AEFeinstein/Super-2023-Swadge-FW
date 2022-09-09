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
#include "esp_spi_flash.h"
#include "esp_partition.h"

#include "meleeMenu.h"
#include "mode_main_menu.h"

int global_i = 100;
font_t meleeMenuFont;
display_t * disp;
int mode7timing;
const uint32_t *map_ptr;
spi_flash_mmap_handle_t map_handle;

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

wsg_t example_sprite;

void sandbox_main(display_t * disp_in)
{
	disp = disp_in;
	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );
	loadFont("mm.font", &meleeMenuFont);
	loadWsg("sprite005.wsg", &example_sprite);


	map_handle = 0;
    const esp_partition_t * partition = esp_partition_find_first( ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "background_image" );
	if( !partition )
	{
		ESP_LOGE( "sandbox", "Error: Could not find background_image" );
		return;
	}
	ESP_ERROR_CHECK( esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, (void*)&map_ptr, &map_handle));


	ESP_LOGI( "sandbox", "Loaded" );
}

void sandbox_exit()
{
	ESP_LOGI( "sandbox", "Exit" );
	freeFont(&meleeMenuFont);
	if( example_sprite.px )
	{
		freeWsg( &example_sprite );
	}
	if( map_handle )
	{
		spi_flash_munmap( map_handle );
	}
	ESP_LOGI( "sandbox", "Exit" );
}

void sandbox_tick()
{
	uint32_t start = get_ccount();
	drawWsg( disp, &example_sprite, 100, 100, 0, 0, 0); // 19600-20400
	uint32_t end = get_ccount();

	for( int mode = 0; mode < 8; mode++ )
	{
		drawWsg( disp, &example_sprite, 50+mode*20, (global_i%20)-10, !!(mode&1), !!(mode & 2), (mode & 4)*10);
		drawWsg( disp, &example_sprite, 50+mode*20, (global_i%20)+230, !!(mode&1), !!(mode & 2), (mode & 4)*10);
		drawWsg( disp, &example_sprite, (global_i%20)-10, 50+mode*20, !!(mode&1), !!(mode & 2), (mode & 4)*10);
		drawWsg( disp, &example_sprite, (global_i%20)+270, 50+mode*20, !!(mode&1), !!(mode & 2), (mode & 4)*10);
	}

	ESP_LOGI( "sandbox", "SPROF: %d / Mode7: %d %08x", end-start, mode7timing, map_ptr[4] );
	
	global_i++;
}

uint8_t localram[128*128];

void sandboxBackgroundDrawCallback(display_t* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum )
{
	int i;
	uint32_t start = get_ccount();
	
	uint8_t * px = disp->pxFb + y * w;
	int lx, ly;
	for( ly = 0; ly < h; ly++ )
	for( lx = 0; lx < w; lx++ )
	{
		//*px = map_ptr[(((uint8_t*)map_ptr)[lx*13+ly])*17];
		//*px = map_ptr[(((uint8_t*)localram)[lx*45+ly])*7+y*9];
		*px = ((uint8_t*)map_ptr)[(lx*45237+ly*3893)&0x7fff];
		px++;
	}

	mode7timing = ( get_ccount() - start ) * 15;
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
