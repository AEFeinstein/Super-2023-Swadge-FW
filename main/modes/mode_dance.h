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

void danceComet(uint32_t tElapsedUs, uint32_t arg, bool reset);
void danceRise(uint32_t tElapsedUs, uint32_t arg, bool reset);
void dancePulse(uint32_t tElapsedUs, uint32_t arg, bool reset);
void danceSmoothRainbow(uint32_t tElapsedUs, uint32_t arg, bool reset);
void danceSharpRainbow(uint32_t tElapsedUs, uint32_t arg, bool reset);
void danceRainbowSolid(uint32_t tElapsedUs, uint32_t arg, bool reset);
void danceBinaryCounter(uint32_t tElapsedUs, uint32_t arg, bool reset);
void danceFire(uint32_t tElapsedUs, uint32_t arg, bool reset);
void dancePoliceSiren(uint32_t tElapsedUs, uint32_t arg, bool reset);
void dancePureRandom(uint32_t tElapsedUs, uint32_t arg, bool reset);
void danceRandomDance(uint32_t tElapsedUs, uint32_t arg, bool reset);
void danceChristmas(uint32_t tElapsedUs, uint32_t arg, bool reset);
void danceNone(uint32_t tElapsedUs, uint32_t arg, bool reset);

void danceEnterMode(display_t* display);
void danceExitMode(void);
void danceMainLoop(int64_t elapsedUs);
void danceButtonCb(buttonEvt_t* evt);

uint8_t getNumDances(void);
void danceClearVars(uint8_t idx);
char* getDanceName(uint8_t idx);

#endif /* MODE_DANCE_H_ */
