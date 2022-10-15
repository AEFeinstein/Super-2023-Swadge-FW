/*
 * mode_dance.h
 *
 *  Created on: Nov 10, 2018
 *      Author: adam
 */

#ifndef MODE_DANCE_H_
#define MODE_DANCE_H_

#include "swadgeMode.h"

extern swadgeMode modeDance;

void danceEnterMode(display_t* display);
void danceExitMode(void);
void danceMainLoop(int64_t elapsedUs);
void danceButtonCb(buttonEvt_t* evt);

uint8_t getNumDances(void);
void danceClearVars(uint8_t idx);
char* getDanceName(uint8_t idx);

#endif /* MODE_DANCE_H_ */
