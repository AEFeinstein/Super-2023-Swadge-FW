//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <math.h>
#include <string.h>

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

// Output buzzer
uint16_t buzzernote = SILENCE;
emu_buzzer_t emuBzrBgm = {0};
emu_buzzer_t emuBzrSfx = {0};

// Keep track of muted state
bool emuBgmMuted;
bool emuSfxMuted;

//==============================================================================
// Function Prototypes
//==============================================================================

void EmuSoundCb(struct SoundDriver *sd, short *in, short *out, int samplesr, int samplesp);
bool buzzer_track_check_next_note(emu_buzzer_t * track, bool isActive);

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Deinitialize the sound driver
 */
void deinitSound(void)
{
#if defined(_WIN32)
	CloseSound(NULL);
#else
	CloseSound(sounddriver); // when calling this on Windows, it halts
#endif
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
		}

	// If this is an output callback, and there are samples to write
	if (samplesp && out)
	{
		// Keep track of our place in the wave
		static float placeInWave = 0;

		// If there is a note to play
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
 * @param group_num unused
 * @param timer_num unused
 * @param isBgmMuted true if music is muted, false if it is not
 * @param isSfxMuted true if sfx is muted, false if it is not
 */
void buzzer_init(gpio_num_t bzrGpio,
    ledc_timer_t ledcTimer, ledc_channel_t ledcChannel,
    timer_group_t noteCheckGrpNum, timer_idx_t noteChkTmrNum,
    bool isBgmMuted, bool isSfxMuted)
{
	emuBgmMuted = isBgmMuted;
	emuSfxMuted = isSfxMuted;

	buzzer_stop();
	if (!sounddriver)
	{
		sounddriver = InitSound(0, EmuSoundCb, SAMPLING_RATE, 1, 1, 256, 0, 0);
	}
	memset(&emuBzrBgm, 0, sizeof(emuBzrBgm));
	memset(&emuBzrSfx, 0, sizeof(emuBzrSfx));
}

/**
 * @brief Set the emulated buzzer's bgm mute status
 * 
 * @param isBgmMuted True if background music is muted, false otherwise
 */
void buzzer_set_bgm_is_muted(bool isBgmMuted)
{
    emuBgmMuted = isBgmMuted;
}

/**
 * @brief Set the emulated buzzer's sfx mute status
 * 
 * @param isSfxMuted True if sound effects are muted, false otherwise
 */
void buzzer_set_sfx_is_muted(bool isSfxMuted)
{
    emuSfxMuted = isSfxMuted;
}

/**
 * @brief Play a song on the emulated buzzer
 *
 * @param song A song to play
 */
void buzzer_play_sfx(const song_t *song)
{
	if(emuSfxMuted)
	{
		return;
	}

	// Save the song pointer
	emuBzrSfx.song = song;
	emuBzrSfx.note_index = 0;
	emuBzrSfx.start_time = esp_timer_get_time();

	// Start playing the first note
	playNote(emuBzrSfx.song->notes[0].note);
}

/**
 * @brief Play a song on the emulated buzzer
 *
 * @param song A song to play
 */
void buzzer_play_bgm(const song_t *song)
{
	if(emuBgmMuted)
	{
		return;
	}

	// Save the song pointer
	emuBzrBgm.song = song;
	emuBzrBgm.note_index = 0;
	emuBzrBgm.start_time = esp_timer_get_time();

	if(NULL == emuBzrSfx.song)
	{
		// Start playing the first note
		playNote(emuBzrBgm.song->notes[0].note);
	}
}

/**
 * @brief Advance the notes in a specific track and play them if the track is active
 * 
 * @param track The track to check notes in
 * @param isActive true to play notes, false to just advance them
 * @return true  if this track is playing a note
 *         false if it is not 
 */
bool buzzer_track_check_next_note(emu_buzzer_t * track, bool isActive)
{
	// Check if there is a song and there are still notes
	if ((NULL != track->song) && (track->note_index < track->song->numNotes))
	{
		// Get the current time
		int64_t cTime = esp_timer_get_time();

		// Check if it's time to play the next note
		if (cTime - track->start_time >= (1000 * track->song->notes[track->note_index].timeMs))
		{
			// Move to the next note
			track->note_index++;
			track->start_time = cTime;

			// Loop if requested
			if(track->song->shouldLoop && (track->note_index == track->song->numNotes))
			{
				track->note_index = 0;
			}

			// If there is a note
			if (track->note_index < track->song->numNotes)
			{
				if(isActive)
				{
					// Play the note
					playNote(track->song->notes[track->note_index].note);
				}
			}
			else
			{
				if(isActive)
				{
					// Song is over
					buzzernote = SILENCE;
				}

				track->start_time = 0;
				track->note_index = 0;
				track->song = NULL;
				// Track is inactive
				return false;
			}
		}
		// Track is active
		return true;
	}
	// Track is inactive
	return false;
}

/**
 * @brief Call this periodically to check if the next note in the song should
 * be played
 */
void buzzer_check_next_note(void)
{
	if(emuBgmMuted && emuSfxMuted)
	{
		return;
	}

	bool sfxIsActive = buzzer_track_check_next_note(&emuBzrSfx, true);
	buzzer_track_check_next_note(&emuBzrBgm, !sfxIsActive);
}

/**
 * @brief Stop playing a song on the emulated buzzer
 */
void buzzer_stop(void)
{
	if(emuBgmMuted && emuSfxMuted)
	{
		return;
	}

	emuBzrBgm.song = NULL;
	emuBzrBgm.note_index = 0;
	emuBzrBgm.start_time = 0;

	emuBzrSfx.song = NULL;
	emuBzrSfx.note_index = 0;
	emuBzrSfx.start_time = 0;

	buzzernote = SILENCE;
	playNote(SILENCE);
}

/**
 * @brief Play the current note on the emulated buzzer
 */
void playNote(noteFrequency_t freq)
{
	buzzernote = freq;
}

/**
 * @brief 
 * 
 */
void stopNote(void)
{
	playNote(SILENCE);
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
	uint32_t samplesRead = 0;
	while (adcSampling &&
		   (sshead != sstail) &&
		   samplesRead < (BYTES_PER_READ / sizeof(adc_digi_output_data_t)))
	{
		*(outSamples++) = ssamples[sstail];
		sstail = (sstail + 1) % SSBUF;
		samplesRead++;
	}
	return samplesRead;
}

void oneshot_adc_init(adc_unit_t adcUnit, adc1_channel_t adcChan)
{
	WARN_UNIMPLEMENTED();
}

uint32_t oneshot_adc_read(void)
{
	WARN_UNIMPLEMENTED();
	return 0;
}
