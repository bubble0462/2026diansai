#include "delay.h"

void Delay_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void Delay_us(uint32_t nus)
{
    uint32_t start_tick = DWT->CYCCNT;
    uint32_t delay_ticks = nus * (HAL_RCC_GetHCLKFreq() / 1000000U);

    while ((DWT->CYCCNT - start_tick) < delay_ticks)
    {
    }
}

void Delay_ms(uint32_t nms)
{
    HAL_Delay(nms);
}
