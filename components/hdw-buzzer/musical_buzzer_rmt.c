/*******************************************************************************
 * Includes
 ******************************************************************************/

#include "esp_timer.h"
#include "musical_buzzer.h"

/*******************************************************************************
 * Structs
 ******************************************************************************/

typedef struct
{
    rmt_channel_t channel;
    uint32_t counter_clk_hz;
    const musical_buzzer_notation_t* notation;
    uint32_t notation_length;
    uint32_t notation_index;
    int64_t start_time;
} rmt_buzzer_t;

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/

static void play_note(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/

rmt_buzzer_t rmt_buzzer;

/*******************************************************************************
 * Functions
 ******************************************************************************/

/**
 * @brief TOOD
 *
 */
bool buzzer_init(gpio_num_t gpio, rmt_channel_t rmt)
{
    // Apply default RMT configuration
    rmt_config_t dev_config = RMT_DEFAULT_CONFIG_TX(gpio, rmt);
    dev_config.tx_config.loop_en = true; // Enable loop mode

    // Install RMT driver
    rmt_config(&dev_config);
    rmt_driver_install(rmt, 0, 0);

    // Install buzzer driver
    rmt_buzzer.channel = rmt;

    // Fill in counter clock frequency
    rmt_get_counter_clock(rmt_buzzer.channel, &rmt_buzzer.counter_clk_hz);

    return true;
}

/**
 * @brief TODO
 *
 * @param notation
 * @param notation_length
 * @return bool
 */
bool buzzer_play(const musical_buzzer_notation_t* notation, uint32_t notation_length)
{
    // update notation with the new one
    rmt_buzzer.notation = notation;
    rmt_buzzer.notation_index = 0;
    rmt_buzzer.notation_length = notation_length;
    rmt_buzzer.start_time = esp_timer_get_time();

    // Play the first note
    play_note();

    return true;
}

/**
 * @brief TODO
 *
 * @return bool
 */
bool buzzer_check_next_note(void)
{
    // Get the current time
    int64_t cTime = esp_timer_get_time();

    // Check if it's time to play the next note
    if(cTime - rmt_buzzer.start_time >= (1000 * rmt_buzzer.notation[rmt_buzzer.notation_index].note_duration_ms))
    {
        // Move to the next note
        rmt_buzzer.notation_index++;
        rmt_buzzer.start_time = cTime;

        // Play the note
        play_note();
    }
    return true;
}

/**
 * @brief TODO
 *
 * @return rmt_item32_t
 */
static void play_note(void)
{
    rmt_item32_t notation_code =
    {
        .level0 = 1,
        .duration0 = 1,
        .level1 = 0,
        .duration1 = 1
    };
    const musical_buzzer_notation_t* notation = &rmt_buzzer.notation[rmt_buzzer.notation_index];

    // convert frequency to RMT item format
    notation_code.duration0 = rmt_buzzer.counter_clk_hz / notation->note_freq_hz / 2;
    notation_code.duration1 = notation_code.duration0;

    // convert duration to RMT loop count
    rmt_set_tx_loop_count(rmt_buzzer.channel, notation->note_duration_ms * notation->note_freq_hz / 1000);

    // start TX
    rmt_write_items(rmt_buzzer.channel, &notation_code, 1, false);
}

/**
 * @brief TODO
 *
 * @return bool
 */
bool buzzer_stop(void)
{
    rmt_tx_stop(rmt_buzzer.channel);
    return true;
}
