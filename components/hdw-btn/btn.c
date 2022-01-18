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
// Structs
//==============================================================================

typedef struct
{
    gpio_num_t gpioNum;
    uint32_t savedBit;
    gpio_config_t gpioConf;
}  gpio_config_plus_t;

//==============================================================================
// Prototypes
//==============================================================================

static void IRAM_ATTR gpio_isr_handler(void* arg);

//==============================================================================
// Variables
//==============================================================================

gpio_config_plus_t gpioConfP[] =
{
    {
        .gpioNum = GPIO_NUM_0, // LEFT
        .savedBit = BIT0,
        .gpioConf =
        {
            .pin_bit_mask = GPIO_SEL_0,
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_ANYEDGE,
        }
    },
    {
        .gpioNum = GPIO_NUM_3, // RIGHT
        .savedBit = BIT1,
        .gpioConf =
        {
            .pin_bit_mask = GPIO_SEL_3,
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_ANYEDGE,
        }
    },
    {
        .gpioNum = GPIO_NUM_2, // MODE
        .savedBit = BIT2,
        .gpioConf =
        {
            .pin_bit_mask = GPIO_SEL_2,
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_ANYEDGE,
        }
    },
    {
        .gpioNum = GPIO_NUM_4, // OLED RST OUTPUT
        .savedBit = 0,
        .gpioConf =
        {
            .pin_bit_mask = GPIO_SEL_4,
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        }
    }
};

static xQueueHandle gpio_evt_queue = NULL;
uint32_t buttonStates = 0;

//==============================================================================
// Functions
//==============================================================================

/**
 * TODO
 */
void initButtons(void)
{
    ESP_LOGD("BTN", "initializing buttons\n");

    // Configure GPIOs
    for(uint8_t i = 0; i < ARRAY_SIZE(gpioConfP); i++)
    {
        gpio_config(&(gpioConfP[i].gpioConf));
        if(GPIO_MODE_INPUT == gpioConfP[i].gpioConf.mode)
        {
            if(gpio_get_level(gpioConfP[i].gpioNum))
            {
                buttonStates |= gpioConfP[i].savedBit;
            }
        }
    }

    // create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(gpio_config_plus_t*));

    // install gpio isr service
    gpio_install_isr_service(0); // See ESP_INTR_FLAG_*

    // Configure interrupts
    for(uint8_t i = 0; i < ARRAY_SIZE(gpioConfP); i++)
    {
        if(GPIO_INTR_DISABLE != gpioConfP[i].gpioConf.intr_type)
        {
            gpio_isr_handler_add(gpioConfP[i].gpioNum, gpio_isr_handler, (gpio_config_plus_t*) &gpioConfP[i]);
        }
    }
}

/**
 * @brief TODO
 *
 * @param arg
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    // Get the GPIO config from the interrupt argument
    gpio_config_plus_t* conf = (gpio_config_plus_t*)arg;

    // Get the corresponding bit from the configuration
    uint32_t savedBit = conf->savedBit;

    // Set BIT31 depending on the GPIO state
    if(gpio_get_level(conf->gpioNum))
    {
        savedBit |= BIT31;
    }

    // Queue up this event
    xQueueSendFromISR(gpio_evt_queue, &savedBit, NULL);
}

/**
 * @brief TODO
 * 
 * @param evt 
 * @return 
 */
bool checkButtonQueue(buttonEvt_t* evt)
{
    uint32_t gpio_evt;
    // Check if there's an event to dequeue from the ISR
    if (xQueueReceive(gpio_evt_queue, &gpio_evt, 0))
    {
        // Save the old button states
        uint32_t oldButtonStates = buttonStates;

        // Set or clear the corresponding bit for this event.
        // BIT31 indicates rising or falling edge
        if(gpio_evt & BIT31)
        {
            buttonStates |= (gpio_evt & (~BIT31));
        }
        else
        {
            buttonStates &= ~(gpio_evt & (~BIT31));
        }

        // If there was a change in states, print it
        if(oldButtonStates != buttonStates)
        {
            evt->button = gpio_evt & (~BIT31);
            evt->down = !(gpio_evt & BIT31);
            evt->state = buttonStates;
            // ESP_LOGD("BTN", ("Bit 0x%02x went %s, buttonStates is %02x\n",
            //        gpio_evt & (~BIT31),
            //        (gpio_evt & BIT31) ? "high" : "low ",
            //        buttonStates);
            return true;
        }
    }
    return false;
}
