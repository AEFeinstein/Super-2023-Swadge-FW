#ifndef _TEXT_ENTRY_H
#define _TEXT_ENTRY_H

#include <stdint.h>
#include "display.h"

void textEntryStart( display_t * usedisp, font_t * usefont, int max_len, char* buffer );
bool textEntryDraw( void );
void textEntryEnd( void );
bool textEntryInput( uint8_t down, uint8_t button );

#endif
