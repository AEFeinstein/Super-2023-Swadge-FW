//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <string.h>

#include "list.h"

#include "esp_log.h"
#include "esp_err.h"

#include "swadge_esp32.h"
#include "emu_esp.h"

#include "QMA6981.h"
#include "qma7981.h"
#include "esp_temperature_sensor.h"
#include "touch_sensor.h"
#include "btn.h"

#include "emu_sensors.h"

//==============================================================================
// Variables
//==============================================================================

// Input queues for buttons
char inputKeys[32];
uint32_t buttonState = 0;
list_t * buttonQueue;

char touchKeys[32];
uint32_t touchState = 0;
list_t * touchQueue;
int32_t lastTouchLoc = 0;

//==============================================================================
// Buttons
//==============================================================================

/**
 * @brief Set up the keyboard to act as input buttons
 *
 * @param group_num The timer group number to poll GPIOs with
 * @param timer_num The timer number to poll GPIOs with
 * @param numButtons The number of buttons to initialize
 * @param ... A list of GPIOs, which are ignored
 */
void initButtons(timer_group_t group_num, timer_idx_t timer_num, uint8_t numButtons, ...)
{
    // The order in which keys are initialized
    // Note that the actuall number of buttons initialized may be less than this
    char keyOrder[] = {'w', 's', 'a', 'd', 'l', 'k', 'o', 'i',
	                   't', 'g', 'f', 'h', 'm', 'n', 'r', 'y'};
    memcpy(inputKeys, keyOrder, (sizeof(keyOrder) / sizeof(keyOrder[0])));
	buttonState = 0;
	buttonQueue = list_new();
}

/**
 * @brief Free memory used for button handling
 */
void deinitButtons(void)
{
	free(buttonQueue);
	free(touchQueue);
}

/**
 * @brief Check the input queue for any events
 *
 * @return true if there was an event, false if there wasn't
 */
bool checkButtonQueue(buttonEvt_t* evt)
{
	// Check the queue
	list_node_t * node = list_lpop(buttonQueue);

	// No events
	if(NULL == node)
	{
		memset(evt, 0, sizeof(buttonEvt_t));
		return false;
	}
	else
	{
		// Copy the event to the arg
		memcpy(evt, node->val, sizeof(buttonEvt_t));
		// Free everything
		free(node->val);
		free(node);
		// Return that an event occurred
		return true;
	}
}

/**
 * @brief This handles key events from rawdraw
 *
 * @param keycode The key that was pressed or released
 * @param bDown true if the key was pressed, false if it was released
 */
void emuSensorHandleKey( int keycode, int bDown )
{
    // Check keycode against initialized keys
	for(uint8_t idx = 0; idx < ARRAY_SIZE(inputKeys); idx++)
	{
		// If this matches
		if(keycode == inputKeys[idx])
		{
			// Set or clear the button
			if(bDown)
			{
				// Check if button was already pressed
				if(buttonState & (1 << idx))
				{
					// It was, just return
					return;
				}
				else
				{
					// It wasn't, set it!
					buttonState |= (1 << idx);
				}
			}
			else
			{
				// Check if button was already released
				if(0 == (buttonState & (1 << idx)))
				{
					// It was, just return
					return;
				}
				else
				{
					// It wasn't, clear it!
					buttonState &= ~(1 << idx);
				}
			}

			// Create a new event
			buttonEvt_t * evt = malloc(sizeof(buttonEvt_t));
			evt->button = (1 << idx);
			evt->down = bDown;
			evt->state = buttonState;

			// Add the event to the list
			list_node_t * buttonNode = list_node_new(evt);
			list_rpush(buttonQueue, buttonNode);
			break;
		}
	}

	//
	// Touch Sensor
	//

	// Check keycode against initialized keys
	for(uint8_t idx = 0; idx < ARRAY_SIZE(touchKeys); idx++)
	{
		// If this matches
		if(keycode == touchKeys[idx])
		{
			// Set or clear the button
			if(bDown)
			{
				// Check if button was already pressed
				if(touchState & (1 << idx))
				{
					// It was, just return
					return;
				}
				else
				{
					// It wasn't, set it!
					touchState |= (1 << idx);
				}
			}
			else
			{
				// Check if button was already released
				if(0 == (touchState & (1 << idx)))
				{
					// It was, just return
					return;
				}
				else
				{
					// It wasn't, clear it!
					touchState &= ~(1 << idx);
				}
			}

			// Create a new event
			touch_event_t * evt = malloc(sizeof(touch_event_t));
			evt->state = touchState;
			evt->pad = idx;
			evt->down = bDown;

			/* LUT the location */
			const uint8_t touchLoc[] =
			{
				128, // 00000
				0,   // 00001
				64,  // 00010
				32,  // 00011
				128, // 00100
				64,  // 00101
				96,  // 00110
				64,  // 00111
				192, // 01000
				96,  // 01001
				128, // 01010
				85,  // 01011
				160, // 01100
				106, // 01101
				128, // 01110
				96,  // 01111
				255, // 10000
				128, // 10001
				160, // 10010
				106, // 10011
				192, // 10100
				128, // 10101
				149, // 10110
				112, // 10111
				224, // 11000
				149, // 11001
				170, // 11010
				128, // 11011
				192, // 11100
				144, // 11101
				160, // 11110
				128, // 11111
			};
			evt->position = touchLoc[touchState];
			lastTouchLoc = touchLoc[touchState];

			// Add the event to the list
			list_node_t * touchNode = list_node_new(evt);
			list_rpush(touchQueue, touchNode);
			break;
		}
	}
}

