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
#include "esp_log.h"

#include "emu_esp.h"
#include "sound.h"
#include "musical_buzzer.h"
#include "emu_sound.h"
#include "hdw-mic.h"

//==============================================================================
// Defines
//==============================================================================

#define SAMPLING_RATE 8000
#define SSBUF 8192

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
	const song_t *song;
	uint32_t note_index;
	int64_t start_time;
} emu_buzzer_t;

//==============================================================================
// Variables
//==============================================================================

// The sound driver
struct SoundDriver *sounddriver = NULL;

// Input sample circular buffer
uint16_t ssamples[SSBUF]  = {0};
int sshead = 0;
int sstail = 0;
bool adcSampling = false;
pthread_mutex_t micMutex = PTHREAD_MUTEX_INITIALIZER;

// Output buzzer
uint16_t buzzernote = SILENCE;
pthread_mutex_t buzzerMutex = PTHREAD_MUTEX_INITIALIZER;
emu_buzzer_t emuBzr = {0};

//==============================================================================
// Function Prototypes
//==============================================================================

void play_note(void);
void EmuSoundCb(struct SoundDriver *sd, short *in, short *out, int samplesr, int samplesp);

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
void EmuSoundCb(struct SoundDriver *sd UNUSED, short *in, short *out,
				int samplesr, int samplesp)
{
	// If there are samples to read
	if (adcSampling && samplesr)
	{
		pthread_mutex_lock(&micMutex);
		// For each sample
		for (int i = 0; i < samplesr; i++)
		{
			// Read the sample into the circular ssamples[] buffer
			if (sstail != ((sshead + 1) % SSBUF))
			{
#ifndef ANDROID
				// 12 bit sound, unsigned
				uint16_t v = ((in[i] + INT16_MAX) >> 4);
#else
				// Android does something different
				uint16_t v = in[i] * 5;
				if (v > 32767)
				{
					v = 32767;
				}
				else if (v < -32768)
				{
					v = -32768;
				}
#endif

				// Find and print max and min samples for tuning
				// static int32_t vMin = INT32_MAX;
				// static int32_t vMax = INT32_MIN;
				// if(v > vMax)
				// {
				// 	vMax = v;
				// 	printf("Audio %d -> %d\n", vMin, vMax);
				// }
				// if(v < vMin)
				// {
				// 	vMin = v;
				// 	printf("Audio %d -> %d\n", vMin, vMax);
				// }

				ssamples[sshead] = v;
				sshead = (sshead + 1) % SSBUF;
			}
		}
		pthread_mutex_unlock(&micMutex);
	}

	// If this is an output callback, and there are samples to write
	if (samplesp && out)
	{
		// Keep track of our place in the wave
		static float placeInWave = 0;

		// If there is a note to play
		pthread_mutex_lock(&buzzerMutex);
		if (buzzernote)
		{
			// For each sample
			for (int i = 0; i < samplesp; i++)
			{
				// Write the sample
				out[i] = 1024 * sin(placeInWave);
				// Advance the place in the wave
				placeInWave += ((2 * M_PI * buzzernote) / ((float)SAMPLING_RATE));
				// Keep it bound between 0 and 2*PI
				if (placeInWave >= (2 * M_PI))
				{
					placeInWave -= (2 * M_PI);
				}
			}
		}
		else
		{
			// No note to play
			memset(out, 0, samplesp * 2);
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
	if (!sounddriver)
	{
		sounddriver = InitSound(0, EmuSoundCb, SAMPLING_RATE, 1, 1, 256, 0, 0);
	}
	memset(&emuBzr, 0, sizeof(emuBzr));
}

/**
 * @brief Play a song on the emulated buzzer
 *
 * @param song A song to play
 */
void buzzer_play(const song_t *song)
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
	if ((NULL != emuBzr.song) && (emuBzr.note_index < emuBzr.song->numNotes))
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
			if (emuBzr.note_index < emuBzr.song->numNotes)
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
	if (NULL != emuBzr.song)
	{
		const musicalNote_t *notation = &emuBzr.song->notes[emuBzr.note_index];

		if (SILENCE == notation->note)
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

//==============================================================================
// Microphone
//==============================================================================

/**
 * @brief TODO
 *
 * @param adc1_chan_mask
 * @param adc2_chan_mask
 * @param channel
 * @param channel_num
 */
void continuous_adc_init(uint16_t adc1_chan_mask UNUSED, uint16_t adc2_chan_mask UNUSED,
						 adc_channel_t *channel UNUSED, uint8_t channel_num UNUSED)
{
	; // Do Nothing
}

/**
 * @brief TODO
 *
 */
void continuous_adc_deinit(void)
{
	; // Do Nothing
}

/**
 * @brief TODO
 *
 */
void continuous_adc_start(void)
{
	adcSampling = true;
}

/**
 * @brief TODO
 *
 */
void continuous_adc_stop(void)
{
	adcSampling = false;
}

/**
 * @brief TODO
 *
 * @param outSamples
 * @return uint32_t
 */
uint32_t continuous_adc_read(uint16_t *outSamples)
{
	pthread_mutex_lock(&micMutex);
	uint32_t samplesRead = 0;
	while (adcSampling &&
		   (sshead != sstail) &&
		   samplesRead < (BYTES_PER_READ / sizeof(adc_digi_output_data_t)))
	{
		*(outSamples++) = ssamples[sstail];
		sstail = (sstail + 1) % SSBUF;
		samplesRead++;
	}
	pthread_mutex_unlock(&micMutex);
	return samplesRead;
}
