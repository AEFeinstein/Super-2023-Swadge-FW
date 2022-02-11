#ifndef _HDW_MIC_H_
#define _HDW_MIC_H_

#include <stdint.h>
#include "hal/adc_types.h"
#include "driver/adc.h"

void continuous_adc_init(uint16_t adc1_chan_mask, uint16_t adc2_chan_mask,
                         adc_channel_t *channel, uint8_t channel_num);
void mic_main(void);

#endif