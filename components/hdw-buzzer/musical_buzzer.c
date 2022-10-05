//==============================================================================
// Includes
//==============================================================================

#include "esp_log.h"
#include "esp_timer.h"
#include "musical_buzzer.h"

//==============================================================================
// Defines
//==============================================================================

// For LEDC
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4095) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095

// For Timer
#define TIMER_DIVIDER (16)  //  Hardware timer clock divider
#define TIMER_TICKS_PER_SECOND   (TIMER_BASE_CLK / TIMER_DIVIDER)  // cycles per second
#define NOTE_ISR_MS   5

//==============================================================================
// Enums
//==============================================================================

typedef struct
{
    const song_t* song;
    uint32_t note_index;
    int64_t start_time;
} buzzerTrack_t;

//==============================================================================
// Variables
//==============================================================================

struct
{
    buzzerTrack_t bgm;
    buzzerTrack_t sfx;
    const musicalNote_t* playNote;
    bool isBgmMuted;
    bool isSfxMuted;
    gpio_num_t gpio;
    uint32_t gpioLevel;
    timer_group_t noteCheckGroupNum;
    timer_idx_t noteCheckTimerNum;
    ledc_timer_t ledcTimer;
    ledc_channel_t ledcChannel;
    uint32_t freq;
} bzr;

//==============================================================================
// Functions Prototypes
//==============================================================================

static bool buzzer_check_next_note_isr(void * ptr);
static bool buzzer_track_check_next_note(buzzerTrack_t* track, bool isActive, int64_t cTime);

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Initialize the buzzer
 * 
 * @param bzrGpio The GPIO the buzzer is attached to
 * @param ledcTimer The LEDC timer used to drive the buzzer
 * @param ledcChannel THe LED channel used to drive the buzzer
 * @param noteCheckGrpNum The HW timer group used to check for new notes
 * @param noteChkTmrNum The HW timer number used to check for new notes
 * @param isBgmMuted True if background music is muted, false otherwise
 * @param isSfxMuted True if sound effects are muted, false otherwise
 */
void buzzer_init(gpio_num_t bzrGpio,
    ledc_timer_t ledcTimer, ledc_channel_t ledcChannel,
    timer_group_t noteCheckGrpNum, timer_idx_t noteChkTmrNum,
    bool isBgmMuted, bool isSfxMuted)
{
    // Save the LEDC timer and channel
    bzr.ledcTimer = ledcTimer;
    bzr.ledcChannel = ledcChannel;

    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = bzr.ledcTimer,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = C_4, // Gotta start somewhere, might as well be middle C
        .clk_cfg          = LEDC_USE_APB_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = bzr.ledcChannel,
        .timer_sel      = bzr.ledcTimer,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = bzrGpio,
        .duty           = LEDC_DUTY, // Set duty to 50%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // Stop all buzzer output
    ESP_ERROR_CHECK(ledc_stop(LEDC_MODE, bzr.ledcChannel, 0));
    
    ////////////////////////////////////////////////////////////////////////////

    // Configure the hardware timer to check for note transitions
    bzr.noteCheckGroupNum = noteCheckGrpNum;
    bzr.noteCheckTimerNum = noteChkTmrNum;

    // Initialize the timer to check when the note should change
    timer_config_t noteCheckConfig =
    {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    };

    timer_init(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum, &noteCheckConfig);

    // Set initial timer count value
    timer_set_counter_value(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum, 0);

    // Configure the alarm value and the interrupt on alarm.
    timer_set_alarm_value(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum,
        (TIMER_TICKS_PER_SECOND * NOTE_ISR_MS) / 1000); // 5ms timer

    // Enable the interrupt
    timer_enable_intr(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum);

    // Configure the ISR
    timer_isr_callback_add(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum, 
        buzzer_check_next_note_isr, NULL, 0);

    // Don't start the timer until a song is played
    timer_pause(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum);
}

/**
 * @brief Start playing a background music on the buzzer. This has lower priority
 * than sound effects
 * 
 * @param song The song to play as a sequence of notes
 */
void buzzer_play_bgm(const song_t* song)
{
    bzr.bgm.song = song;
    bzr.bgm.note_index = 0;
    bzr.bgm.start_time = esp_timer_get_time();

    // If there is no current SFX
    if(NULL == bzr.sfx.song)
    {
        // Start playing BGM
        playNote(bzr.bgm.song->notes[0].note);
        timer_start(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum);
    }
}

