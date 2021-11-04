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
void buzzer_init(gpio_num_t gpio, rmt_channel_t rmt)
{
    // Start with the default RMT configuration
    rmt_config_t dev_config = RMT_DEFAULT_CONFIG_TX(gpio, rmt);

    // Enable looping
    dev_config.tx_config.loop_en = true;

    // Install RMT driver
    rmt_config(&dev_config);
    rmt_driver_install(rmt, 0, 0);

    // Enable autostopping when the loop count hits the threshold
    rmt_enable_tx_loop_autostop(rmt, true);

    // Save the channel and clock frequency
    rmt_buzzer.channel = rmt;
    rmt_get_counter_clock(rmt, &rmt_buzzer.counter_clk_hz);
}

/**
 * @brief TODO
 *
 * @param notation
 * @param notation_length
 * @return bool
 */
void buzzer_play(const song_t* song)
{
    // update notation with the new one
    rmt_buzzer.song = song;
    rmt_buzzer.note_index = 0;
    rmt_buzzer.start_time = esp_timer_get_time();

    // Play the first note
    play_note();
}

/**
 * @brief TODO
 *
 * @return bool
 */
void buzzer_check_next_note(void)
{
    // Check if there is a song and there are still notes
    if((NULL != rmt_buzzer.song) &&
        (rmt_buzzer.note_index < rmt_buzzer.song->numNotes))
    {
        // Get the current time
        int64_t cTime = esp_timer_get_time();
        
        // Check if it's time to play the next note
        if (cTime - rmt_buzzer.start_time >= (1000 * rmt_buzzer.song->notes[rmt_buzzer.note_index].timeMs))
        {
            // Make sure RMT is idle
            uint32_t status;
            rmt_get_status(rmt_buzzer.channel, &status);
            if(0 == (status & RMT_TX_START_CH1))
            {
                // Move to the next note
                rmt_buzzer.note_index++;
                rmt_buzzer.start_time = cTime;

                // If there is a note
                if(rmt_buzzer.note_index < rmt_buzzer.song->numNotes)
                {
                    // Play the note
                    play_note();
                }
                else
                {
                    // Song is over
                    buzzer_stop();
                }
            }
        }
    }
}

/**
 * @brief TODO
 *
 * @return rmt_item32_t
 */
static void play_note(void)
{
    const musicalNote_t* notation = &rmt_buzzer.song->notes[rmt_buzzer.note_index];

    if(SILENCE == notation->note)
    {
        buzzer_stop();
    }
    else
    {
        static rmt_item32_t notation_code;
        notation_code.level0 = 1;
        // convert frequency to RMT item format
        notation_code.duration0 = rmt_buzzer.counter_clk_hz / notation->note / 2;
        notation_code.level1 = 0,
        // Copy RMT item format
        notation_code.duration1 = notation_code.duration0;

        // convert duration to RMT loop count
        rmt_set_tx_loop_count(rmt_buzzer.channel, notation->timeMs * notation->note / 1000);

        // start TX
        rmt_write_items(rmt_buzzer.channel, &notation_code, 1, false);
    }
}

/**
 * @brief TODO
 *
 * @return bool
 */
void buzzer_stop(void)
{
    // Stop transmitting and reset memory
    rmt_tx_stop(rmt_buzzer.channel);
    rmt_tx_memory_reset(rmt_buzzer.channel);
}
