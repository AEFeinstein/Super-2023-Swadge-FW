#ifndef _TOUCH_SENSOR_H_
#define _TOUCH_SENSOR_H_

//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include "driver/touch_pad.h"

//==============================================================================
// Structs
//==============================================================================

typedef struct
{
    uint16_t state;
    touch_pad_t pad;
    bool down;
    int16_t position;
} touch_event_t;

//==============================================================================
// Function Prototypes
//==============================================================================

void initTouchSensor(float touchPadSensitivity, bool denoiseEnable,
                     uint8_t numTouchPads, ...);
bool checkTouchSensor(touch_event_t*);

#endif /* _TOUCH_SENSOR_H_ */