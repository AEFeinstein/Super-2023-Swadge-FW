//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <pthread.h>

#include "gpio_types.h"
#include "driver/rmt.h"
#include "esp_timer.h"

#include "esp_emu.h"
#include "sound.h"
#include "musical_buzzer.h"
#include "emu_sound.h"

//==============================================================================
// Defines
//==============================================================================

#define SAMPLING_RATE 16000
#define SSBUF 8192

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    const song_t* song;
    uint32_t note_index;
    int64_t start_time;
} emu_buzzer_t;

//==============================================================================
// Variables
//==============================================================================

// The sound driver
struct SoundDriver * sounddriver;

// Input sample circular buffer
uint8_t ssamples[SSBUF];
int sshead;
int sstail;

// Output buzzer
uint16_t buzzernote;
pthread_mutex_t buzzerMutex = PTHREAD_MUTEX_INITIALIZER;
emu_buzzer_t emuBzr;

//==============================================================================
// Function Prototypes
//==============================================================================

void play_note(void);
void EmuSoundCb( struct SoundDriver* sd, short* in, short* out, int samplesr, int samplesp );

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Deinitialize the sound driver
 */
void deinitSound(void)
{
    // CloseSound(sounddriver); TODO when calling this on Windows, it halts
    CloseSound(NULL);
}

/**
 * @brief Callback for sound events, both input and output
 * 
 * @param sd The sound driver
 * @param in A pointer to read samples from. May be NULL
 * @param out A pointer to write samples to. May be NULL
 * @param samplesr The number of samples to read
 * @param samplesp The number of samples to write
 */
void EmuSoundCb( struct SoundDriver* sd UNUSED, short* in, short* out,
    int samplesr, int samplesp )
{
    // If there are samples to read
    if( samplesr )
    {
        // For each sample
        for( int i = 0; i < samplesr; i++ )
        {
            // Read the sample into the circular ssamples[] buffer
            if( sstail != (( sshead + 1 ) % SSBUF) )
            {
                int v = in[i];
#ifdef ANDROID
                v *= 5;
                if( v > 32767 )
                {
                    v = 32767;
                }
                else if( v < -32768 )
                {
                    v = -32768;
                }
#endif
                ssamples[sshead] = (v / 256) + 128;
                sshead = ( sshead + 1 ) % SSBUF;
            }
        }
    }

    // If this is an output callback, and there are samples to write
    if( samplesp && out )
    {
        // Keep track of our place in the wave
        static float placeInWave = 0;

        // If there is a note to play
        pthread_mutex_lock(&buzzerMutex);
        if ( buzzernote )
        {
            // For each sample
            for( int i = 0; i < samplesp; i++ )
            {
                // Write the sample
                out[i] = 16384 * sin(placeInWave);
                // Advance the place in the wave
                placeInWave += ((2 * M_PI * buzzernote) / ((float)SAMPLING_RATE));
                // Keep it bound between 0 and 2*PI
                if(placeInWave >= (2 * M_PI))
                {
                    placeInWave -= (2 * M_PI);
                }
            }
        }
        else
        {
            // No note to play
            memset( out, 0, samplesp * 2 );
            placeInWave = 0;
        }
        pthread_mutex_unlock(&buzzerMutex);
    }
}

//==============================================================================
// Buzzer
//==============================================================================

/**
 * Initialize the emulated buzzer
 *
 * @param gpio unused
 * @param rmt unused
 */
void buzzer_init(gpio_num_t gpio UNUSED, rmt_channel_t rmt UNUSED)
{
    buzzer_stop();
    if( !sounddriver )
    {
        sounddriver = InitSound( 0, EmuSoundCb, SAMPLING_RATE, 1, 1, 256, 0, 0 );
    }
    memset(&emuBzr, 0, sizeof(emuBzr));
}

/**
 * @brief Play a song on the emulated buzzer
 *
 * @param song A song to play
 */
void buzzer_play(const song_t* song)
{
    // Stop everything
    buzzer_stop();

    // Save the song pointer
    emuBzr.song = song;
    emuBzr.note_index = 0;
    emuBzr.start_time = esp_timer_get_time();

    // Start playing the first note
    play_note();
}

/**
 * @brief Call this periodically to check if the next note in the song should
 * be played
 */
void buzzer_check_next_note(void)
{
    // Check if there is a song and there are still notes
    if((NULL != emuBzr.song) && (emuBzr.note_index < emuBzr.song->numNotes))
    {
        // Get the current time
        int64_t cTime = esp_timer_get_time();
        
        // Check if it's time to play the next note
        if (cTime - emuBzr.start_time >= (1000 * emuBzr.song->notes[emuBzr.note_index].timeMs))
        {
            // Move to the next note
            emuBzr.note_index++;
            emuBzr.start_time = cTime;

            // If there is a note
            if(emuBzr.note_index < emuBzr.song->numNotes)
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

/**
 * @brief Stop playing a song on the emulated buzzer
 */
void buzzer_stop(void)
{
    emuBzr.song = NULL;
    emuBzr.note_index = 0;
    emuBzr.start_time = 0;

    pthread_mutex_lock(&buzzerMutex);
    buzzernote = SILENCE;
    pthread_mutex_unlock(&buzzerMutex);

    play_note();
}

/**
 * @brief Play the current note on the emulated buzzer
 */
void play_note(void)
{
    if(NULL != emuBzr.song)
    {
        const musicalNote_t* notation = &emuBzr.song->notes[emuBzr.note_index];

        if(SILENCE == notation->note)
        {
            buzzer_stop();
        }
        else
        {
            pthread_mutex_lock(&buzzerMutex);
            buzzernote = notation->note;
            pthread_mutex_unlock(&buzzerMutex);
        }
    }
    else
    {
        pthread_mutex_lock(&buzzerMutex);
        buzzernote = SILENCE;
        pthread_mutex_unlock(&buzzerMutex);
    }
}
