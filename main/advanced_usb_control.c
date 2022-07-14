#include "tinyusb.h"
#include "tusb_hid_gamepad.h"
#include <stdio.h>
#include "esp_partition.h"
#include "esp_attr.h"

#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "rom/cache.h"
#include "soc/sensitive_reg.h"
#include "soc/dport_access.h"

#include "swadgeMode.h"

#define ULOG( x... ) 
    //ESP_LOGI( "advanced_usb_control", x )

// TODO
//   * Enable flash modification
//   * Be smarter about printf. 

uint32_t * advanced_usb_scratch_buffer_data;
uint32_t   advanced_usb_scratch_buffer_data_size;
uint32_t   advanced_usb_scratch_immediate[4];
uint8_t  advanced_usb_printf_buffer[2048];
int      advanced_usb_printf_head, advanced_usb_printf_tail;

static int advanced_usb_read_offset;
static int terminal_redirected;

int handle_advanced_usb_control_get( int reqlen, uint8_t * data )
{
    if( advanced_usb_read_offset == 0 ) return 0;
    memcpy( data, (void*)advanced_usb_read_offset, reqlen );
    return reqlen;
}

static int advanced_usb_write_log( void* cookie, const char* data, int size )
{
    int next = ( advanced_usb_printf_head + 1 ) % sizeof( advanced_usb_printf_buffer );
    int idx = 0;
    // Drop extra characters on the floor.
    while( next != advanced_usb_printf_tail && idx < size )
    {
        advanced_usb_printf_buffer[next] = data[idx++];
        advanced_usb_printf_head = next;
        next = ( advanced_usb_printf_head + 1 ) % sizeof( advanced_usb_printf_buffer );
    }
    return size;
}




int advanced_usb_write_log_printf(const char *fmt, va_list args)
{
    char buffer[1024];
    int l = vsnprintf( buffer, 1023, fmt, args );
    advanced_usb_write_log( 0, buffer, l );
    return l;
}

int handle_advanced_usb_terminal_get( int reqlen, uint8_t * data )
{
    if( !terminal_redirected )
    {
        ULOG("redirecting stdout");
        esp_log_set_vprintf( advanced_usb_write_log_printf );
        ULOG("stdout redirected");
        terminal_redirected = 1;
    }
    int togo = ( advanced_usb_printf_head - advanced_usb_printf_tail +
        sizeof( advanced_usb_printf_buffer ) ) % sizeof( advanced_usb_printf_buffer );

    data[0] = 171;

    int mark = 1;
    if( togo )
    {
        if( togo > reqlen-1 ) togo = reqlen-1;
        while( mark++ != togo )
        {
            data[mark] = advanced_usb_printf_buffer[advanced_usb_printf_tail++];
            if( advanced_usb_printf_tail == sizeof( advanced_usb_printf_buffer ) )
                advanced_usb_printf_tail = 0;
        }
    }
    return mark;
}

void IRAM_ATTR handle_advanced_usb_control_set( int datalen, const uint8_t * data )
{
    if( datalen < 6 ) return;
    intptr_t value = data[2] | ( data[3] << 8 ) | ( data[4] << 16 ) | ( data[5]<<24 );
    switch( data[1] )
    {
    case 0x04:
        // Write into scratch.
        ULOG( "Writing %d into %p", datalen-6, (void*)value );
        memcpy( (void*)value, data+6, datalen-6 );
        break;
    case 0x05:
        // Configure read.
        advanced_usb_read_offset = value;
        break;
    case 0x06:
        // Execute scratch
        {
            void (*scratchfn)() = (void (*)())(value);
            ULOG( "Executing %p (%p) // base %08x/%p", (void*)value, scratchfn, 0, advanced_usb_scratch_buffer_data );
            scratchfn();
        }
        break;
    case 0x07:
        // Switch Swadge Mode
        {
            ULOG( "SwadgeMode Value: 0x%08x", value );
            if( value == 0 )
                switchToSwadgeMode( 0 );
            else
                overrideToSwadgeMode( (swadgeMode*)value );
        }
        break;
    case 0x08:
        // (re) allocate the primary scratch buffer.
        {
            // value = -1 will just cause a report.
            // value = 0 will clear it.
            // value < 0 just read current data.
            ULOG( "Allocating to %d (Current: %p / %d)", value, advanced_usb_scratch_buffer_data, advanced_usb_scratch_buffer_data_size );
            if( ! (value & 0x80000000 ) )
            {
                if( value > advanced_usb_scratch_buffer_data_size  )
                {
                    advanced_usb_scratch_buffer_data = realloc( advanced_usb_scratch_buffer_data, value );
                    advanced_usb_scratch_buffer_data_size = value;
                }
                if( value == 0 )
                {
                    if( advanced_usb_scratch_buffer_data ) free( advanced_usb_scratch_buffer_data );
                    advanced_usb_scratch_buffer_data_size = 0;
                }
            }
            advanced_usb_scratch_immediate[0] = (intptr_t)advanced_usb_scratch_buffer_data;
            advanced_usb_scratch_immediate[1] = advanced_usb_scratch_buffer_data_size;
            advanced_usb_read_offset = (intptr_t)(&advanced_usb_scratch_immediate[0]);
            ULOG( "New: %p / %d", advanced_usb_scratch_buffer_data, advanced_usb_scratch_buffer_data_size );
        }
        break;
    }
}



