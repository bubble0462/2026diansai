#ifndef SIGNAL_PREPROCESS_H
#define SIGNAL_PREPROCESS_H

#include <stdint.h>
#include "measurement_types.h"

void SignalPreprocess_Init(void);
uint32_t SignalPreprocess_Run(const uint16_t *raw,
                              float *windowed,
                              measurement_result_t *result);
float SignalPreprocess_GetWindowPowerGain(void);

#endif
