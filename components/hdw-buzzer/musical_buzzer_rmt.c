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
    const song_t* song;
    uint32_t note_index;
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
bool buzzer_play(const song_t* song)
{
    // update notation with the new one
    rmt_buzzer.song = song;
    rmt_buzzer.note_index = 0;
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

    if(NULL != rmt_buzzer.song && rmt_buzzer.note_index < rmt_buzzer.song->numNotes)
    {
        // Check if it's time to play the next note
        if(cTime - rmt_buzzer.start_time >= (1000 * rmt_buzzer.song->notes[rmt_buzzer.note_index].timeMs))
        {
            // Move to the next note
            rmt_buzzer.note_index++;
            rmt_buzzer.start_time = cTime;

            // Play the note
            if(rmt_buzzer.note_index < rmt_buzzer.song->numNotes)
            {
                play_note();
            }
            else
            {
                buzzer_stop();
            }
        }
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
    static rmt_item32_t notation_code =
    {
        .level0 = 1,
        .duration0 = 1,
        .level1 = 0,
        .duration1 = 1
    };
    const musicalNote_t* notation = &rmt_buzzer.song->notes[rmt_buzzer.note_index];

    if(SILENCE == notation->note)
    {
        notation_code.level0 = 0;
        notation_code.duration0 = 1;
        notation_code.duration1 = 1;
    }
    else
    {
        notation_code.level0 = 1;
        // convert frequency to RMT item format
        notation_code.duration0 = rmt_buzzer.counter_clk_hz / notation->note / 2;
        notation_code.duration1 = notation_code.duration0;
    }

    // convert duration to RMT loop count
    rmt_set_tx_loop_count(rmt_buzzer.channel, notation->timeMs * notation->note / 1000);

    // start TX
    // TODO This is halting sometimes, but only when espnow is used
    // Note, commenting this out kills sound but does NOT fix the halt
    // Commenting out both calls to rmt_write_items() seems to fix it
    rmt_write_items(rmt_buzzer.channel, &notation_code, 1, false);
}

/**
 * @brief TODO
 *
 * @return bool
 */
bool buzzer_stop(void)
{
    // Don't actually stop RMT, which seems to cause problems
    // Instead just have it play silence
    static rmt_item32_t notation_code =
    {
        .level0 = 0,
        .duration0 = 1,
        .level1 = 0,
        .duration1 = 1
    };
    rmt_set_tx_loop_count(rmt_buzzer.channel, 1);
    // TODO this can also cause halts
    rmt_write_items(rmt_buzzer.channel, &notation_code, 1, false);

    rmt_buzzer.song = NULL;
    rmt_buzzer.note_index = 0;
    rmt_buzzer.start_time = 0;

    return true;
}
