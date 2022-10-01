#ifndef _PAINT_NVS_H_
#define _PAINT_NVS_H_

#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>

#include "nvs_manager.h"

void paintLoadIndex(void);
void paintSaveIndex(void);
void paintResetStorage(void);
bool paintGetSlotInUse(uint8_t slot);
void paintSetSlotInUse(uint8_t slot);
bool paintGetAnySlotInUse(void);
uint8_t paintGetRecentSlot(void);
void paintSetRecentSlot(uint8_t slot);
void paintSave(uint8_t slot);
void paintLoad(uint8_t slot, uint16_t xTr, uint16_t yTr, uint16_t xScale, uint16_t yScale);


#endif
