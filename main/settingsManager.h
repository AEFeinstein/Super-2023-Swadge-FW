#ifndef _SETTINGS_MANAGER_H_
#define _SETTINGS_MANAGER_H_

//==============================================================================
// Function Prototypes
//==============================================================================

bool getIsMuted(void);
bool setIsMuted(bool);

int32_t getBrightness(void);
bool setBrightness(int32_t);

int32_t getMicVolume(void);
uint8_t getMicAmplitude(void);
bool setMicVolume(int32_t micVolume);

uint32_t getQJumperHighScore(void);
bool setQJumperHighScore(uint32_t highScore);

#endif