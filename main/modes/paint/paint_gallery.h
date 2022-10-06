#ifndef _PAINT_GALLERY_H_
#define _PAINT_GALLERY_H_

#include <stdint.h>

#include "swadgeMode.h"

void paintGallerySetup(void);
void paintGalleryMainLoop(int64_t elapsedUs);
void paintGalleryModeButtonCb(buttonEvt_t* evt);
void paintGalleryDoLoad(void);

#endif
