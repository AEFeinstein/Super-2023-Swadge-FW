#ifndef FIGHTER_MENU_H_
#define FIGHTER_MENU_H_

extern swadgeMode modeFighter;

void fighterSendButtonsToOther(int32_t btnState);
void fighterSendSceneToOther(int16_t* scene, uint8_t len);

#endif