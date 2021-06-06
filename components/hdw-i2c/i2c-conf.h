#ifndef _OLED_H_
#define _OLED_H_

#define I2C_MASTER_SCL_IO         GPIO_NUM_6 /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO         GPIO_NUM_5 /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM            I2C_NUM_0  /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ        1000000    /*!< I2C master clock frequencym actually measured at 1mhz */
#define I2C_MASTER_TX_BUF_DISABLE 0          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0          /*!< I2C master doesn't need buffer */

void i2c_master_init(void);

#endif