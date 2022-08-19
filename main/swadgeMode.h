#ifndef _SWADGE_MODE_H_
#define _SWADGE_MODE_H_

#include <stdint.h>

#include <esp_now.h>

#include "QMA6981.h"
#include "btn.h"
#include "touch_sensor.h"
#include "display.h"

#define NUM_LEDS 6

typedef enum __attribute__((packed))
{
    NO_WIFI,
    ESP_NOW
}
wifiMode_t;

/**
 * A struct of all the function pointers necessary for a swadge mode. If a mode
 * does not need a particular function, say it doesn't do audio handling, it
 * is safe to set the pointer to NULL. It just won't be called.
 */
typedef struct _swadgeMode
{
    /**
     * This swadge mode's name, mostly for debugging.
     * This is not a function pointer.
     */
    char* modeName;

    /**
     * This function is called when this mode is started. It should initialize
     * any necessary variables.
     * disp should be saved and used for later draw calls.
     *
     * @param disp The display to draw to
     */
    void (*fnEnterMode)(display_t* disp);

    /**
     * This function is called when the mode is exited. It should clean up
     * anything that shouldn't happen when the mode is not active
     */
    void (*fnExitMode)(void);

    /**
     * This function is called from the main loop. It's pretty quick, but the
     * timing may be inconsistent.
     *
     * @param elapsedUs The time elapsed since the last time this function was
     *                  called. Use this value to determine when it's time to do
     *                  things
     */
    void (*fnMainLoop)(int64_t elapsedUs);

    /**
     * This function is called when a button press is pressed. Buttons are
     * handled by interrupts and queued up for this callback, so there are no
     * strict timing restrictions for this function.
     *
     * @param evt The button event that occurred
     */
    void (*fnButtonCallback)(buttonEvt_t* evt);

    /**
     * This function is called when a touchpad event occurs.
     *
     * @param evt The touchpad event that occurred
     */
    void (*fnTouchCallback)(touch_event_t* evt);

    /**
     * This function is called periodically with the current acceleration
     * vector.
     *
     * @param accel A struct with 10 bit signed X, Y, and Z accel vectors
     */
    void (*fnAccelerometerCallback)(accel_t* accel);

    /**
     * This function is called whenever audio samples are read from the
     * microphone (ADC) and are ready for processing. Samples are read at 8KHz
     *
     * @param samples A pointer to 12 bit audio samples
     * @param sampleCnt The number of samples read
     */
    void (*fnAudioCallback)(uint16_t* samples, uint32_t sampleCnt);

    /**
     * This function is called periodically with the current temperature
     *
     * @param temperature A floating point temperature in celcius
     */
    void (*fnTemperatureCallback)(float temperature);

    /**
     * This is a setting, not a function pointer. Set it to one of these
     * values to have the system configure the swadge's WiFi
     *
     * NO_WIFI - Don't use WiFi at all. This saves power.
     * ESP_NOW - Send and receive packets to and from all swadges in range
     */
    wifiMode_t wifiMode;

    /**
     * This function is called whenever an ESP-NOW packet is received.
     *
     * @param mac_addr The MAC address which sent this data
     * @param data     A pointer to the data received
     * @param len      The length of the data received
     * @param rssi     The RSSI for this packet, from 1 (weak) to ~90 (touching)
     */
    void (*fnEspNowRecvCb)(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);

    /**
     * This function is called whenever an ESP-NOW packet is sent.
     * It is just a status callback whether or not the packet was actually sent.
     * This will be called after calling espNowSend()
     *
     * @param mac_addr The MAC address which the data was sent to
     * @param status   The status of the transmission
     */
    void (*fnEspNowSendCb)(const uint8_t* mac_addr, esp_now_send_status_t status);
} swadgeMode;

void overrideToSwadgeMode(swadgeMode* mode);
void switchToSwadgeMode(swadgeMode* mode);

#endif
