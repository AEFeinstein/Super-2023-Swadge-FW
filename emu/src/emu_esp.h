#ifndef _EMU_ESP_H_
#define _EMU_ESP_H_

#include <stdbool.h>

#define UNUSED __attribute__((unused))

#define RTC_DATA_ATTR

#define WARN_UNIMPLEMENTED() do {  \
    static bool isPrinted = false; \
    if(!isPrinted) {               \
        isPrinted = true;          \
        ESP_LOGW("EMU", "%s UNIMPLEMENTED", __func__); \
    }                              \
} while(0)

extern volatile bool threadsShouldRun;
void joinThreads(void);

#endif