#ifndef _OLED_H_
#define _OLED_H_

#define I2C_MASTER_NUM            I2C_NUM_0  /*!< I2C port number for master dev */

void i2c_master_init(gpio_num_t sda, gpio_num_t scl, gpio_pullup_t pullup, uint32_t clkHz);

#endif