#ifndef _FREE_RTOS_TASK_H_
#define _FREE_RTOS_TASK_H_

#include <stdint.h>

typedef void* TaskHandle_t;

#define tskIDLE_PRIORITY    ( ( int ) 0U )

#define portBASE_TYPE int
typedef portBASE_TYPE BaseType_t;
typedef void (*TaskFunction_t)( void * );

typedef unsigned portBASE_TYPE	UBaseType_t;

void taskYIELD(void);

#endif