//==============================================================================
// Touch Sensor
//==============================================================================

/**
 * @brief Initialize touchpad sensors
 *
 * @param touchPadSensitivity The sensitivity to set for these touchpads
 * @param denoiseEnable true to denoise the input, false to use it raw
 * @param numTouchPads The number of touchpads to initialize
 * @param ... A list of touchpads to initialize (touch_pad_t)
 */
void initTouchSensor(float touchPadSensitivity UNUSED, bool denoiseEnable UNUSED,
    uint8_t numTouchPads UNUSED, ...)
{
    // The order in which keys are initialized
    // Note that the actuall number of buttons initialized may be less than this
    char touchOrder[] = {'1', '2', '3', '4', '5'};
    memcpy(touchKeys, touchOrder, (sizeof(touchOrder) / sizeof(touchOrder[0])));
	touchState= 0;
	touchQueue = list_new();
}

/**
 * @brief Call this function periodically to check the touch pad interrupt queue
 *
 * @param evt Return a touch event through this arg if there was one
 * @return true if there was a touch event, false if there was not
 */
bool checkTouchSensor(touch_event_t * evt UNUSED)
{
	// Check the queue
	list_node_t * node = list_lpop(touchQueue);

	// No events
	if(NULL == node)
	{
		memset(evt, 0, sizeof(touch_event_t));
		return false;
	}
	else
	{
		// Copy the event to the arg
		memcpy(evt, node->val, sizeof(touch_event_t));
		// Free everything
		free(node->val);
		free(node);
		// Return that an event occurred
		return true;
	}
}

int getTouchRawValues( uint32_t * rawvalues, int maxPads )
{
	for(int i = 0; i < maxPads; i++)
	{
		rawvalues[i] = 0;
	}
	return 0;
}

int getBaseTouchVals( int32_t * data, int count )
{
	for(int i = 0; i < count; i++)
	{
		data[i] = 0;
	}
	return 0;
}

int getTouchCentroid( int32_t * centerVal, int32_t * intensityVal )
{
	*centerVal = lastTouchLoc * 4;
	*intensityVal = 1;
	return (touchState ? true : false);
}

//==============================================================================
// Temperature Sensor
//==============================================================================

/**
 * @brief Initialize the ESP's onboard temperature sensor
 */
void initTemperatureSensor(void)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Get a temperature reading from the ESP's onboard temperature sensor
 *
 * @return A floating point temperature
 */
float readTemperatureSensor(void)
{
    WARN_UNIMPLEMENTED();
    return -274; // Impossible! One degree below absolute zero
}

//==============================================================================
// Accelerometer
//==============================================================================

/**
 * @brief Initialize the QMA6981 and start it going
 *
 * @return true if initialization succeeded, false if it failed
 */
bool QMA6981_setup(void)
{
    WARN_UNIMPLEMENTED();
    return true;
}

/**
 * @brief Poll the QMA6981 for the current acceleration value
 *
 * @param currentAccel A pointer where the acceleration data will be stored
 */
void QMA6981_poll(accel_t* currentAccel UNUSED)
{
	currentAccel->x = 0;
	currentAccel->y = 0;
	currentAccel->z = 0;
    WARN_UNIMPLEMENTED();
}

esp_err_t qma7981_init(void)
{
    WARN_UNIMPLEMENTED();
	return ESP_OK;
}

esp_err_t qma7981_get_acce_int(int16_t *x, int16_t *y, int16_t *z)
{
    WARN_UNIMPLEMENTED();
	*x = 4095;
	*y = (4095 * 2) / 3;
	*z = 4095 / 3;
	return ESP_OK;	
}
