#ifndef _NVS_MANAGER_H_
#define _NVS_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool initNvs(bool firstTry);
bool eraseNvs(void);
bool readNvs32(const char* key, int32_t* outVal);
bool writeNvs32(const char* key, int32_t val);
bool readNvsBlob(const char* key, void* out_value, size_t* length);
bool writeNvsBlob(const char* key, const void* value, size_t length);
bool eraseNvsKey(const char* key);

#endif
