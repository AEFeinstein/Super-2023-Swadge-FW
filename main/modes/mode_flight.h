/*
 * mode_flight.h
 *
 *  Created on: Sept 15th, 2020
 *      Author: <>< CNLohr
 */

#ifndef MODES_MODE_FLIGHT_H_
#define MODES_MODE_FLIGHT_H_

extern swadgeMode flightMode;

#define NUM_FLIGHTSIM_TOP_SCORES 4
#define FLIGHT_HIGH_SCORE_NAME_LEN 4

typedef struct __attribute__((aligned(4)))
{
    //One set for any% one set for 100%
    char displayName[NUM_FLIGHTSIM_TOP_SCORES * 2][FLIGHT_HIGH_SCORE_NAME_LEN];
    uint32_t timeCentiseconds[NUM_FLIGHTSIM_TOP_SCORES * 2];
    uint8_t flightInvertY;
}
flightSimSaveData_t;


#endif /* MODES_MODE_FLIGHT_H_ */
