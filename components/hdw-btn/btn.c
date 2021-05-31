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
    {
        .gpioNum = GPIO_NUM_4,
        .gpioConf =
        {
            .pin_bit_mask = GPIO_SEL_4,
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_ANYEDGE,
        }
    },
    {
        .gpioNum = GPIO_NUM_5,
        .gpioConf =
        {
            .pin_bit_mask = GPIO_SEL_5,
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_ANYEDGE,
        }
    }
};

static xQueueHandle gpio_evt_queue = NULL;

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

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(0); // See ESP_INTR_FLAG_*

    // Configure GPIOs
    for(uint8_t i = 0; i < ARRAY_SIZE(gpioConfP); i++)
    {
        gpio_isr_handler_add(gpioConfP[i].gpioNum, gpio_isr_handler, (void*) gpioConfP[i].gpioNum);
    }
}

/**
 * @brief TODO
 *
 * @param arg
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

/**
 * @brief TODO
 *
 * @param arg
 */
static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    while(1)
    {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}
