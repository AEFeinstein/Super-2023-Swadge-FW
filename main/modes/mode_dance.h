/*
 * mode_dance.h
 *
 *  Created on: Nov 10, 2018
 *      Author: adam
 */

#ifndef MODE_DANCE_H_
#define MODE_DANCE_H_

void ICACHE_FLASH_ATTR danceLeds(uint8_t danceIdx);
void ICACHE_FLASH_ATTR setDanceBrightness(uint8_t brightness);
uint8_t ICACHE_FLASH_ATTR getNumDances(void);
void ICACHE_FLASH_ATTR danceClearVars(uint8_t idx);
char* getDanceName(uint8_t idx);

#endif /* MODE_DANCE_H_ */
