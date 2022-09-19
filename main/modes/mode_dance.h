/*
 * mode_dance.h
 *
 *  Created on: Nov 10, 2018
 *      Author: adam
 */

#ifndef MODE_DANCE_H_
#define MODE_DANCE_H_

extern swadgeMode modeDance;

void danceLeds(uint8_t danceIdx);
void setDanceBrightness(uint8_t brightness);
uint8_t getNumDances(void);
void danceClearVars(uint8_t idx);
char* getDanceName(uint8_t idx);

#endif /* MODE_DANCE_H_ */
