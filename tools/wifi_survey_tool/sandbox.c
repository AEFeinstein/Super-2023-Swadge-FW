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
#include "esp_wifi.h"
#include "esp_private/wifi.h"
#include "swadge_util.h"
#include "meleeMenu.h"
#include "mode_main_menu.h"
#include "hdw-esp-now/espNowUtils.h"

#include "nvs_flash.h"
#include "nvs.h"

#define FSNET_CODE_SERVER 0x73534653
#define FSNET_CODE_PEER 0x66534653

meleeMenu_t * menu;
font_t meleeMenuFont;
display_t * disp;

// External functions defined in .S file for you assembly people.
void minimal_function();
uint32_t asm_read_gpio();

uint8_t thistest = 0;
int16_t prx = 0;
int16_t pigi = 0;
int8_t tas = 0;
int8_t tss = 0;
int16_t tstimes[256];
int is_host = 0;
#define TEST_LEN 100
#define NUM_TESTS 100

int trx = 0;
int this_test = -1;
int current_tx_igi;
int this_rx = 0;
int lastrxa = 0;
int lastrxtest = 0;
int8_t lasttest = 0;
int gotrx = 0;
int lasta;

int oktosend = 0;
int slave_clear_to_send = 1;
void menuCb(const char* opt)
{
}

int advanced_usb_write_log_printf(const char *fmt, va_list args);
int uprintf( const char * fmt, ... )
{
	va_list args;
	va_start(args, fmt);
	int r = advanced_usb_write_log_printf(fmt, args);
	va_end(args);
	return r;
}



void sandboxRxESPNow(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi)
{
	prx++;
	pigi += -rssi;
	if( len < 5 ) return;

	trx++;
	lasta = data[0];
	if( data[0] == 0xaa )
	{
		if( data[1] == this_test )
		{
			this_rx++;
		}
		else
		{
			if( !is_host )
			{
				// Change IGI
				const int igi_reduction = data[1];
				volatile uint32_t * test = (uint32_t*)0x6001c02c; //Should be the "RSSI Offset" but seems to do more.
				*test = (*test & 0xffffff00) + igi_reduction; 

				// Make sure the first takes effect.
				//vTaskDelay( 1 );

				//No idea  Somehow applies setting of 0x6001c02c  (Ok, actually I don't know what the right-most value should be but 0xff in the MSB seems to work?
				test = (uint32_t*)0x6001c0a0;
				*test = (*test & 0xffffff) | 0xff00000000; 
			}
			gotrx = this_rx;
			lasttest = this_test;
			this_rx = 1;
			this_test = data[1];
		}
	}
	if( data[0] == 0xbb && is_host )
	{
		lastrxa = data[1];
		lastrxtest = data[2];
	}

//	uprintf( "ESP-NOW<%d %d [%02x %02x %02x %02x %02x %02x] [%02x %02x %02x] \n", len, rssi, mac_addr[0],
//		mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], data[0], data[1], data[2] );
}


uint32_t stime;
void sandboxTxESPNow(const uint8_t* mac_addr, esp_now_send_status_t status)
{
	//uprintf( "TASS %d %d\n", tas, status );
	if( is_host )
	{
		if( tas < NUM_TESTS )
		{
			oktosend = 1;
		}

		if( status == 0 && tss < 255 )
		{
			uint32_t end = esp_timer_get_time();
			tstimes[tss] = (end-stime)/100; // in tenths of milliseconds.
			tss++;
		}
	}
	else
	{
		slave_clear_to_send = 1;
	}
//	uprintf( "ESP-NOW>%d %d\n", status, end-stime );
}

void sandbox_main(display_t * disp_in)
{
	menu = 0; disp = disp_in;

	esp_log_level_set( "*", ESP_LOG_VERBOSE ); // Enable logging if there's any way.

	loadFont("mm.font", &meleeMenuFont);
	menu = initMeleeMenu("IGI Test", &meleeMenuFont, menuCb);

	prx = 0;
	pigi = 0;
	tas = 0;
	tss = 0;
	slave_clear_to_send = 1;


	uprintf( "Installing espnow.\n" );

    espNowInit(&sandboxRxESPNow, &sandboxTxESPNow, GPIO_NUM_NC,
		GPIO_NUM_NC, UART_NUM_MAX, ESP_NOW_IMMEDIATE);


	uprintf( "Loaded\n" );
}

