#ifndef _ADVANCED_USB_CONTROL_H

#include <stdint.h>

void advanced_usb_tick();
int handle_advanced_usb_control_get( int reqlen, uint8_t * data );
int handle_advanced_usb_terminal_get( int reqlen, uint8_t * data );
void handle_advanced_usb_control_set( int datalen, const uint8_t * data );
void advanced_usb_setup();

#endif

