#ifndef _ACC_HELPER
#define _ACC_HELPER

#include <stdint.h>

#include "../Components/lsm6dsl/lsm6dsl.h"
#include "stm32l475xx.h"
#include "stm32l4xx_hal.h"

void ACC_SingnificantMotionDetectionOn(void);
void ACC_SingnificantMotionDetectionOff(void);
void ACC_InitGPIO(void);

#endif