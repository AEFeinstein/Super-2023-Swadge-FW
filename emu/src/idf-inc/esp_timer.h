#ifndef _ESP_TIMER_H_
#define _ESP_TIMER_H_

#include <stdint.h>
#include <stdbool.h>
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

/**
 * @brief Method for dispatching timer callback
 */
typedef enum {
    ESP_TIMER_TASK,     //!< Callback is called from timer task
#ifdef CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD
    ESP_TIMER_ISR,      //!< Callback is called from timer ISR
#endif
    ESP_TIMER_MAX,      //!< Count of the methods for dispatching timer callback
} esp_timer_dispatch_t;

typedef struct {
    esp_timer_cb_t callback;        //!< Function to call when timer expires
    void* arg;                      //!< Argument to pass to the callback
    esp_timer_dispatch_t dispatch_method;   //!< Call the callback from task or from ISR
    const char* name;               //!< Timer name, used in esp_timer_dump function
    bool skip_unhandled_events;     //!< Skip unhandled events for periodic timers
} esp_timer_create_args_t;

int64_t esp_timer_get_time(void);

esp_err_t esp_timer_init(void);
esp_err_t esp_timer_deinit(void);

esp_err_t esp_timer_create(const esp_timer_create_args_t* create_args,
                           esp_timer_handle_t* out_handle);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);

esp_err_t esp_timer_stop(esp_timer_handle_t timer);
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period);

void check_esp_timer(uint64_t elapsed_us);

#endif