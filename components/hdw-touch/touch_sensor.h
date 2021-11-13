#ifndef _TOUCH_SENSOR_H_
#define _TOUCH_SENSOR_H_

#include <stdint.h>
#include "driver/touch_pad.h"

void initTouchSensor(uint8_t numTouchPads, touch_pad_t * touchPads,
    float * touchPadSensitivities, bool denoiseEnable, touch_pad_t waterproofPad);
void checkTouchSensor(void);

#endif /* _TOUCH_SENSOR_H_ */