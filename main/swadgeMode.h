#ifndef _SWADGE_MODE_H_
#define _SWADGE_MODE_H_

#include <stdint.h>
#include "QMA6981.h"

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
     * any necessary variables
     */
    void (*fnEnterMode)(void);
    /**
     * This function is called when the mode is exited. It should clean up
     * anything that shouldn't happen when the mode is not active
     */
    void (*fnExitMode)(void);
    /**
     * This function is called when a button press is detected from user_main.c's
     * HandleButtonEvent(). It does not pass mode select button events. It is
     * called from an interrupt, so do the minimal amount of processing here as
     * possible. It is better to do heavy processing in timerCallback(). Any
     * global variables shared between buttonCallback() and other functions must
     * be declared volatile.
     *
     * @param state A bitmask of all button statuses
     * @param button  The button number which was pressed
     * @param down 1 if the button was pressed, 0 if it was released
     */
    void (*fnButtonCallback)(uint8_t state, int button, int down);
    // /**
    //  * This is a setting, not a function pointer. Set it to one of these
    //  * values to have the system configure the swadge's WiFi
    //  * NO_WIFI - Don't use WiFi at all. This saves power.
    //  * SOFT_AP - Become a WiFi access point serving up the colorchord
    //  *           configuration website
    //  * ESP_NOW - Send and receive packets to and from all swadges in range
    //  */
    // wifiMode_t wifiMode;
    // /**
    //  * This function is called whenever an ESP NOW packet is received.
    //  *
    //  * @param mac_addr The MAC address which sent this data
    //  * @param data     A pointer to the data received
    //  * @param len      The length of the data received
    //  * @param rssi     The RSSI for this packet, from 1 (weak) to ~90 (touching)
    //  */
    // void (*fnEspNowRecvCb)(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);
    // /**
    //  * This function is called whenever an ESP NOW packet is sent.
    //  * It is just a status callback whether or not the packet was actually sent.
    //  * This will be called after calling espNowSend()
    //  *
    //  * @param mac_addr The MAC address which the data was sent to
    //  * @param status   The status of the transmission
    //  */
    // void (*fnEspNowSendCb)(uint8_t* mac_addr, esp_now_send_status_t status);
    /**
     * This function is called periodically with the current acceleration
     * vector.
     *
     * @param accel A struct with 10 bit signed X, Y, and Z accel vectors
     */
    void (*fnAccelerometerCallback)(accel_t* accel);
    // /**
    //  * A pointer to the compressed image data in ROM
    //  */
    // uint8_t* menuImageData;
    // /**
    //  * The length of the compressed image data in ROM
    //  */
    // uint16_t menuImageLen;
} swadgeMode;

#endif