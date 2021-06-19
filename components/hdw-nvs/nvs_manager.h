#ifndef _NVS_MANAGER_H_
#define _NVS_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

bool initNvs(bool firstTry);
bool writeNvs32(const char* key, int32_t val);
bool readNvs32(const char* key, int32_t* outVal);

#endif
