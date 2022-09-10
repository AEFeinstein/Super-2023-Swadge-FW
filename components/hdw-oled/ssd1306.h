/*
 * ssd1306.h
 *
 *  Created on: Mar 16, 2019
 *      Author: adam, CNLohr
 */

#ifndef _SSD1306_H_
#define _SSD1306_H_

#include <stdint.h>
#include <stdbool.h>

#include "hal/gpio_types.h"
#include "../../main/display/display.h"

bool initOLED(display_t* disp, bool reset, gpio_num_t rst);

#endif /* _SSD1306_H_ */
