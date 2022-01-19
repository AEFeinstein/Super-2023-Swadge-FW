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

typedef struct touch_msg {
    touch_pad_intr_mask_t intr_mask;
    touch_pad_t pad_num;
    uint32_t pad_status;
    uint32_t pad_val;
} touch_event_t;

//==============================================================================
// Function Prototypes
//==============================================================================

void initTouchSensor(float touchPadSensitivity, bool denoiseEnable,
    uint8_t numTouchPads, ...);
bool checkTouchSensor(touch_event_t *);

#endif /* _TOUCH_SENSOR_H_ */