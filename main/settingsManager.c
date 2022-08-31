//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>

#include "nvs_manager.h"
#include "settingsManager.h"

//==============================================================================
// Variables
//==============================================================================

const char KEY_MUTE[] = "mute";
const char KEY_BRIGHT[] = "bright";
const char KEY_MIC[] = "mic";
const char KEY_QJ_HS[] = "qj";

//==============================================================================
// Functions
//==============================================================================

/**
 * @return true if the buzzer is muted, false if it is not
 */
bool getIsMuted(void)
{
    int32_t muted = false;
    // Try reading the value
    if(false == readNvs32(KEY_MUTE, &muted))
    {
        // Value didn't exist, so write the default
        setIsMuted(muted);
    }
    // Return the read value
    return (bool)muted;
}

/**
 * Set if the buzzer is muted or not
 *
 * @param isMuted true to mute the buzzer, false to turn it on
 * @return true if the setting was saved, false if it was not
 */
bool setIsMuted(bool isMuted)
{
    // Write the value
    return writeNvs32(KEY_MUTE, isMuted);
}

/**
 * @return The brightness level for the TFT, 0-9
 */
int32_t getBrightness(void)
{
    int32_t brightness = 5;
    // Try reading the value
    if(false == readNvs32(KEY_BRIGHT, &brightness))
    {
        // Value didn't exist, so write the default
        setBrightness(brightness);
    }
    // Return the read value
    return brightness;
}

/**
 * Set the brightness level for the TFT
 *
 * @param brightness The brightness, 0-9
 * @return true if the setting was saved, false if it was not
 */
bool setBrightness(int32_t brightness)
{
    // Bound the value, just in case
    if(brightness < 0)
    {
        brightness = 0;
    }
    else if(brightness > 9)
    {
        brightness = 9;
    }

    // Write the value
    return writeNvs32(KEY_BRIGHT, brightness);
}

/**
 * @return The volume setting for the microphone, 0-9
 */
int32_t getMicVolume(void)
{
    int32_t micVolume = 5;
    // Try reading the value
    if(false == readNvs32(KEY_MIC, &micVolume))
    {
        // Value didn't exist, so write the default
        setMicVolume(micVolume);
    }
    // Return the read value
    return micVolume;
}

/**
 * Set the microphone volume
 *
 * @param micVolume The volume setting for the microphone, 0-9
 * @return true if the setting was saved, false if it was not
 */
bool setMicVolume(int32_t micVolume)
{
    // Bound the value, just in case
    if(micVolume < 0)
    {
        micVolume = 0;
    }
    else if(micVolume > 9)
    {
        micVolume = 9;
    }

    // Write the value
    return writeNvs32(KEY_MIC, micVolume);
}

/**
 * @return The amplitude for the microphone when reading samples
 */
uint8_t getMicAmplitude(void)
{
    const uint8_t micVols[] =
    {
        26,
        51,
        77,
        102,
        128,
        153,
        179,
        204,
        230,
        255
    };
    return micVols[getMicVolume()];
}

/**
 * @return the QJumper High Score
 */
uint32_t getQJumperHighScore(void)
{
    int32_t highScore = 0;
    // Try reading the value
    if(false == readNvs32(KEY_QJ_HS, &highScore))
    {
        // Value didn't exist, so write the default
        setQJumperHighScore(highScore);
    }
    // Return the read value
    return highScore;
}

/**
 * Set the new QJumper high score
 *
 * @param highScore the new high score
 * @return true if the setting was saved, false if it was not
 */
bool setQJumperHighScore(uint32_t highScore)
{
    // Write the value
    return writeNvs32(KEY_QJ_HS, highScore);
}
