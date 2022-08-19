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

//==============================================================================
// Functions
//==============================================================================

/**
 * TODO doc
 *
 * @return true
 * @return false
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
 * TODO doc
 *
 * @param isMuted
 * @return
 */
bool setIsMuted(bool isMuted)
{
    // Write the value
    return writeNvs32(KEY_MUTE, isMuted);
}

/**
 * TODO doc
 *
 * @return int32_t
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
 * TODO doc
 *
 * @param brightness
 * @return
 */
bool setBrightness(int32_t brightness)
{
    // Write the value
    return writeNvs32(KEY_BRIGHT, brightness);
}

/**
 * TODO doc
 *
 * @return int32_t
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
 * TODO doc
 *
 * @param micVolume
 * @return
 */
bool setMicVolume(int32_t micVolume)
{
    // Write the value
    return writeNvs32(KEY_MIC, micVolume);
}

/**
 * TODO doc
 *
 * @return uint8_t
 */
uint8_t getMicAmplitude(void)
{
    return micVols[getMicVolume()];
}
