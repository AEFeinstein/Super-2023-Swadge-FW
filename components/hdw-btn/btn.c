//==============================================================================
// Includes
//==============================================================================

#include <stdio.h>

#include "driver/gpio.h"
#include "driver/dedic_gpio.h"
#include "driver/timer.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "btn.h"

//==============================================================================
// Defines
//==============================================================================

#define TIMER_DIVIDER (16)  //  Hardware timer clock divider
#define TIMER_SCALE   (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

//==============================================================================
// Prototypes
//==============================================================================

static bool IRAM_ATTR btn_timer_isr_cb(void* args);

//==============================================================================
// Variables
//==============================================================================

// A bundle of GPIOs to read as button input
dedic_gpio_bundle_handle_t bundle = NULL;

// A queue to move button reads from the ISR to the main loop
static xQueueHandle gpio_evt_queue = NULL;

// The current state of the buttons
static uint32_t buttonStates = 0;

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Initialize the given GPIOs as inputs for buttons
 * The GPIOs are polled on a hardware timer
 *
 * @param group_num The timer group number to poll GPIOs with
 * @param timer_num The timer number to poll GPIOs with
 * @param numButtons The number of GPIOs to initialize as buttons
 * @param ... A list of GPIOs to initialize as buttons
 */
void initButtons(timer_group_t group_num, timer_idx_t timer_num, uint8_t numButtons, ...)
{
    ESP_LOGD("BTN", "initializing buttons");

    // Make sure there aren't too many
    if(numButtons > 31)
    {
        ESP_LOGE("BTN", "Too many buttons initialized (%d), max 31", numButtons);
        return;
    }

    // Make a list of all the button GPIOs
    int bundle_gpios[numButtons];

    // For each GPIO
    va_list ap;
    va_start(ap, numButtons);
    for(uint8_t i = 0; i < numButtons; i++)
    {
        // Get the GPIO, put it in a bundle
        bundle_gpios[i] = va_arg(ap, gpio_num_t);

        // Configure the GPIO
        gpio_config_t io_conf =
        {
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = true,
            .pull_down_en = false,
        };
        io_conf.pin_bit_mask = 1ULL << bundle_gpios[i];
        gpio_config(&io_conf);
    }
    va_end(ap);

    // Create bundle, input only
    dedic_gpio_bundle_config_t bundle_config =
    {
        .gpio_array = bundle_gpios,
        .array_size = numButtons,
        .flags = {
            .in_en = 1,
            .in_invert = 1,
            .out_en = 0,
            .out_invert = 0
        }
    };
    ESP_ERROR_CHECK(dedic_gpio_new_bundle(&bundle_config, &bundle));

    // Get initial state
    buttonStates = dedic_gpio_bundle_read_in(bundle);

    // create a queue to handle polling GPIO from ISR
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Initialize the timer
    timer_config_t config =
    {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    }; // default clock source is APB
    timer_init(group_num, timer_num, &config);

    // Set initial timer count value
    timer_set_counter_value(group_num, timer_num, 0);

    // Configure the alarm value and the interrupt on alarm.
    timer_set_alarm_value(group_num, timer_num, TIMER_SCALE / 1000); // 1ms timer
    timer_enable_intr(group_num, timer_num);

    // Configure the ISR
    timer_isr_callback_add(group_num, timer_num, btn_timer_isr_cb, NULL, 0);

    // Start the timer
    timer_start(group_num, timer_num);
}

/**
 * @brief Free memory used by the buttons
 */
void deinitButtons(void)
{
    // Nothing allocated
}

/**
 * @brief Interrupt called by the hardware timer to poll buttons
 *
 * @param args unused
 * @return if we need to yield at the end of the ISR
 * @return false
 */
static bool IRAM_ATTR btn_timer_isr_cb(void* args __attribute__((unused)))
{
    BaseType_t high_task_awoken = pdFALSE;
    // Read GPIOs and enqueue for main task
    uint32_t evt = dedic_gpio_bundle_read_in(bundle);
    xQueueSendFromISR(gpio_evt_queue, &evt, &high_task_awoken);
    // return whether we need to yield at the end of ISR
    return high_task_awoken == pdTRUE;
}

/**
 * @brief Service the queue of button events that caused interrupts
 *
 * @param evt If an event occurred, return it through this argument
 * @return true if an event occurred, false if nothing happened
 */
bool checkButtonQueue(buttonEvt_t* evt)
{
    // Check if there's an event to dequeue from the ISR
    uint32_t gpio_evt;
    if (xQueueReceive(gpio_evt_queue, &gpio_evt, 0))
    {
        // Save the old state, set the new state
        uint32_t oldButtonStates = buttonStates;
        buttonStates = gpio_evt;
        // If there was a change
        if(oldButtonStates != buttonStates)
        {
            // Figure out what the change was
            evt->button = oldButtonStates ^ buttonStates;
            evt->down = (buttonStates > oldButtonStates);
            evt->state = buttonStates;

            // Debug print
            // ESP_LOGE("BTN", "Bit 0x%02x was %s, buttonStates is %02x",
            //     evt->button,
            //     (evt->down) ? "pressed " : "released",
            //     evt->state);

            // Something happened
            return true;
        }
    }
    // Nothing happened
    return false;
}
