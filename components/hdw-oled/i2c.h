#ifndef _OLED_H_
#define _OLED_H_

void i2c_master_init(void);
void i2cMasterSend(uint8_t addr, uint8_t* data, uint16_t dataLen);

#endif