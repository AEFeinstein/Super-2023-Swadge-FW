//==============================================================================
// Includes
//==============================================================================

#include <stdio.h>

#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "swadge_esp32.h"
#include "btn.h"

//==============================================================================
// Defines
//==============================================================================

#define GPIO_HIGH_BIT BIT31

//==============================================================================
// Prototypes
//==============================================================================

static void IRAM_ATTR gpio_isr_handler(void* arg);

//==============================================================================
// Variables
//==============================================================================

static gpio_num_t * btnGpios;
static xQueueHandle gpio_evt_queue = NULL;
static uint32_t buttonStates = 0;

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Initialize the given GPIOs as inputs for buttons
 * 
 * @param numButtons The number of GPIOs to initialize as buttons
 * @param ... A list of GPIOs to initialize as buttons
 */
void initButtons(uint8_t numButtons, ...)
{
    ESP_LOGD("BTN", "initializing buttons");

    if(numButtons > 31)
    {
        ESP_LOGE("BTN", "Too many buttons initialized (%d), max 31", numButtons);
        return;
    }

    // Save all the button GPIOs
    btnGpios = malloc(numButtons * sizeof(gpio_num_t));

    // create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // install gpio isr service
    gpio_install_isr_service(0); // See ESP_INTR_FLAG_*

    // For each GPIO
    va_list ap;
    va_start(ap, numButtons);
    for(uint8_t i = 0; i < numButtons; i++)
    {
        // Save the GPIO number
        btnGpios[i] = va_arg(ap, gpio_num_t);

        // Configure the GPIO
        gpio_config_t conf = 
        {
            .intr_type = GPIO_INTR_ANYEDGE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = ((uint64_t)(((uint64_t)1)<<btnGpios[i])),
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_ENABLE
        };
        ESP_ERROR_CHECK(gpio_config(&conf));

        // Get the initial state
        if(!gpio_get_level(btnGpios[i]))
        {
            buttonStates |= (1 << i);
        }

        // Register the interrupt with the index as the arg
        ESP_ERROR_CHECK(gpio_isr_handler_add(btnGpios[i], gpio_isr_handler, (void*)((uintptr_t)i)));
    }
    // Clean up
    va_end(ap);
}

/**
 * @brief Free memory used by the buttons
 */
void deinitButtons(void)
{
    free(btnGpios);
}

/**
 * @brief Interrupt handler for button presses
 *
 * @param arg The index into btnGpios[] of the GPIO that caused this interrupt
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    // Get the button index
    uint8_t buttonIdx = (uint8_t)((uintptr_t)arg);

    // Report this button index and if the GPIO is high or low (GPIO_HIGH_BIT)
    uint32_t gpio_evt = buttonIdx;
    if(gpio_get_level(btnGpios[buttonIdx]))
    {
        gpio_evt |= GPIO_HIGH_BIT;
    }

    // Queue up this event
    xQueueSendFromISR(gpio_evt_queue, &gpio_evt, NULL);
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
        // Save the old button states
        uint32_t oldButtonStates = buttonStates;

        // Get the button bit and edge from the interrupt queue
        uint32_t buttonBit = 1 << (gpio_evt & (~GPIO_HIGH_BIT));
        bool buttonPressed = (gpio_evt & GPIO_HIGH_BIT) ? false : true;

        // Set or clear the bit for this event.
        if(buttonPressed)
        {
            buttonStates |= buttonBit;
        }
        else
        {
            buttonStates &= (~buttonBit);
        }

        // If there was a change in states, report it
        if(oldButtonStates != buttonStates)
        {
            // Build the event to return
            evt->button = buttonBit;
            evt->down = buttonPressed;
            evt->state = buttonStates;

            // Debug print
            ESP_LOGD("BTN", "Bit 0x%02x was %s, buttonStates is %02x",
                   buttonBit,
                   (buttonPressed) ? "pressed " : "released",
                   buttonStates);

            // An event occurred
            return true;
        }
    }
    // Nothing happened
    return false;
}
