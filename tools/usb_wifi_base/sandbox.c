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


int global_i = 100;
meleeMenu_t * menu;
font_t meleeMenuFont;
display_t * disp;

const char * menu_MainMenu = "Main Menu";
const char * menu_Bootload = "Bootloader";

// External functions defined in .S file for you assembly people.
void minimal_function();
uint32_t asm_read_gpio();

void menuCb(const char* opt)
{
	if( opt == menu_MainMenu )
	{
		switchToSwadgeMode( &modeMainMenu );
	}
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
	uprintf( "ESP-NOW<%d %d [%02x %02x %02x %02x %02x %02x] [%02x %02x %02x] \n", len, rssi, mac_addr[0],
		mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], data[0], data[1], data[2] );
}

uint32_t stime;
void sandboxTxESPNow(const uint8_t* mac_addr, esp_now_send_status_t status)
{

	uint32_t end = esp_timer_get_time();
	uprintf( "ESP-NOW>%d %d\n", status, end-stime );
}

int dummy( uint32_t a, uint32_t b )
{
	uprintf( "DUMMY %08x %08x\n", a, b );
	return 0;
}


static uint32_t ReadUQ( uint32_t * rin, uint32_t bits )
{
    uint32_t ri = *rin;
    *rin = ri >> bits;
    return ri & ((1<<bits)-1);
}

static uint32_t PeekUQ( uint32_t * rin, uint32_t bits )
{
    uint32_t ri = *rin;
    return ri & ((1<<bits)-1);
}

static uint32_t ReadBitQ( uint32_t * rin )
{
    uint32_t ri = *rin;
    *rin = ri>>1;
    return ri & 1;
}

static uint32_t ReadUEQ( uint32_t * rin )
{
    if( !*rin ) return 0; //0 is invalid for reading Exp-Golomb Codes
    // Based on https://stackoverflow.com/a/11921312/2926815
    int32_t zeroes = 0;
    while( ReadBitQ( rin ) == 0 ) zeroes++;
    uint32_t ret = 1 << zeroes;
    for (int i = zeroes - 1; i >= 0; i--)
        ret |= ReadBitQ( rin ) << i;
    return ret - 1;
}

static int WriteUQ( uint32_t * v, uint32_t number, int bits )
{
    uint32_t newv = *v;
    newv <<= bits;
    *v = newv | number;
    return bits;
}

static int WriteUEQ( uint32_t * v, uint32_t number )
{
    int numvv = number+1;
    int gqbits = 0;
    while ( numvv >>= 1)
    {
        gqbits++;
    }
    *v <<= gqbits;
    return WriteUQ( v,number+1, gqbits+1 ) + gqbits;
}

static void FinalizeUEQ( uint32_t * v, int bits )
{
    uint32_t vv = *v;
    uint32_t temp = 0;
    for( ; bits != 0; bits-- )
    {
        int lb = vv & 1;
        vv>>=1;
        temp<<=1;
        temp |= lb;
    }
    *v = temp;
}