void sandbox_exit()
{
	uprintf( "Exit\n" );
	if( menu )
	{
		deinitMeleeMenu(menu);
		freeFont(&meleeMenuFont);
	}
	uprintf( "Exit Complete\n" );
}

void sandbox_tick()
{
	//uprintf( "%d\n", oktosend );
	if( oktosend > 1 ) oktosend--;
	else if( oktosend == 1 )
	{
		oktosend = 0;
		uint8_t buff[TEST_LEN] = { 0 };
		buff[0] = 0xaa;
		buff[1] = current_tx_igi;
		buff[2] = tas;
		stime = esp_timer_get_time();
		tas++;
		espNowSend((char*)buff, TEST_LEN);
	}

	int i, j;
	if( menu )
	    drawMeleeMenu(disp, menu);

	char sts[80];

	if( !is_host )
	{
		static int waitframe = 0;
		waitframe++;
		if( (waitframe & 0x7) == 0 && slave_clear_to_send )
		{
			uint8_t buff[40] = { 0 };
			buff[0] = 0xbb;
			buff[1] = gotrx;
			buff[2] = lasttest;
			slave_clear_to_send = 0;
			espNowSend((char*)buff, 10);	
			sprintf( sts, "%d", lasttest );
			drawText( disp, &meleeMenuFont, 215, "x", 200, 90);
		}
	}

	sprintf( sts, "TOT RX: %d", trx );
	drawText( disp, &meleeMenuFont, 215, sts, 40, 60);
	sprintf( sts, "IGT: %d %d", lasttest, current_tx_igi );
	drawText( disp, &meleeMenuFont, 215, sts, 40, 90);
	sprintf( sts, "RX PCT: %d", gotrx );
	drawText( disp, &meleeMenuFont, 215, sts, 40, 120);
	sprintf( sts, "LAST: %d", lasta );
	drawText( disp, &meleeMenuFont, 215, sts, 40, 150);
	sprintf( sts, "THIS RX: %d", this_rx );
	drawText( disp, &meleeMenuFont, 215, sts, 40, 180);



	

/*

		int len = pack - buff;
		uprintf( "len: %d\n", len );
		now = esp_timer_get_time();
		espNowSend((char*)buff, len); //Don't enable yet.
		stime = now;
	}
*/
}
int ict;

void sandboxBackgroundDrawCallback(display_t* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum )
{
}


int16_t sandboxAdvancedUSB(uint8_t * buffer, uint16_t length, uint8_t isGet )
{
	if( isGet )
	{
		memset( buffer, 0, length );
		buffer[1] = 0xaa;
		memcpy( buffer+2, &prx, 2 );
		memcpy( buffer+4, &pigi, 2 );
		memcpy( buffer+6, &tas, 1 );
		memcpy( buffer+7, &tss, 1 );
		buffer[8] = lastrxa;
		buffer[9] = lastrxtest;
		memcpy( buffer+10, tstimes, NUM_TESTS * 2 );

		return 255;
	}
	else
	{
		if( buffer[1] == 5 )
		{
			is_host = 1;

			// Change IGI
		    const int igi_reduction = buffer[2];
		    volatile uint32_t * test = (uint32_t*)0x6001c02c; //Should be the "RSSI Offset" but seems to do more.
		    *test = (*test & 0xffffff00) + igi_reduction; 

		    // Make sure the first takes effect.
		    vTaskDelay( 1 );

		    //No idea  Somehow applies setting of 0x6001c02c  (Ok, actually I don't know what the right-most value should be but 0xff in the MSB seems to work?
		    test = (uint32_t*)0x6001c0a0;
		    *test = (*test & 0xffffff) | 0xff00000000; 

			current_tx_igi = igi_reduction;

			prx = 0;
			pigi = 0;
			tas = 0;
			tss = 0;
			memset( tstimes, 0, sizeof(tstimes ) );

			oktosend = 1;


			uprintf( "IGI Set: %d\n", igi_reduction );
		}
		return length;
	}
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
    .modeName = "wifi_survey_tool",
    .fnEnterMode = sandbox_main,
    .fnExitMode = sandbox_exit,
    .fnMainLoop = sandbox_tick,
    .fnButtonCallback = sandbox_button,
	.fnBackgroundDrawCallback = sandboxBackgroundDrawCallback,
    .fnTouchCallback = NULL,
	.overrideUsb = false,
	.fnAdvancedUSB = sandboxAdvancedUSB,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = 0,
    .fnEspNowSendCb = 0,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};
