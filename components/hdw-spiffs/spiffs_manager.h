#ifndef _SPIFFS_MANAGER_H_
#define _SPIFFS_MANAGER_H_

#include <stdbool.h>
#include <stddef.h>

bool initSpiffs(void);
bool deinitSpiffs(void);

bool spiffsReadFile(const char * fname, uint8_t ** output, size_t * outsize);

#endif
