/*******************************************************************************
 * Includes
 ******************************************************************************/

#include <stdio.h>

#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "esp_intr_alloc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "hello_world_main.h"
#include "btn.h"

/*******************************************************************************
 * Structs
 ******************************************************************************/

typedef struct
{
    gpio_num_t gpioNum;
    uint32_t savedBit;
    gpio_config_t gpioConf;
}  gpio_config_plus_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

static void gpio_task_example(void* arg);
static void IRAM_ATTR gpio_isr_handler(void* arg);

/*******************************************************************************
 * Variables
 ******************************************************************************/

gpio_config_plus_t gpioConfP[] =
{
    // {
    //     .gpioNum = GPIO_NUM_4,
    //     .savedBit = BIT0,
    //     .gpioConf =
    //     {
    //         .pin_bit_mask = GPIO_SEL_4,
    //         .mode         = GPIO_MODE_INPUT,
    //         .pull_up_en   = GPIO_PULLUP_ENABLE,
    //         .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //         .intr_type    = GPIO_INTR_ANYEDGE,
    //     }
    // },
    // {
    //     .gpioNum = GPIO_NUM_5,
    //     .savedBit = BIT1,
    //     .gpioConf =
    //     {
    //         .pin_bit_mask = GPIO_SEL_5,
    //         .mode         = GPIO_MODE_INPUT,
    //         .pull_up_en   = GPIO_PULLUP_ENABLE,
    //         .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //         .intr_type    = GPIO_INTR_ANYEDGE,
    //     }
    // },
    {
        .gpioNum = GPIO_NUM_4,
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

/*******************************************************************************
 * Functions
 ******************************************************************************/

/**
 * TODO
 */
void initButtons(void)
{
    printf("initializing buttons\n");

    // Configure GPIOs
    for(uint8_t i = 0; i < ARRAY_SIZE(gpioConfP); i++)
    {
        gpio_config(&(gpioConfP[i].gpioConf));
    }

    // create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(gpio_config_plus_t*));

    // start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

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

void setOledResetOn(bool on)
{
    gpio_set_level(GPIO_NUM_4, on ? 1 : 0);
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
 * @param arg
 */
static void gpio_task_example(void* arg)
{
    uint32_t gpio_evt;
    while(1)
    {
        // Check if there's an event to dequeue from the ISR
        if(xQueueReceive(gpio_evt_queue, &gpio_evt, portMAX_DELAY))
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
                printf("Bit 0x%02x went %s, buttonStates is %02x\n",
                       gpio_evt & (~BIT31),
                       (gpio_evt & BIT31) ? "high" : "low ",
                       buttonStates);
            }
        }
    }
}
