#ifndef APP_MEASUREMENT_H
#define APP_MEASUREMENT_H

#include <stdint.h>

void AppMeasurement_Init(void);
void AppMeasurement_Process(void);
/* Number of ADC frames fully processed so far (diagnostic heartbeat). */
uint32_t AppMeasurement_GetFrameCount(void);

#endif
