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
const char KEY_CC_MODE[] = "ccm";

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
 * @return The colorchordMode level for the TFT, LINEAR_LEDS or ALL_SAME_LEDS
 */
colorchordMode_t getColorchordMode(void)
{
    colorchordMode_t colorchordMode = ALL_SAME_LEDS;
    // Try reading the value
    if(false == readNvs32(KEY_CC_MODE, (int32_t*)&colorchordMode))
    {
        // Value didn't exist, so write the default
        setColorchordMode(colorchordMode);
    }
    // Return the read value
    return colorchordMode;
}

/**
 * Set the colorchordMode
 *
 * @param colorchordMode The colorchordMode, ALL_SAME_LEDS or LINEAR_LEDS
 * @return true if the setting was saved, false if it was not
 */
bool setColorchordMode(colorchordMode_t colorchordMode)
{
    // Bound the value, just in case
    if((ALL_SAME_LEDS != colorchordMode) && (LINEAR_LEDS != colorchordMode))
    {
        colorchordMode = ALL_SAME_LEDS;
    }

    // Write the value
    return writeNvs32(KEY_CC_MODE, colorchordMode);
}
