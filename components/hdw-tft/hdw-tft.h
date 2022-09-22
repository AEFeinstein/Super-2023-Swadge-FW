#ifndef _HDW_TFT_H_
#define _HDW_TFT_H_

#include "hal/gpio_types.h"
#include "hal/spi_types.h"

#include "../../main/display/display.h"

void initTFT(display_t* disp, spi_host_device_t spiHost, gpio_num_t sclk,
             gpio_num_t mosi, gpio_num_t dc, gpio_num_t cs, gpio_num_t rst,
             gpio_num_t backlight, bool isPwmBacklight);
int setTFTBacklight(uint8_t intensity);
void disableTFTBacklight(void);

#endif
