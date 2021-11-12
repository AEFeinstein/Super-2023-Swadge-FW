#ifndef _SPIFFS_MANAGER_H_
#define _SPIFFS_MANAGER_H_

#include <stdbool.h>

bool initSpiffs(void);
bool deinitSpiffs(void);
bool spiffsTest(void);

#endif
