/**
 * @file qma7981.c
 * @brief
 * @version 0.1
 * @date 2021-09-01
 *
 * @copyright Copyright (c) 2021
 *
 */

// #include "bsp_i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "i2c-conf.h"
#include "qma7981.h"

#define QMA7981_REG_CHIP_ID 0x00
#define QMA7981_REG_DX_L 0x01
#define QMA7981_REG_DX_H 0x02
#define QMA7981_REG_DY_L 0x03
#define QMA7981_REG_DY_H 0x04
#define QMA7981_REG_DZ_L 0x05
#define QMA7981_REG_DZ_H 0x06
#define QMA7981_REG_STEP_L 0x07
#define QMA7981_REG_STEP_H 0x08
#define QMA7981_REG_INT_STAT_0 0x0A
#define QMA7981_REG_INT_STAT_1 0x0B
#define QMA7981_REG_INT_STAT_4 0x0D
#define QMA7981_REG_RANGE 0x0F
#define QMA7981_REG_BAND_WIDTH 0x10
#define QMA7981_REG_PWR_MANAGE 0x11
#define QMA7981_REG_STEP_CONF_0 0x12
#define QMA7981_REG_STEP_CONF_1 0x13
#define QMA7981_REG_STEP_CONF_2 0x14
#define QMA7981_REG_STEP_CONF_3 0x15
#define QMA7981_REG_INT_EN_0 0x16
#define QMA7981_REG_INT_EN_1 0x17
#define QMA7981_REG_INT_MAP_0 0x19
#define QMA7981_REG_INT_MAP_1 0x1A
#define QMA7981_REG_INT_MAP_2 0x1B
#define QMA7981_REG_INT_MAP_3 0x1C
#define QMA7981_REG_SIG_STEP_TH 0x1D
#define QMA7981_REG_STEP 0x1F

static const char* TAG = "qma7981";
static qma_range_t qma_range = QMA_RANGE_2G;

#define QMA7981_ADDR 0x12

static esp_err_t qma7981_read_byte(uint8_t reg_addr, uint8_t* data);
static esp_err_t qma7981_write_byte(uint8_t reg_addr, uint8_t data);
static esp_err_t qma7981_read_bytes(uint8_t reg_addr, size_t data_len, uint8_t* data);

/**
 * @brief
 *
 * @param reg_addr
 * @param data
 * @return esp_err_t
 */
static esp_err_t qma7981_read_byte(uint8_t reg_addr, uint8_t* data)
{
    return qma7981_read_bytes(reg_addr, 1, data);
}

static esp_err_t qma7981_write_byte(uint8_t reg_addr, uint8_t data)
{
    i2c_cmd_handle_t cmdHandle = i2c_cmd_link_create();
    i2c_master_start(cmdHandle);

    i2c_master_write_byte(cmdHandle, QMA7981_ADDR << 1, false);
    i2c_master_write_byte(cmdHandle, reg_addr, false);
    i2c_master_write_byte(cmdHandle, data, false);

    i2c_master_stop(cmdHandle);
    esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmdHandle, 100);
    i2c_cmd_link_delete(cmdHandle);
    return err;
}

static esp_err_t qma7981_read_bytes(uint8_t reg_addr, size_t data_len, uint8_t* data)
{
    i2c_cmd_handle_t cmdHandle = i2c_cmd_link_create();
    i2c_master_start(cmdHandle);
    i2c_master_write_byte(cmdHandle, QMA7981_ADDR << 1 | I2C_MASTER_WRITE, false);
    i2c_master_write_byte(cmdHandle, reg_addr, false);
    i2c_master_stop(cmdHandle);
    esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmdHandle, 100); // Hanging on second poll???
    i2c_cmd_link_delete(cmdHandle);

    if(ESP_OK != err)
    {
        return err;
    }

    cmdHandle = i2c_cmd_link_create();
    i2c_master_start(cmdHandle);
    i2c_master_write_byte(cmdHandle, QMA7981_ADDR << 1 | I2C_MASTER_READ, false);
    i2c_master_read(cmdHandle, data, data_len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmdHandle);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmdHandle, 100);
    i2c_cmd_link_delete(cmdHandle);

    return err;
}

