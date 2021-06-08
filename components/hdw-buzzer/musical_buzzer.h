#include <stdbool.h>
#include <stdint.h>
#include "hal/gpio_types.h"
#include "driver/rmt.h"

/**
 * @brief Type of musical buzzer notation
 *
 */
typedef struct
{
    uint32_t note_freq_hz;     /*!< Note frequency, in Hz */
    uint32_t note_duration_ms; /*!< Note duration, in ms */
} musical_buzzer_notation_t;

bool buzzer_init(gpio_num_t gpio, rmt_channel_t rmt);
bool buzzer_play(const musical_buzzer_notation_t* notation, uint32_t notation_length);
bool buzzer_check_next_note(void);
bool buzzer_stop(void);
