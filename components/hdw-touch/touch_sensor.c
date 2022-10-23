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

static inline uint32_t getCycleCount()
{
    uint32_t ccount;
    asm volatile("rsr %0,ccount":"=a" (ccount));
    return ccount;
}


//==============================================================================
// Structs
//==============================================================================

typedef struct touch_msg
{
    touch_pad_intr_mask_t intr_mask;
    touch_pad_t pad_num;
    uint32_t pad_status;
    uint32_t pad_val;
} touch_isr_event_t;

//==============================================================================
// Variables
//==============================================================================

static QueueHandle_t touchEvtQueue = NULL;
static touch_pad_t minPad = 0xFF;
static uint32_t tpState = 0;
static int numTouchPadsKept;
static touch_pad_t * touchPads;
static int32_t * baseOffsets = 0;


//==============================================================================
// Prototypes
//==============================================================================

static void touchsensor_interrupt_cb(void* arg);

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Initialize touchpad sensors
 *
 * @param touchPadSensitivity The sensitivity to set for these touchpads
 * @param denoiseEnable true to denoise the input, false to use it raw
 * @param numTouchPads The number of touchpads to initialize
 * @param ... A list of touchpads to initialize (touch_pad_t)
 */
void initTouchSensor(float touchPadSensitivity, bool denoiseEnable,
                     uint8_t numTouchPads, ...)
{
    ESP_LOGD("TOUCH", "Initializing touch pad");

    /* Create a queue to receive events from the interrupt */
    if (touchEvtQueue == NULL)
    {
        touchEvtQueue = xQueueCreate(3 * numTouchPads, sizeof(touch_isr_event_t));
    }

    touchPads = realloc( touchPads, sizeof( touchPads[0] * numTouchPads ) );
    numTouchPadsKept = numTouchPads;

    /* Initialize touch pad peripheral. */
    ESP_ERROR_CHECK(touch_pad_init());

    /* Initialie each touch pad */
    va_list ap;
    va_start(ap, numTouchPads);
    for(uint8_t i = 0; i < numTouchPads; i++)
    {
        touch_pad_t touchpad = va_arg(ap, touch_pad_t);
        ESP_ERROR_CHECK(touch_pad_config(touchpad));
        touchPads[i] = touchpad;
        if(touchpad < minPad)
        {
            minPad = touchpad;
        }
    }
    va_end(ap);

    /* Initialize denoise if requested */
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
        ESP_LOGD("TOUCH", "Denoise function init");
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
    ESP_LOGD("TOUCH", "touch pad filter init");
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
        /* read benchmark value */
        ESP_ERROR_CHECK(touch_pad_read_benchmark(touchPads[i], &touch_value));
        /* set interrupt threshold */
        ESP_ERROR_CHECK(touch_pad_set_thresh(touchPads[i], touch_value * touchPadSensitivity));
        ESP_LOGD("TOUCH", "touch pad [%d] base %d, thresh %d", touchPads[i],
                 touch_value, (uint32_t)(touch_value * touchPadSensitivity));
    }

    // Make sure this starts off completely cleared for the filter calculation.
    if( baseOffsets )
    {
        free( baseOffsets );
        baseOffsets = 0;
    }

    getTouchCentroid( 0, 0 );
}

/**
 * Handle an interrupt triggered when a pad is touched.
 * Recognize what pad has been touched and save it in a table.
 *
 * @param arg unused
 */
