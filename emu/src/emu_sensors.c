#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "list.h"

#include "esp_log.h"

#include "swadge_esp32.h"
#include "esp_emu.h"
#include "emu_main.h"

#include "QMA6981.h"
#include "esp_temperature_sensor.h"
#include "touch_sensor.h"
#include "btn.h"

#include "emu_sensors.h"

// Input queues for buttons
char inputKeys[32];
uint32_t buttonState = 0;
list_t * buttonQueue;
pthread_mutex_t buttonQueueMutex = PTHREAD_MUTEX_INITIALIZER;


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
    memcpy(inputKeys, keyOrder, numButtons);
	buttonState = 0;
	pthread_mutex_lock(&buttonQueueMutex);
	buttonQueue = list_new();
	pthread_mutex_unlock(&buttonQueueMutex);
}

/**
 * @brief Do nothing
 *
 */
void deinitButtons(void)
{
	pthread_mutex_lock(&buttonQueueMutex);
	free(buttonQueue);
	pthread_mutex_unlock(&buttonQueueMutex);
}

/**
 * @brief Check the input queue for any events
 *
 * @return true if there was an event, false if there wasn't
 */
bool checkButtonQueue(buttonEvt_t* evt)
{
	// Check the queue, guarded by a mutex
	pthread_mutex_lock(&buttonQueueMutex);
	list_node_t * node = list_lpop(buttonQueue);
	pthread_mutex_unlock(&buttonQueueMutex);

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
 * @brief TODO
 * 
 * @param keycode 
 * @param bDown 
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
			evt->button = idx;
			evt->down = bDown;
			evt->state = buttonState;

			// Add the event to the list, guarded by a mutex
			pthread_mutex_lock(&buttonQueueMutex);
			list_node_t * buttonNode = list_node_new(evt);
			list_rpush(buttonQueue, buttonNode); 
			pthread_mutex_unlock(&buttonQueueMutex);
			
			break;
		}
	}
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
