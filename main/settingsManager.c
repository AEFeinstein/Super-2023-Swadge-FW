//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>

#include "nvs_manager.h"
#include "settingsManager.h"
#include "led_util.h"
#include "hdw-tft.h"

//==============================================================================
// Defines
//==============================================================================

#define MAX_LED_BRIGHTNESS 7
#define MAX_TFT_BRIGHTNESS 7
#define MAX_MIC_GAIN       7

//==============================================================================
// Variables
//==============================================================================

const char KEY_MUTE[]       = "mute";
const char KEY_TFT_BRIGHT[] = "bright";
const char KEY_MIC[]        = "mic";
const char KEY_LED_BRIGHT[] = "led";
const char KEY_CC_MODE[]    = "ccm";
const char KEY_QJ_HS[]      = "qj";
const char KEY_TEST[]       = "test";

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
 * @return The tftBrightness level for the TFT, 0-9
 */
int32_t getTftBrightness(void)
{
    int32_t tftBrightness = 5;
    // Try reading the value
    if(false == readNvs32(KEY_TFT_BRIGHT, &tftBrightness))
    {
        // Value didn't exist, so write the default
        writeNvs32(KEY_TFT_BRIGHT, tftBrightness);
    }
    // Return the read value
    return tftBrightness;
}

/**
 * Increment the brightness level for the TFT
 *
 * @return true if the setting was saved, false if it was not
 */
bool incTftBrightness(void)
{
    // Increment the value
    uint8_t brightness = (getTftBrightness() + 1) % (MAX_TFT_BRIGHTNESS + 1);

    // Write the value
    bool retVal = writeNvs32(KEY_TFT_BRIGHT, brightness);

    // Set the brightness
    setTFTBacklight(getTftIntensity());

    return retVal;
}

/**
 * Decrement the brightness level for the TFT
 *
 * @return true if the setting was saved, false if it was not
 */
bool decTftBrightness(void)
{
    // Decrement the value
    uint8_t brightness = getTftBrightness();
    if(brightness == 0)
    {
        brightness = MAX_TFT_BRIGHTNESS;
    }
    else
    {
        brightness--;
    }

    // Write the value
    bool retVal = writeNvs32(KEY_TFT_BRIGHT, brightness);

    // Set the brightness
    setTFTBacklight(getTftIntensity());

    return retVal;
}

/**
 * @brief Get the Tft Intensity, passed into setTFTBacklight()
 *
 * @return The TFT intensity
 */
uint8_t getTftIntensity(void)
{
    return (CONFIG_TFT_MIN_BRIGHTNESS + (((CONFIG_TFT_MAX_BRIGHTNESS - CONFIG_TFT_MIN_BRIGHTNESS) * getTftBrightness()) /
                                         MAX_TFT_BRIGHTNESS));
}

/**
 * @return The Brightness level for the LED, 0-7
 */
int32_t getLedBrightness(void)
{
    int32_t ledBrightness = 5;
    // Try reading the value
    if(false == readNvs32(KEY_LED_BRIGHT, &ledBrightness))
    {
        // Value didn't exist, so write the default
        writeNvs32(KEY_LED_BRIGHT, ledBrightness);
    }
    // Return the read value
    return ledBrightness;
}

/**
 * Increment the brightness level for the LED
 *
 * @return true if the setting was saved, false if it was not
 */
bool incLedBrightness(void)
{
    // Increment the value
    uint8_t brightness = (getLedBrightness() + 1) % (MAX_LED_BRIGHTNESS + 1);
    // Adjust the LEDs
    setLedBrightness(brightness);
    // Write the value
    return writeNvs32(KEY_LED_BRIGHT, brightness);
}

/**
 * Decrement the brightness level for the LED
 *
 * @return true if the setting was saved, false if it was not
 */
bool decLedBrightness(void)
{
    // Decrement the value
    uint8_t brightness = getLedBrightness();
    if(brightness == 0)
    {
        brightness = MAX_LED_BRIGHTNESS;
    }
    else
    {
        brightness--;
    }
    // Adjust the LEDs
    setLedBrightness(brightness);
    // Write the value
    return writeNvs32(KEY_LED_BRIGHT, brightness);
}

/**
 * @return The gain setting for the microphone, 0-9
 */
int32_t getMicGain(void)
{
    int32_t micGain = MAX_MIC_GAIN;
    // Try reading the value
    if(false == readNvs32(KEY_MIC, &micGain))
    {
        // Value didn't exist, so write the default
        writeNvs32(KEY_MIC, micGain);
    }
    // Return the read value
    return micGain;
}

/**
 * Increment the microphone gain setting
 *
 * @return true if the setting was written, false if it was not
 */
bool incMicGain(void)
{
    // Increment the value
    uint8_t newGain = (getMicGain() + 1) % (MAX_MIC_GAIN + 1);
    // Write the value
    return writeNvs32(KEY_MIC, newGain);
}

/**
 * Decrement the microphone gain setting
 *
 * @return true if the setting was written, false if it was not
 */
bool decMicGain(void)
{
    // Decrement the value
    uint8_t newGain = getMicGain();
    if(newGain == 0)
    {
        newGain = MAX_MIC_GAIN;
    }
    else
    {
        newGain--;
    }
    // Write the value
    return writeNvs32(KEY_MIC, newGain);
}

/**
 * @return The amplitude for the microphone when reading samples
 */
uint16_t getMicAmplitude(void)
{
    const uint16_t micVols[] =
    {
        32,
        64,
        96,
        128,
        160,
        192,
        224,
        256,
    };
    return micVols[getMicGain()];
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

/**
 * @return true if the test mode passed
 */
bool getTestModePassed(void)
{
    int32_t pass = false;
    // Try reading the value
    if(false == readNvs32(KEY_TEST, &pass))
    {
        // Value didn't exist, so write the default
        setTestModePassed(false);
    }
    // Return the read value
    return (bool)pass;
}

/**
 * Set the new test mode pass status
 *
 * @param status true if the test passed, false if it did not
 * @return true if the setting was saved, false if it was not
 */
bool setTestModePassed(bool status)
{
    // Write the value
    return writeNvs32(KEY_TEST, status);
}
