#include "esp_log.h"

#include "esp_emu.h"
#include "emu_main.h"

#include "QMA6981.h"
#include "esp_temperature_sensor.h"
#include "touch_sensor.h"
#include "btn.h"

//==============================================================================
// Buttons
//==============================================================================

/**
 * @brief Set up the keyboard to act as input buttons
 *
 * @param numButtons The number of buttons to initialize
 * @param ... A list of GPIOs, which are ignored
 */
void initButtons(uint8_t numButtons, ...)
{
    // The order in which keys are initialized
    // Note that the actuall number of buttons initialized may be less than this
    char keyOrder[] = {'w', 's', 'a', 'd', 'i', 'k', 'j', 'l'};
    setInputKeys(numButtons, keyOrder);
}

/**
 * @brief Do nothing
 *
 */
void deinitializeButtons(void)
{
    ;
}

/**
 * @brief Check the input queue for any events
 *
 * @return true if there was an event, false if there wasn't
 */
bool checkButtonQueue(buttonEvt_t* evt)
{
    return checkInputKeys(evt);
}

//==============================================================================
// Touch Sensor
//==============================================================================

/**
 * @brief TODO
 *
 * @param touchPadSensitivity
 * @param denoiseEnable
 * @param numTouchPads
 * @param ...
 */
void initTouchSensor(float touchPadSensitivity, bool denoiseEnable,
    uint8_t numTouchPads, ...)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool checkTouchSensor(touch_event_t * evt)
{
    WARN_UNIMPLEMENTED();
    return false;
}

//==============================================================================
// Temperature Sensor
//==============================================================================

/**
 * @brief TODO
 *
 */
void initTemperatureSensor(void)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief TODO
 *
 * @return float
 */
float readTemperatureSensor(void)
{
    WARN_UNIMPLEMENTED();
    return 0;
}

//==============================================================================
// Accelerometer
//==============================================================================

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool QMA6981_setup(void)
{
    WARN_UNIMPLEMENTED();
    return false;
}

/**
 * @brief TODO
 *
 * @param currentAccel
 */
void QMA6981_poll(accel_t* currentAccel)
{
    WARN_UNIMPLEMENTED();
}
