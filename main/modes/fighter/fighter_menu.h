#ifndef FIGHTER_MENU_H_
#define FIGHTER_MENU_H_

#include "mode_fighter.h"

extern swadgeMode modeFighter;

void fighterSendButtonsToOther(int32_t btnState);
void fighterSendSceneToOther(fighterScene_t* scene, uint8_t len);
void fighterShowHrResult(fightingCharacter_t character, vector_t position,
                         vector_t velocity, int32_t gravity, int32_t platformEndX);
void fighterShowMpResult(void);

#endif