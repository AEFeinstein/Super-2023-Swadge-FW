#include "driver/i2c.h"
#include "hal/gpio_types.h"

#include "i2c-conf.h"

/**
 * @brief i2c master initialization
 */
void i2c_master_init(gpio_num_t sda, gpio_num_t scl, gpio_pullup_t pullup, uint32_t clkHz)
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf =
    {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .sda_pullup_en = pullup,
        .scl_io_num = scl,
        .scl_pullup_en = pullup,
        .master.clk_speed = clkHz,
        .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL,
    };
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}
