/*
 * QMA6981.h
 *
 *  Created on: Jul 9, 2019
 *      Author: adam
 */

#ifndef QMA6981_H_
#define QMA6981_H_

#include <stdbool.h>

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} accel_t;

bool QMA6981_setup(void);
void QMA6981_poll(accel_t* currentAccel);

#endif /* QMA6981_H_ */
