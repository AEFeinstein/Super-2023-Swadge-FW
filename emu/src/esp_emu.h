#ifndef _ESP_EMU_H_
#define _ESP_EMU_H_

#define UNUSED __attribute__((unused))

#define WARN_UNIMPLEMENTED() do {  \
    static bool isPrinted = false; \
    if(!isPrinted) {               \
        isPrinted = true;          \
        ESP_LOGW("EMU", "%s UNIMPLEMENTED", __func__); \
    }                              \
} while(0)

#endif