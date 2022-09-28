//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <string.h>
#include <pthread.h>

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
pthread_mutex_t buttonQueueMutex = PTHREAD_MUTEX_INITIALIZER;

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
    char keyOrder[] = {'w', 's', 'a', 'd', 'l', 'k', 'o', 'i'};
    memcpy(inputKeys, keyOrder, numButtons);
	buttonState = 0;
	pthread_mutex_lock(&buttonQueueMutex);
	buttonQueue = list_new();
	pthread_mutex_unlock(&buttonQueueMutex);
}

/**
 * @brief Free memory used for button handling
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
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Call this function periodically to check the touch pad interrupt queue
 *
 * @param evt Return a touch event through this arg if there was one
 * @return true if there was a touch event, false if there was not
 */
bool checkTouchSensor(touch_event_t * evt UNUSED)
{
    WARN_UNIMPLEMENTED();
    return false;
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
