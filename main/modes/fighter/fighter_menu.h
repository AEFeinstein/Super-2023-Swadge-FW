#ifndef FIGHTER_MENU_H_
#define FIGHTER_MENU_H_

#include "mode_fighter.h"

extern swadgeMode modeFighter;

extern const char* charNames[3];
extern const char str_multiplayer[];
extern const char str_hrContest[];

void fighterSendButtonsToOther(int32_t btnState);
void fighterSendSceneToOther(fighterScene_t* scene, uint8_t len);
void fighterShowHrResult(fightingCharacter_t character, vector_t position,
                         vector_t velocity, int32_t gravity, int32_t platformEndX);
void fighterShowMpResult(uint32_t roundTimeMs,
                         fightingCharacter_t self,  int8_t selfKOs, int16_t selfDmg,
                         fightingCharacter_t other, int8_t otherKOs, int16_t otherDmg);

#endif