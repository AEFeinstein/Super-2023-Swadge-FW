#ifndef _BTN_H_
#define _BTN_H_

#include <stdbool.h>

void initButtons(void);
void checkButtonQueue(void);
void setOledResetOn(bool on);

#endif