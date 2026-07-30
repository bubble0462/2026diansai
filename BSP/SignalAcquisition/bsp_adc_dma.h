#ifndef BSP_ADC_DMA_H
#define BSP_ADC_DMA_H

#include <stdint.h>
#include "app_config.h"

int BSP_ADC_DMA_Start(void);
int BSP_ADC_DMA_Stop(void);
int BSP_ADC_DMA_Recover(void);
const uint16_t *BSP_ADC_DMA_TakeFrame(void);
uint32_t BSP_ADC_DMA_GetOverrunCount(void);
float BSP_ADC_DMA_GetActualSampleRate(void);
void BSP_ADC_DMA_HalfComplete(void);
void BSP_ADC_DMA_Complete(void);

#endif
