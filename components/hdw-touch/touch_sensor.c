//==============================================================================
// Includes
//==============================================================================

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/rtc_periph.h"
#include "soc/sens_periph.h"
#include "esp_log.h"
#include "touch_sensor.h"

//==============================================================================
// Structs
//==============================================================================

typedef struct touch_msg {
    touch_pad_intr_mask_t intr_mask;
    uint32_t pad_num;
    uint32_t pad_status;
    uint32_t pad_val;
} touch_event_t;

//==============================================================================
// Variables
//==============================================================================

static QueueHandle_t que_touch = NULL;

//==============================================================================
// Prototypes
//==============================================================================

static void touchsensor_interrupt_cb(void *arg);

//==============================================================================
// Functions
//==============================================================================

/**
 * Handle an interrupt triggered when a pad is touched.
 * Recognize what pad has been touched and save it in a table.
 *
 * @param arg unused
 */
static void touchsensor_interrupt_cb(void *arg)
{
    int task_awoken = pdFALSE;
    touch_event_t evt;

    evt.intr_mask = touch_pad_read_intr_status_mask();
    evt.pad_status = touch_pad_get_status();
    evt.pad_num = touch_pad_get_current_meas_channel();

    xQueueSendFromISR(que_touch, &evt, &task_awoken);
    if (task_awoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Initialize touchpad sensors
 *
 * @param numTouchPads The number of touchpads to initialize
 * @param touchPads An array of touchpads to initialize
 * @param touchPadSensitivity The sensitivity to set for these touchpads
 * @param denoiseEnable true to denoise the input, false to use it raw
 */
void initTouchSensor(uint8_t numTouchPads, touch_pad_t * touchPads,
                     float touchPadSensitivity, bool denoiseEnable)
{
    ESP_LOGD("TOUCH", "Initializing touch pad\n");

    /* Create a queue to receive events from the interrupt */
    if (que_touch == NULL)
    {
        que_touch = xQueueCreate(numTouchPads, sizeof(touch_event_t));
    }

    /* Initialize touch pad peripheral. */
    touch_pad_init();
    for (int i = 0; i < numTouchPads; i++)
    {
        ESP_ERROR_CHECK(touch_pad_config(touchPads[i]));
    }

    /* Other configuration settings, just as an example */
    // touch_pad_set_meas_time(TOUCH_PAD_SLEEP_CYCLE_DEFAULT,
    //                         TOUCH_PAD_MEASURE_CYCLE_DEFAULT);
    // touch_pad_set_voltage(TOUCH_PAD_HIGH_VOLTAGE_THRESHOLD,
    //                       TOUCH_PAD_LOW_VOLTAGE_THRESHOLD,
    //                       TOUCH_PAD_ATTEN_VOLTAGE_THRESHOLD);
    // touch_pad_set_idle_channel_connect(TOUCH_PAD_IDLE_CH_CONNECT_DEFAULT);
    // for (int i = 0; i < numTouchPads; i++)
    // {
    //     touch_pad_set_cnt_mode(touchPads[i], TOUCH_PAD_SLOPE_DEFAULT,
    //                            TOUCH_PAD_TIE_OPT_DEFAULT);
    // }

    if(denoiseEnable)
    {
        /* Denoise setting at TouchSensor 0. */
        touch_pad_denoise_t denoise =
        {
            /* The bits to be cancelled are determined according to the noise
             * level.
             */
            .grade = TOUCH_PAD_DENOISE_BIT4,
            /* By adjusting the parameters, the reading of T0 should be
             * approximated to the reading of the measured channel.
             */
            .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
        };
        ESP_ERROR_CHECK(touch_pad_denoise_set_config(&denoise));
        ESP_ERROR_CHECK(touch_pad_denoise_enable());
        ESP_LOGD("TOUCH", "Denoise function init\n");
    }

    /* Filter setting */
    touch_filter_config_t filter_info =
    {
        .mode = TOUCH_PAD_FILTER_IIR_16,  // Test jitter and filter 1/4.
        .debounce_cnt = 1,                // 1 time count.
        .noise_thr = 0,                   // 50%
        .jitter_step = 4,                 // use for jitter mode.
        .smh_lvl = TOUCH_PAD_SMOOTH_IIR_2,
    };
    ESP_ERROR_CHECK(touch_pad_filter_set_config(&filter_info));
    ESP_ERROR_CHECK(touch_pad_filter_enable());
    ESP_LOGD("TOUCH", "touch pad filter init\n");
    ESP_ERROR_CHECK(touch_pad_timeout_set(true, SOC_TOUCH_PAD_THRESHOLD_MAX));

    /* Register touch interrupt ISR, enable intr type. */
    ESP_ERROR_CHECK(touch_pad_isr_register(touchsensor_interrupt_cb, NULL,
        TOUCH_PAD_INTR_MASK_ALL));

    /* Enable interrupts, but not TOUCH_PAD_INTR_MASK_SCAN_DONE */
    ESP_ERROR_CHECK(touch_pad_intr_enable(TOUCH_PAD_INTR_MASK_ACTIVE   |
                                          TOUCH_PAD_INTR_MASK_INACTIVE |
                                          TOUCH_PAD_INTR_MASK_TIMEOUT));

    /* Enable touch sensor clock. Work mode is "timer trigger" */
    ESP_ERROR_CHECK(touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER));
    ESP_ERROR_CHECK(touch_pad_fsm_start());

    /* Wait touch sensor init done */
    vTaskDelay(50 / portTICK_RATE_MS);

    /* Set thresholds */
    uint32_t touch_value;
    for (int i = 0; i < numTouchPads; i++)
    {
        //read benchmark value
        ESP_ERROR_CHECK(touch_pad_read_benchmark(touchPads[i], &touch_value));
        //set interrupt threshold.
        touch_pad_set_thresh(touchPads[i], touch_value * touchPadSensitivity);
        ESP_LOGD("TOUCH", "touch pad [%d] base %d, thresh %d\n", touchPads[i],
                        touch_value, (uint32_t)(touch_value * touchPadSensitivity));
    }
}

/**
 * Call this function periodically to check the touch pad interrupt queue
 *
 * TODO: have a callback for these events
 */
void checkTouchSensor(void)
{
    /* Check the queue, but don't block */
    touch_event_t evt = {0};
    int ret = xQueueReceive(que_touch, &evt, 0);
    if (ret != pdTRUE)
    {
        return;
    }

    /* Handle events */
    if (evt.intr_mask & TOUCH_PAD_INTR_MASK_ACTIVE)
    {
        ESP_LOGI("TOUCH", "TouchSensor [%d] be activated, status mask 0x%x\n", evt.pad_num, evt.pad_status);
    }
    if (evt.intr_mask & TOUCH_PAD_INTR_MASK_INACTIVE)
    {
        ESP_LOGI("TOUCH", "TouchSensor [%d] be inactivated, status mask 0x%x\n", evt.pad_num, evt.pad_status);
    }
    if (evt.intr_mask & TOUCH_PAD_INTR_MASK_SCAN_DONE)
    {
        ESP_LOGI("TOUCH", "The touch sensor group measurement is done [%d].\n", evt.pad_num);
    }
    if (evt.intr_mask & TOUCH_PAD_INTR_MASK_TIMEOUT)
    {
        /* Add your exception handling in here. */
        ESP_LOGI("TOUCH", "Touch sensor channel %d measure timeout. Skip this exception channel!!\n", evt.pad_num);
        ESP_ERROR_CHECK(touch_pad_timeout_resume()); // Point on the next channel to measure.
    }
}
