#include "bsp_adc_dma.h"
#include "adc.h"
#include "tim.h"
#include "main.h"

/*
 * Single-ADC1 one-shot DMA frame grabber triggered by TIM1_CH1.
 *
 * Start() arms the DMA for APP_FFT_SIZE samples, then starts the TIM1_CH1
 * PWM; its 2.4 MHz rising edges drive ADC1 conversions.  When the DMA has
 * moved the whole frame the transfer-complete callback stops the timer,
 * leaving the 16-bit samples in the buffer ready for TakeFrame().  This
 * mirrors the proven acquisition path of the reference F407 analyser.
 */

static uint16_t s_adc_dma[APP_FFT_SIZE];
static volatile const uint16_t *s_ready_frame = 0;
static volatile uint32_t s_overrun_count = 0U;

/* Diagnostic counters read out via SWD to locate the acquisition fault. */
volatile uint32_t dbg_start_calls;
volatile uint32_t dbg_start_fail;
volatile uint32_t dbg_complete_calls;
volatile uint32_t dbg_dma_state;

static void submit_frame(const uint16_t *frame)
{
  if (s_ready_frame != 0) ++s_overrun_count;
  else s_ready_frame = frame;
}

int BSP_ADC_DMA_Start(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  s_ready_frame = 0;
  if (primask == 0U) __enable_irq();
  __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_OVR | ADC_FLAG_EOC);
  ++dbg_start_calls;
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc_dma,
                        APP_FFT_SIZE) != HAL_OK)
  {
    ++dbg_start_fail;
    return -1;
  }
  /*
   * Drive TIM1_CH1 directly at register level: HAL_TIM_PWM_Start can return
   * without enabling CEN when the handle state is not READY.  Enable the
   * capture/compare channel, the main output (advanced timer requirement),
   * and the counter so the CC1 rising edges trigger ADC1.
   */
  TIM1->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1NP);
  TIM1->CCER |= TIM_CCER_CC1E;
  TIM1->BDTR |= TIM_BDTR_MOE;
  TIM1->CR1 |= TIM_CR1_CEN;
  return 0;
}

int BSP_ADC_DMA_Stop(void)
{
  TIM1->CR1 &= ~TIM_CR1_CEN;
  TIM1->CCER &= ~TIM_CCER_CC1E;
  (void)HAL_ADC_Stop_DMA(&hadc1);
  return 0;
}

int BSP_ADC_DMA_Recover(void)
{
  (void)BSP_ADC_DMA_Stop();
  (void)HAL_DMA_Abort(&hdma_adc1);
  (void)HAL_ADC_DeInit(&hadc1);
  __HAL_RCC_DMA2_FORCE_RESET();
  __HAL_RCC_DMA2_RELEASE_RESET();
  __HAL_RCC_DMA2_CLK_ENABLE();
  MX_ADC1_Init();
  s_ready_frame = 0;
  return BSP_ADC_DMA_Start();
}

const uint16_t *BSP_ADC_DMA_TakeFrame(void)
{
  const uint16_t *frame;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  frame = (const uint16_t *)s_ready_frame;
  s_ready_frame = 0;
  if (primask == 0U) __enable_irq();
  return frame;
}

uint32_t BSP_ADC_DMA_GetOverrunCount(void) { return s_overrun_count; }

float BSP_ADC_DMA_GetActualSampleRate(void)
{
  /* TIM1 on APB2 timer clock (144 MHz), ARR=59 -> 2.4 MSPS. */
  return 2.4e6f;
}

void BSP_ADC_DMA_HalfComplete(void)
{
  /* A frame is published only after the one-shot DMA transfer is complete. */
}

void BSP_ADC_DMA_Complete(void)
{
  ++dbg_complete_calls;
  /* Stop the trigger once the frame is captured. */
  TIM1->CR1 &= ~TIM_CR1_CEN;
  TIM1->CCER &= ~TIM_CCER_CC1E;
  submit_frame(&s_adc_dma[0]);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *adc)
{
  if (adc->Instance == ADC1) BSP_ADC_DMA_HalfComplete();
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc)
{
  if (adc->Instance == ADC1) BSP_ADC_DMA_Complete();
}
