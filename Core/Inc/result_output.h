#ifndef RESULT_OUTPUT_H
#define RESULT_OUTPUT_H

#include "measurement_types.h"

void ResultOutput_Init(void);
void ResultOutput_Process(void);
int ResultOutput_Send(const measurement_result_t *result,
                      const float *spectrum_rms,
                      const uint16_t *waveform_samples,
                      uint16_t waveform_sample_count);
void ResultOutput_TxComplete(void);
void ResultOutput_RxComplete(void);
void ResultOutput_UartError(void);
uint32_t ResultOutput_GetDroppedCount(void);
uint32_t ResultOutput_GetUartRecoveryCount(void);

#endif
