#ifndef _HDW_MIC_H_
#define _HDW_MIC_H_

#include <stdint.h>
#include "hal/adc_types.h"
#include "driver/adc.h"

#define BYTES_PER_READ 512 // Each sample is two bytes

void continuous_adc_start(void);
void continuous_adc_init(uint16_t adc1_chan_mask, uint16_t adc2_chan_mask,
                         adc_channel_t* channel, uint8_t channel_num);
uint32_t continuous_adc_read(uint16_t* outSamples);
void continuous_adc_deinit(void);
void continuous_adc_stop(void);

void oneshot_adc_init(adc_unit_t adcUnit, adc1_channel_t adcChan);
uint32_t oneshot_adc_read(void);

#endif