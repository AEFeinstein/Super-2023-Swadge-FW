/*
 * mode_dance.h
 *
 *  Created on: Nov 10, 2018
 *      Author: adam
 */

#ifndef MODE_DANCE_H_
#define MODE_DANCE_H_

#include "swadgeMode.h"

typedef struct portableDance_t portableDance_t;

extern swadgeMode modeDance;


/// @brief Returns a pointer to a portableDance_t.
/// @param nvsKey The key where the dance index will be loaded and saved, if not NULL
/// @return A pointer to a new portableDance_t
portableDance_t* initPortableDance(const char* nvsKey);

/// @brief Deletes the given portableDance_t.
/// @param dance A pointer to the portableDance_t to delete
void freePortableDance(portableDance_t* dance);

/// @brief Runs the current LED dance. Should be called from the main loop every frame.
/// @param dance The portableDance_t pointer
/// @param elapsedUs The number of microseconds since the last frame
void portableDanceMainLoop(portableDance_t* dance, int64_t elapsedUs);

/// @brief Sets the current LED dance to the one specified, if it exists, and updates the saved index.
///        This works even if a dance is disabled.
/// @param dance The portableDance_t pointer to update
/// @param danceName The name of the dance to select
/// @return true if the dance was found, false if not
bool portableDanceSetByName(portableDance_t* dance, const char* danceName);

/// @brief Switches to the next enabled LED dance and updates the saved index.
/// @param dance The portableDance_t pointer to update
void portableDanceNext(portableDance_t* dance);

/// @brief Switches to the previous enabled LED dance and updates the saved index.
/// @param dance The portableDance_t pointer to update
void portableDancePrev(portableDance_t* dance);

/// @brief Disables the specified dance, if it exists, causing it to be skipped by portableDanceNext()/Prev()
/// @param dance The portableDance_t pointer to update
/// @param danceName The name of the dance to be disabled
/// @return true if the dance was found and disabled, false if not
bool portableDanceDisableDance(portableDance_t* dance, const char* danceName);

/// @brief Returns the name of the currently selected LED dance. The string does not need to be freed.
/// @param dance The portableDance_t pointer
/// @return The name of the current dance
const char* portableDanceGetName(portableDance_t* dance);


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
