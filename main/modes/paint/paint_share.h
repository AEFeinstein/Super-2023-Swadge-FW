#ifndef _PAINT_SHARE_H_
#define _PAINT_SHARE_H_

#include "swadgeMode.h"

#include "paint_common.h"

extern swadgeMode modePaintShare;

// Because I'm stupid and tried to pass state in a struct that gets freed between modes
extern bool paintShareIsSender;

#endif