esp_err_t qma7981_init(void)
{
    esp_err_t ret_val = ESP_OK;

    uint8_t id = 0;
    ret_val |= qma7981_read_byte(QMA7981_REG_CHIP_ID, &id);
    ESP_LOGW(TAG, "ID : %02X", id);

    /* ******************************** ******************************** */
    ret_val |= qma7981_write_byte(QMA7981_REG_PWR_MANAGE, 0xC0); /* Exit sleep mode*/
    vTaskDelay(pdMS_TO_TICKS(20));
    ret_val |= qma7981_write_byte(QMA7981_REG_RANGE, QMA_RANGE_2G);               /* Set range */
    ret_val |= qma7981_write_byte(QMA7981_REG_BAND_WIDTH, QMA_BANDWIDTH_1024_HZ); /* Set bandwidth */
    /* ******************************** ******************************** */

    return ret_val;
}

esp_err_t qma7981_get_step(uint16_t* data)
{
    esp_err_t ret_val = ESP_OK;
    uint8_t step_h = 0, step_l = 0;

    if (NULL == data)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ret_val |= qma7981_read_byte(QMA7981_REG_STEP_L, &step_l);
    ret_val |= qma7981_read_byte(QMA7981_REG_STEP_H, &step_h);

    *data = (step_h << 8) + step_l;

    return ret_val;
}

/**
 * @brief
 *
 * @param range
 * @return esp_err_t
 */
esp_err_t qma7981_set_range(qma_range_t range)
{
    esp_err_t ret_val = qma7981_write_byte(QMA7981_REG_RANGE, range);
    qma_range = range;

    return ret_val;
}

/**
 * @brief
 *
 * @param x
 * @param y
 * @param z
 * @return esp_err_t
 */
esp_err_t qma7981_get_acce(float* x, float* y, float* z)
{
    float multiple = 2;
    esp_err_t ret_val = ESP_OK;
    struct qma_acce_data_t
    {
        int16_t x;
        int16_t y;
        int16_t z;
    } data;

    switch (qma_range)
    {
        case QMA_RANGE_2G:
            multiple = 2;
            break;
        case QMA_RANGE_4G:
            multiple = 4;
            break;
        case QMA_RANGE_8G:
            multiple = 8;
            break;
        case QMA_RANGE_16G:
            multiple = 16;
            break;
        case QMA_RANGE_32G:
            multiple = 32;
            break;
        default:
            multiple = 2;
            break;
    }

    ret_val |= qma7981_read_bytes(QMA7981_REG_DX_L, 6, (uint8_t*)&data);

    /* QMA7981's range is 14 bit. Adjust data format */
    data.x >>= 2;
    data.y >>= 2;
    data.z >>= 2;

    /* Convert to acceleration of gravity */
    *x = data.x / (float)(1 << 13) * multiple;
    *y = data.y / (float)(1 << 13) * multiple;
    *z = data.z / (float)(1 << 13) * multiple;

    return ret_val;
}

/**
 * @brief
 *
 * @param x
 * @param y
 * @param z
 * @return esp_err_t
 */
esp_err_t qma7981_get_acce_int(int16_t* x, int16_t* y, int16_t* z)
{
    struct qma_acce_data_t
    {
        int16_t x;
        int16_t y;
        int16_t z;
    } data;
    static struct qma_acce_data_t lastKnownAccel;

    // Do the read
    esp_err_t ret_val = qma7981_read_bytes(QMA7981_REG_DX_L, 6, (uint8_t*)&data);

    // If the read was successsful
    if(ESP_OK == ret_val)
    {
        /* QMA7981's range is 14 bit. Adjust data format */
        lastKnownAccel.x = data.x >> 2;
        lastKnownAccel.y = data.y >> 2;
        lastKnownAccel.z = data.z >> 2;
    }

    // Return the values
    *x = lastKnownAccel.x;
    *y = lastKnownAccel.y;
    *z = lastKnownAccel.z;

    // Return the read status
    return ret_val;
}