void sandbox_main(display_t * disp_in)
{
	menu = 0; disp = disp_in;

	uprintf( "Running from IRAM. %d\n", global_i );
	esp_log_level_set( "*", ESP_LOG_VERBOSE ); // Enable logging if there's any way.

	loadFont("mm.font", &meleeMenuFont);
	menu = initMeleeMenu("USB Sandbox", &meleeMenuFont, menuCb);
	addRowToMeleeMenu(menu, menu_MainMenu);
	addRowToMeleeMenu(menu, menu_Bootload);

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

#define speedyHash( seed ) (( seed = (seed * 1103515245) + 12345 ), seed>>16 )


void sandbox_tick()
{
	int i, j;
	if( menu )
	    drawMeleeMenu(disp, menu);

	static int seed = 0;
	int iter;
	for( iter = 0; iter < 10; iter++ )
	{
		seed++;
	//	uint32_t end = getCycleCount();
	//	uprintf( "SPROF: %d\n", end-start );

	//INTERESTING
	//	int32_t rom_read_hw_noisefloor();
	//	uprintf( "%d\n", rom_read_hw_noisefloor() );

		uint32_t now = esp_timer_get_time();
		uint8_t buff[256];
		uint8_t * pack = buff;

		*((uint32_t*)pack) = FSNET_CODE_SERVER;  pack += 4;

		*((uint32_t*)pack) = now;  pack += 4; // protVer, models, ships, boolets in UEQ.
		uint32_t assetCounts = 0;
		int acbits = 0;
		int sendmod = 2;
		int sendshp = 2;
		int sendboo = 2;
		acbits += WriteUEQ( &assetCounts, 1 );
		acbits += WriteUEQ( &assetCounts, sendmod ); //models
		acbits += WriteUEQ( &assetCounts, sendshp ); // ships
		acbits += WriteUEQ( &assetCounts, sendboo ); // boolets
		FinalizeUEQ( &assetCounts, acbits );
		*((uint32_t*)pack) = assetCounts; pack += 4;

		static int send_no;
		send_no+=2;
		if( send_no >= 80 ) send_no = 0;

		for( i = 0; i < sendmod; i++ )
		{
			int nrbones = 16;
			// First is a codeword.  Contains ID, bones, bone mapping.
			uint32_t codeword = (send_no+i) | ((nrbones-1)<<8);

			int sbl = 4+8;
			for( j = 0; j < nrbones; j++ )
			{
				int v = !(j==5|| j == 11);
				codeword |= v << (sbl++);

				if( !v )
				{
					//Do we reset back to zero?
					codeword |= 0 << (sbl++);  // 0 is yes, reset back to zero.
					codeword |= 1 << (sbl++);  // Do we draw another line from zero?
				}
			}
			*((uint32_t*)pack) = codeword; pack += 4;

			int16_t loc[3];
			int ang = ((now >> 19)+i*30) % 360;
			loc[0] = speedyHash(seed)>>6;//getSin1024( ang )>>2;
			loc[1] = i+send_no+1000+(speedyHash(seed)>>6);
			loc[2] = speedyHash(seed)>>6;//getCos1024( ang )>>2;
			memcpy( pack, loc, sizeof( loc ) ); pack += sizeof( loc );
			*(pack++) = 255; // radius
			*(pack++) = (i+send_no+iter*10)%216; // req color
			int8_t vel[3] = { 0 };
			memcpy( pack, vel, sizeof( vel ) ); pack += sizeof( vel );

			for( j = 0; j < nrbones; j++ )
			{
				int8_t bone[3];
				int ang = ((now >> 13)+j*40) % 360;
				bone[0] = getSin1024( ang )>>5;
				bone[1] = getCos1024( ang )>>5;
				bone[2] = 30;
				memcpy( pack, bone, sizeof( bone ) ); pack += sizeof( bone );
			}
		}

		for( i = 0; i < sendshp; i++ )
		{
			// Send a ship.
			*(pack++) = i+send_no; // "shipNo"
			int16_t loc[3];
			int ang = ((now >> 12)+i*30) % 360;
			loc[0] = speedyHash( seed ) >> 6; //getSin1024( ang )>>3;
			loc[1] = speedyHash( seed ) >> 6;//200;
			loc[2] = speedyHash( seed ) >> 6;//getCos1024( ang )>>3;
			int8_t vel[3] = { 0 };
			int8_t orot[3] = { 0 };
			memcpy( pack, loc, sizeof( loc ) ); pack += sizeof( loc );
			memcpy( pack, vel, sizeof( vel ) ); pack += sizeof( vel ); //mirrors velAt real speed = ( this * microsecond >> 16 )
			orot[0] = ((now>>16)&0xff);
			orot[1] = 0;
			orot[2] = 0;
			memcpy( pack, orot, sizeof( orot ) ); pack += sizeof( orot );

			uint8_t flags = 1;
			uint16_t kbb = 0;
			memcpy( pack, &flags, sizeof( flags ) ); pack += sizeof( flags );
			memcpy( pack, &kbb, sizeof( kbb ) ); pack += sizeof( kbb );
			*(pack++) = (i+send_no+iter*10)%216; // req color

		}
		// Now, need to send boolets.
		for( i = 0; i < sendboo; i++ )
		{
			int16_t loc[3];
			int16_t rot[2] = { 0 };
			rot[1] = 1000;
			uint16_t bid = i+1 +send_no;

			int ang = (i*30+(now >> 12)) % 360;
			loc[0] = getSin1024( ang )>>3;
			loc[1] = 500;
			loc[2] = getCos1024( ang )>>3;

		    *(pack++) = i+send_no; // Local "bulletID"
		    memcpy( pack, &now, sizeof(now) ); pack += sizeof( now );
		    memcpy( pack, loc, sizeof(loc) ); pack += sizeof( loc );
		    memcpy( pack, rot, sizeof(rot) ); pack += sizeof( rot );
		    memcpy( pack, &bid, sizeof(bid) ); pack += sizeof( bid );
		}

		int len = pack - buff;
		uprintf( "len: %d\n", len );
		now = esp_timer_get_time();
		espNowSend((char*)buff, len); //Don't enable yet.
		stime = now;
	}
}


void sandboxBackgroundDrawCallback(display_t* disp, int16_t x, int16_t y, int16_t w, int16_t h, int16_t up, int16_t upNum )
{
}


int16_t sandboxAdvancedUSB(uint8_t * buffer, uint16_t length, uint8_t isGet )
{
	//uprintf( "---- *\n" );
	return 0;
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
    .modeName = "usb_wifi_base",
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
