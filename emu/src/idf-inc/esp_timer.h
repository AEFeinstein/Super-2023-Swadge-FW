#ifndef _ESP_TIMER_H_
#define _ESP_TIMER_H_

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    FL_ISR_DISPATCH_METHOD   = (1 << 0),  //!< 0=Callback is called from timer task, 1=Callback is called from timer ISR
    FL_SKIP_UNHANDLED_EVENTS = (1 << 1),  //!< 0=NOT skip unhandled events for periodic timers, 1=Skip unhandled events for periodic timers
} flags_t;

typedef void (*esp_timer_cb_t)(void* arg);

struct esp_timer {
    uint64_t alarm;
    uint64_t period:56;
    flags_t flags:8;
    union {
        esp_timer_cb_t callback;
        uint32_t event_id;
    };
    void* arg;
#if WITH_PROFILING
    const char* name;
    size_t times_triggered;
    size_t times_armed;
    size_t times_skipped;
    uint64_t total_callback_run_time;
#endif // WITH_PROFILING
    // LIST_ENTRY(esp_timer) list_entry;
};

typedef struct esp_timer* esp_timer_handle_t;

int64_t esp_timer_get_time(void);
esp_err_t esp_timer_init(void);

#endif