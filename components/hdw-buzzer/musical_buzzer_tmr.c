//==============================================================================
// Includes
//==============================================================================

// #include "esp_timer.h"
#include "driver/timer.h"
#include "esp_log.h"

#include "musical_buzzer.h"

//==============================================================================
// Defines
//==============================================================================

#define TIMER_DIVIDER (16)  //  Hardware timer clock divider
#define TIMER_TICKS_PER_SECOND   (TIMER_BASE_CLK / TIMER_DIVIDER)  // cycles per second

#define TIMER_DIVIDER_DRIVE (2)  //  Hardware timer clock divider
#define TIMER_DRIVE_TICKS_PER_SECOND   (TIMER_BASE_CLK / TIMER_DIVIDER_DRIVE)  // cycles per second

#define NOTE_ISR_MS   5

typedef struct
{
    const song_t* song;
    uint32_t note_index;
    int64_t start_time;
} buzzerTrack_t;

#define DBG_GPIO

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
    timer_group_t bzrGroupNum;
    timer_idx_t bzrTimerNum;
} bzr;

//==============================================================================
// Functions Prototypes
//==============================================================================

static bool buzzer_check_next_note_isr(void * ptr);
static bool buzzer_track_check_next_note(buzzerTrack_t* track, bool isActive, int64_t cTime);
static bool toggle_buzzer_isr(void * arg);

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief TODO
 * 
 * @param bzrGpio 
 * @param noteCheckGrpNum 
 * @param noteChkTmrNum 
 * @param bzrDriveGrpNum 
 * @param bzrDriveTmrNum 
 * @param isBgmMuted 
 * @param isSfxMuted 
 */
void buzzer_init(gpio_num_t bzrGpio,
    timer_group_t noteCheckGrpNum, timer_idx_t noteChkTmrNum,
    timer_group_t bzrDriveGrpNum, timer_idx_t bzrDriveTmrNum,
    bool isBgmMuted, bool isSfxMuted)
{
    // Configure the GPIO
    gpio_config_t bzrConf =
    {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = false,
        .pull_down_en = false,
        .pin_bit_mask = 1ULL << bzrGpio,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&bzrConf);
    bzr.gpio = bzrGpio;
    bzr.gpioLevel = 0;
    gpio_set_level(bzr.gpio, bzr.gpioLevel);

#ifdef DBG_GPIO
    // Configure the GPIO
    gpio_config_t dbgConf =
    {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = false,
        .pull_down_en = false,
        .pin_bit_mask = 1ULL << GPIO_NUM_17,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&dbgConf);
    gpio_set_level(GPIO_NUM_17, 0);
#endif

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
    timer_isr_callback_add(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum, buzzer_check_next_note_isr, NULL, 0);

    // Start the timer
    timer_start(bzr.noteCheckGroupNum, bzr.noteCheckTimerNum);

    // Configure the hardware timer to drive the buzzer
    bzr.bzrGroupNum = bzrDriveGrpNum;
    bzr.bzrTimerNum = bzrDriveTmrNum;

    // Initialize the timer to drive the buzzer
    timer_config_t bzrDriveConfig =
    {
        .divider = TIMER_DIVIDER_DRIVE,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    }; // default clock source is APB

    timer_init(bzr.bzrGroupNum, bzr.bzrTimerNum, &bzrDriveConfig);

    // Configure the ISR
    timer_isr_callback_add(bzr.bzrGroupNum, bzr.bzrTimerNum, toggle_buzzer_isr, NULL, 0);

    // Enable the interrupt
    timer_enable_intr(bzr.bzrGroupNum, bzr.bzrTimerNum);

    // Do not start the timer
    timer_pause(bzr.bzrGroupNum, bzr.bzrTimerNum);
}

/**
 * @brief TODO
 * 
 * @param song 
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
 * @brief TODO
 * 
 * @param song 
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
 * @brief TODO
 * 
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
 * @brief TODO
 * 
 * @param freq cycles per second
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
        // Configure the alarm value and the interrupt on alarm.
        timer_set_alarm_value(bzr.bzrGroupNum, bzr.bzrTimerNum, TIMER_DRIVE_TICKS_PER_SECOND / (2 * freq));

        // Set initial timer count value
        // timer_set_counter_value(bzr.bzrGroupNum, bzr.bzrTimerNum, 0);

        // Start the timer
        timer_start(bzr.bzrGroupNum, bzr.bzrTimerNum);
    }
}

/**
 * @brief TODO
 * 
 */
void IRAM_ATTR stopNote(void)
{
    timer_pause(bzr.bzrGroupNum, bzr.bzrTimerNum);
    // bzr.gpioLevel = 0;
    // gpio_set_level(bzr.gpio, bzr.gpioLevel);
}

/////////////////////////////

/**
 * @brief TODO
 * 
 * @param arg 
 * @return true 
 * @return false 
 */
static bool IRAM_ATTR toggle_buzzer_isr(void * arg)
{
    bzr.gpioLevel = bzr.gpioLevel ? 0 : 1;
    gpio_set_level(bzr.gpio, bzr.gpioLevel);
#ifdef DBG_GPIO
    gpio_set_level(GPIO_NUM_17, bzr.gpioLevel);
#endif
    return false;
}

/**
 * @brief Check if there is a new note to play on the buzzer. 
 * This is called periodically in a timer interrupt
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