static void touchsensor_interrupt_cb(void* arg)
{
    int task_awoken = pdFALSE;
    touch_isr_event_t evt;

    evt.intr_mask = touch_pad_read_intr_status_mask();
    evt.pad_status = touch_pad_get_status();
    evt.pad_num = touch_pad_get_current_meas_channel();
    //touch_ll_filter_read_smooth(touch_pad_t touch_num, uint32_t *smooth_data)

    xQueueSendFromISR(touchEvtQueue, &evt, &task_awoken);
    if (task_awoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Get totally raw touch sensor values from buffer. NOTE: You must have touch callbacks enabled to use this.
 *
 * @param data is a pointer to an array of int32_t's to receive the raw touch data.
 * @param count is the number of ints in your array.
 * @return is the number of values that were successfully read.
 */
int getTouchRawValues( uint32_t * rawvalues, int maxPads )
{
    if( maxPads > numTouchPadsKept ) maxPads = numTouchPadsKept;
    int i;
    for( i = 0; i < maxPads; i++ )
    {
        // If any errors, abort.
        if( touch_pad_read_raw_data( touchPads[i], &rawvalues[i] ) ) return 0;
    }
    return maxPads;
}

/**
 * @brief Get "zeroed" touch sensor values. NOTE: You must have touch callbacks enabled to use this.
 *
 * @param data is a pointer to an array of int32_t's to receive the zeroed touch data.
 * @param count is the number of ints in your array.
 * @return is the number of values that were successfully read.
 */
int getBaseTouchVals( int32_t * data, int count )
{
    int i;
    uint32_t curVals[numTouchPadsKept];
    if( getTouchRawValues( curVals, numTouchPadsKept ) == 0 ) return 0;
    for( i = 0; i < numTouchPadsKept; i++ )
    {
        if( curVals[i] <= 0 ) return 0;
    }

    if( count > numTouchPadsKept ) count = numTouchPadsKept;

    // curVals is valid.
    if( !baseOffsets )
    {
        baseOffsets = malloc( sizeof( baseOffsets[0] ) * numTouchPadsKept );
        for( i = 0; i < numTouchPadsKept; i++ )
            baseOffsets[i] = curVals[i]<<8;
    }

    for( i = 0; i < numTouchPadsKept; i++ )
    {
        int32_t base = baseOffsets[i];
        int32_t val = curVals[i];
        int32_t baseNorm = (base>>8);

        // Asymmetric filter on base.
        if( baseNorm < val )
        {
            base++; // VERY slowly slack offset up.
        }
        else
        {
            base-=8192; // VERY quickly slack up.
        }

        baseOffsets[i] = base;
        if( i < count )
            data[i] = val - baseNorm;
    }

    return count;
}

/**
 * @brief Get totally raw touch sensor values from buffer.  NOTE: You must have touch callbacks enabled to use this.
 *
 * @param centerVal = pointer to centroid of touch locaiton from 0..1024 inclusive.  Cannot be NULL.
 * @param intensityVal = intensity of touch press.  Cannot be NULL.
 * @return 1 if touched (centroid), 0 if not touched (no centroid)
 */
int getTouchCentroid( int32_t * centerVal, int32_t * intensityVal )
{
    int32_t baseVals[numTouchPadsKept];
    if( !centerVal || !intensityVal || getBaseTouchVals( baseVals, numTouchPadsKept ) == 0 )
        return 0;

    int peak = -1;
    int peakBin = -1;
    int i;
    for( i = 0; i < numTouchPadsKept; i++ )
    {
        int32_t bv = baseVals[i];
        if( bv > peak )
        {
            peak = bv;
            peakBin = i;
        }
    }

    if( peakBin < 0 ) return 0;

    // Arbitrary, but we use 1200 as the minimum peak value.
    if( peak < 1200 ) return 0;

    // We know our peak bin, now we need to know the average and differential of the adjacent bins.
    int leftOfPeak = (peakBin>0)?baseVals[peakBin-1]:0;
    int rightOfPeak = (peakBin<numTouchPadsKept-1)?baseVals[peakBin+1]:0;

    int opeak = peak;
    int center = peakBin<<8;

    if( rightOfPeak >= leftOfPeak  )
    {
        // We bend upward (or are neutral)
        rightOfPeak -= leftOfPeak;
        peak -= leftOfPeak;
        center += (rightOfPeak<<8)/(rightOfPeak + peak );

        *intensityVal = opeak + rightOfPeak;
    }
    else
    {
        // We bend downward
        leftOfPeak -= rightOfPeak;
        peak -= rightOfPeak;
        center -= (leftOfPeak<<8)/(leftOfPeak + peak );

        *intensityVal = opeak + leftOfPeak;
    }
    *centerVal = center;
    return 1;
}

/**
 * @brief Call this function periodically to check the touch pad interrupt queue
 *
 * @param evt Return a touch event through this arg if there was one
 * @return true if there was a touch event, false if there was not
 */
bool checkTouchSensor(touch_event_t* evt)
{
    /* Check the queue, but don't block */
    touch_isr_event_t isrEvt;
    int ret = xQueueReceive(touchEvtQueue, &isrEvt, 0);

    if (ret != pdTRUE)
    {
        return false;
    }

    /* Non-touch event statuses */
    if (isrEvt.intr_mask & TOUCH_PAD_INTR_MASK_SCAN_DONE)
    {
        ESP_LOGI("TOUCH", "The touch sensor group measurement is done [%d].", isrEvt.pad_num);
        return false;
    }
    else if (isrEvt.intr_mask & TOUCH_PAD_INTR_MASK_TIMEOUT)
    {
        /* Add your exception handling in here. */
        ESP_LOGI("TOUCH", "Touch sensor channel %d measure timeout. Skip this exception channel!!", isrEvt.pad_num);
        ESP_ERROR_CHECK(touch_pad_timeout_resume()); // Point on the next channel to measure.
        return false;
    }

    /* Save the pad number, starting at 0 */
    evt->pad = (isrEvt.pad_num - minPad);

    /* Handle events */
    if (isrEvt.intr_mask & TOUCH_PAD_INTR_MASK_ACTIVE)
    {
        // ESP_LOGI("TOUCH", "TouchSensor [%d] be activated, status mask 0x%x", isrEvt.pad_num, isrEvt.pad_status);
        evt->down = true;
        tpState |= (1 << evt->pad);
    }
    else if (isrEvt.intr_mask & TOUCH_PAD_INTR_MASK_INACTIVE)
    {
        // ESP_LOGI("TOUCH", "TouchSensor [%d] be inactivated, status mask 0x%x", isrEvt.pad_num, isrEvt.pad_status);
        evt->down = false;
        tpState &= ~(1 << evt->pad);
    }
    else
    {
        /* Shouldn't happen */
        return false;
    }

    /* Save the whole state */
    evt->state = tpState;

    /* LUT the location */
    const uint8_t touchLoc[] =
    {
        128, // 00000
        0,   // 00001
        64,  // 00010
        32,  // 00011
        128, // 00100
        64,  // 00101
        96,  // 00110
        64,  // 00111
        192, // 01000
        96,  // 01001
        128, // 01010
        85,  // 01011
        160, // 01100
        106, // 01101
        128, // 01110
        96,  // 01111
        255, // 10000
        128, // 10001
        160, // 10010
        106, // 10011
        192, // 10100
        128, // 10101
        149, // 10110
        112, // 10111
        224, // 11000
        149, // 11001
        170, // 11010
        128, // 11011
        192, // 11100
        144, // 11101
        160, // 11110
        128, // 11111
    };
    evt->position = touchLoc[tpState];

    // ESP_LOGI("TOUCH", "pad %d %s, st: 0x%02X, pos: %d", evt->pad, evt->down ? "down" : " up ", evt->state, evt->position);

    /* Return if something was touched or released */
    return true;
}