/**
 * @brief Start playing a sound effect on the buzzer. This has higher priority
 * than background music
 * 
 * @param song The song to play as a sequence of notes
 */
void buzzer_play_sfx(const song_t* song)
{
    bzr.sfx.song = song;
    bzr.sfx.note_index = 0;
    bzr.sfx.start_time = esp_timer_get_time();

    // Always play SFX
    playNote(bzr.sfx.song->notes[0].note);
    timer_start(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum);
}

/**
 * @brief Stop the buzzer from playing anything
 */
void buzzer_stop(void)
{
    // Stop the timer to check notes
    timer_pause(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum);

    // Stop the note    
    stopNote();

    // Clear internal variables
    bzr.bgm.note_index = 0;
    bzr.bgm.song = NULL;
    bzr.bgm.start_time = 0;

    bzr.sfx.note_index = 0;
    bzr.sfx.song = NULL;
    bzr.sfx.start_time = 0;
}

/////////////////////////////

/**
 * @brief Start playing a single note on the buzzer.
 * This note will play until stopped.
 * This has IRAM_ATTR because it may be called from an interrupt
 * 
 * @param freq The frequency of the note
 */
void IRAM_ATTR playNote(noteFrequency_t freq)
{
    if(SILENCE == freq)
    {
        stopNote();
        return;
    }
    else
    {
        if(bzr.freq != freq)
        {
            bzr.freq = freq;
            // Set the frequency
            ledc_set_freq(LEDC_MODE, bzr.ledcTimer, freq);
            // Set duty to 50%
            ledc_set_duty(LEDC_MODE, bzr.ledcChannel, LEDC_DUTY);
            // Update duty to start the buzzer
            ledc_update_duty(LEDC_MODE, bzr.ledcChannel);
        }
    }
}

/**
 * @brief Stop playing a single note on the buzzer
 * This has IRAM_ATTR because it may be called from an interrupt
 */
void IRAM_ATTR stopNote(void)
{
    bzr.freq = 0;
    ledc_stop(LEDC_MODE, bzr.ledcChannel, 0);
}

/////////////////////////////

/**
 * @brief Check if there is a new note to play on the buzzer. 
 * This is called periodically in a timer interrupt
 * 
 * This has IRAM_ATTR because it is an interrupt
 */
static bool IRAM_ATTR buzzer_check_next_note_isr(void * ptr)
{
    // Don't do much if muted
    if(bzr.isBgmMuted && bzr.isSfxMuted)
    {
        return false;
    }

    // Get the current time
    int64_t cTime = esp_timer_get_time();

    // Try playing SFX first
    bool sfxIsActive = buzzer_track_check_next_note(&bzr.sfx, true, cTime);
    // Then play BGM if SFX isn't active
    buzzer_track_check_next_note(&bzr.bgm, !sfxIsActive, cTime);
    return false;
}

/**
 * Check a specific track for notes to be played and queue them for playing.
 * This will always advance through notes in a song, even if it's not the active
 * track
 * This has IRAM_ATTR because it is called from buzzer_check_next_note_isr()
 * 
 * @param track The track to advance notes in
 * @param isActive true if this is active and should set a note to be played
 *                 false to just advance notes without playing
 * @return true  if this track is playing a note
 *         false if this track is not playing a note
 */
static bool IRAM_ATTR buzzer_track_check_next_note(buzzerTrack_t* track, bool isActive, int64_t cTime)
{
    // Check if there is a song and there are still notes
    if((NULL != track->song) && (track->note_index < track->song->numNotes))
    {
        // Check if it's time to play the next note
        if (cTime - track->start_time >= (1000 * track->song->notes[track->note_index].timeMs))
        {
            // Move to the next note
            track->note_index++;
            track->start_time = cTime;

            // Loop if we should
            if(track->song->shouldLoop && (track->note_index == track->song->numNotes))
            {
                track->note_index = 0;
            }

            // If there is a note
            if(track->note_index < track->song->numNotes)
            {
                if(isActive)
                {
                    // Set the note to be played
                    playNote(track->song->notes[track->note_index].note);
                }
            }
            else
            {
                if(isActive)
                {
                    // Set the song to stop
                    stopNote();
                }

                // Clear track data
                track->start_time = 0;
                track->note_index = 0;
                track->song = NULL;
                // Track isn't active
                return false;
            }
        }
        // Track is active
        return true;
    }
    // Track isn't active
    return false;
}
