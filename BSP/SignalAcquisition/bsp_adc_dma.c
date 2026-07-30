#include "bsp_adc_dma.h"
#include "adc.h"
#include "tim.h"
#include "main.h"

/*
 * ADC1 + ADC2 dual-interleaved one-shot DMA frame grabber.
 *
 * DMA mode 2 reads ADC common CDR as packed 32-bit words.  On little-endian
 * Cortex-M4 memory, reinterpreting each word as two uint16_t values yields
 * the time order ADC1, ADC2, ADC1, ADC2 without a deinterleave copy.
 */

static uint32_t s_adc_dma[APP_ADC_DMA_WORD_COUNT];
static volatile const uint16_t *s_ready_frame = 0;
static volatile uint32_t s_overrun_count = 0U;

/* Diagnostic counters read out via SWD to locate the acquisition fault. */
volatile uint32_t dbg_start_calls;
volatile uint32_t dbg_start_fail;
volatile uint32_t dbg_complete_calls;
volatile uint32_t dbg_dma_state;
volatile uint32_t dbg_half_cycle_stamp;
volatile uint32_t dbg_second_half_cycles;
static uint8_t s_cycle_counter_enabled;

static void submit_frame(const uint16_t *frame)
{
  if (s_ready_frame != 0) ++s_overrun_count;
  else s_ready_frame = frame;
}

int BSP_ADC_DMA_Start(void)
{
  volatile uint32_t delay;
  const uint32_t multimode_mask =
      ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS | ADC_CCR_DELAY;
  const uint32_t multimode_config =
      ADC_DUALMODE_INTERL | ADC_DMAACCESSMODE_2 | ADC_CCR_DDS |
      ADC_TWOSAMPLINGDELAY_13CYCLES;
  uint32_t primask = __get_PRIMASK();
  if (s_cycle_counter_enabled == 0U)
  {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    s_cycle_counter_enabled = 1U;
  }
  __disable_irq();
  s_ready_frame = 0;
  if (primask == 0U) __enable_irq();
  __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_OVR | ADC_FLAG_EOC);
  __HAL_ADC_CLEAR_FLAG(&hadc2, ADC_FLAG_OVR | ADC_FLAG_EOC);
  ++dbg_start_calls;

  /*
   * F407's regular interleaved sequencer can retain its completed-pair
   * state after a NORMAL DMA frame.  Re-selecting the common mode while
   * both ADCs are off provides a deterministic per-frame re-arm without
   * resetting DMA2 (which is also used by USART1 RX).
   */
  CLEAR_BIT(ADC->CCR, multimode_mask);
  __DSB();
  SET_BIT(ADC->CCR, multimode_config);

  /*
   * F4 HAL only enables the master in MultiModeStart_DMA.  Enable and
   * stabilize ADC2 explicitly before starting ADC1, otherwise CDR contains
   * only master samples and the DMA completion path can stall.
   */
  if ((hadc2.Instance->CR2 & ADC_CR2_ADON) == 0U)
  {
    __HAL_ADC_ENABLE(&hadc2);
    delay = (SystemCoreClock / 1000000U) * 3U;
    while (delay-- != 0U) { __NOP(); }
  }
  if (HAL_ADCEx_MultiModeStart_DMA(&hadc1, s_adc_dma,
                                   APP_ADC_DMA_WORD_COUNT) != HAL_OK)
  {
    ++dbg_start_fail;
    return -1;
  }
  return 0;
}

int BSP_ADC_DMA_Stop(void)
{
  (void)HAL_ADCEx_MultiModeStop_DMA(&hadc1);
  __HAL_ADC_DISABLE(&hadc2);
  return 0;
}

int BSP_ADC_DMA_Recover(void)
{
  (void)BSP_ADC_DMA_Stop();
  (void)HAL_DMA_Abort(&hdma_adc1);
  (void)HAL_ADC_DeInit(&hadc1);
  (void)HAL_ADC_DeInit(&hadc2);
  __HAL_RCC_DMA2_FORCE_RESET();
  __HAL_RCC_DMA2_RELEASE_RESET();
  __HAL_RCC_DMA2_CLK_ENABLE();
  MX_ADC1_Init();
  MX_ADC2_Init();
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
  /* Hardware-timed combined rate for the 15-cycle/13-delay setting. */
  return APP_ADC_SAMPLE_RATE_HZ;
}

void BSP_ADC_DMA_HalfComplete(void)
{
  dbg_half_cycle_stamp = DWT->CYCCNT;
}

void BSP_ADC_DMA_Complete(void)
{
  dbg_second_half_cycles = DWT->CYCCNT - dbg_half_cycle_stamp;
  ++dbg_complete_calls;
  /* Stop both free-running converters immediately after the one-shot DMA. */
  CLEAR_BIT(ADC1->CR2, ADC_CR2_ADON);
  CLEAR_BIT(ADC2->CR2, ADC_CR2_ADON);
  submit_frame((const uint16_t *)&s_adc_dma[0]);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *adc)
{
  if (adc->Instance == ADC1) BSP_ADC_DMA_HalfComplete();
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc)
{
  if (adc->Instance == ADC1) BSP_ADC_DMA_Complete();
}
