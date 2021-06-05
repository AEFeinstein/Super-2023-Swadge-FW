#include "driver/i2c.h"
#include "hal/gpio_types.h"

#include "i2c.h"

#define I2C_MASTER_SCL_IO         GPIO_NUM_6 /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO         GPIO_NUM_5 /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM            I2C_NUM_0  /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ        1000000    /*!< I2C master clock frequencym actually measured at 1mhz */
#define I2C_MASTER_TX_BUF_DISABLE 0          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0          /*!< I2C master doesn't need buffer */

/**
 * @brief i2c master initialization
 */
void i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf =
    {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL,
    };
    ESP_ERROR_CHECK(i2c_param_config(i2c_master_port, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE,
                                       0));
}

void i2cMasterSend(uint8_t addr, uint8_t* data, uint16_t dataLen)
{
    i2c_cmd_handle_t cmdHandle = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmdHandle));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmdHandle, addr, false));
    for (uint16_t idx = 0; idx < dataLen; idx++)
    {
        ESP_ERROR_CHECK(i2c_master_write_byte(cmdHandle, data[idx], false));
    }
    ESP_ERROR_CHECK(i2c_master_stop(cmdHandle));

    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmdHandle, 100));
    i2c_cmd_link_delete(cmdHandle);
}
