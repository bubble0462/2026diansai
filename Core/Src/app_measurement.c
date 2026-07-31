#include "app_measurement.h"
#include "app_config.h"
#include "adc.h"
#include "bsp_adc_dma.h"
#include "distortion_analyzer.h"
#include "gain_control.h"
#include "result_output.h"
#include "signal_preprocess.h"
#include "spectrum_analyzer.h"
#include "main.h"
#include <math.h>
#include <string.h>

/*
 * Measurement pipeline for the F407 distortion / reconstruction demo.
 *
 * Each cycle: take one interleaved ADC frame -> window + DC/RMS ->
 * FFT + harmonic analysis -> gain-control decision -> push the result
 * and a slice of the raw waveform to the TJC display state machine.
 * Gain switching (AD603 via DAC) discards a few frames while the
 * analogue front end settles, which keeps the displayed amplitude steady.
 */

static float s_spectrum_rms[APP_SPECTRUM_BINS];
static uint16_t s_aligned_frame[APP_ADC_FRAME_SAMPLES];
static measurement_result_t s_result;
static uint32_t s_sequence;
static uint8_t s_adc_recovery_pending;
static uint32_t s_last_frame_ms;
static uint32_t s_last_overrun_count;
#if APP_GAIN_AUTO_CONTROL_ENABLED
static uint8_t s_gain_initialised;
#endif

/*
 * Dual interleaving exposes the small ADC1/ADC2 offset and gain mismatch as
 * an alternating-code ripple.  It is most visible on a 50 mVpp waveform and
 * can also create false high-frequency energy.  Estimate each converter's
 * mean and AC power over the complete frame, then map both streams to their
 * common mean and RMS.  A 10% clamp prevents noise-only frames from receiving
 * an unreasonable gain correction.
 */
static const uint16_t *align_interleaved_adcs(const uint16_t *raw)
{
  float mean[2] = {0.0f, 0.0f};
  float power[2] = {0.0f, 0.0f};
  float scale[2] = {1.0f, 1.0f};
  float common_mean;
  float common_rms;
  uint32_t count = (APP_ADC_FRAME_SAMPLES / 2U) - 1U;
  uint32_t index;

  /* The first conversion after enabling dual mode is not settled. */
  for (index = 2U; index < APP_ADC_FRAME_SAMPLES; ++index)
  {
    mean[index & 1U] += (float)raw[index];
  }
  mean[0] /= (float)count;
  mean[1] /= (float)count;
  for (index = 2U; index < APP_ADC_FRAME_SAMPLES; ++index)
  {
    uint32_t adc = index & 1U;
    float centered = (float)raw[index] - mean[adc];
    power[adc] += centered * centered;
  }
  power[0] = sqrtf(power[0] / (float)count);
  power[1] = sqrtf(power[1] / (float)count);
  common_mean = 0.5f * (mean[0] + mean[1]);
  common_rms = 0.5f * (power[0] + power[1]);
  if ((power[0] > 3.0f) && (power[1] > 3.0f))
  {
    scale[0] = common_rms / power[0];
    scale[1] = common_rms / power[1];
    if (scale[0] < 0.90f) scale[0] = 0.90f;
    if (scale[0] > 1.10f) scale[0] = 1.10f;
    if (scale[1] < 0.90f) scale[1] = 0.90f;
    if (scale[1] > 1.10f) scale[1] = 1.10f;
  }
  for (index = 2U; index < APP_ADC_FRAME_SAMPLES; ++index)
  {
    uint32_t adc = index & 1U;
    float corrected = common_mean +
        ((float)raw[index] - mean[adc]) * scale[adc];
    if (corrected < 0.0f) corrected = 0.0f;
    if (corrected > APP_ADC_FULL_SCALE) corrected = APP_ADC_FULL_SCALE;
    s_aligned_frame[index] = (uint16_t)(corrected + 0.5f);
  }
  /* Hann-windowed FFT nearly suppresses these endpoints, but replacing the
   * invalid startup pair also protects raw min/max and debug views. */
  s_aligned_frame[0] = s_aligned_frame[2];
  s_aligned_frame[1] = s_aligned_frame[3];
  return s_aligned_frame;
}

static uint16_t waveform_sample_count(const measurement_result_t *result)
{
  float samples_per_period;
  float minimum_frequency_hz;
  float maximum_frequency_hz;
  uint32_t count;

  /*
   * The interpolated FFT estimate can land slightly above an exact 500 kHz
   * input.  Use the same edge tolerance as the spectrum search instead of
   * rejecting that valid upper-bound signal and publishing no Upp samples.
   */
  maximum_frequency_hz = APP_MAX_FUNDAMENTAL_HZ +
      ((float)APP_EDGE_TOLERANCE_BINS *
       result->frequency_resolution_hz);
  /*
   * A true 10 kHz tone can interpolate to 9999.x Hz from one frame to the
   * next.  The previous strict 10000.0 Hz comparison discarded roughly half
   * of the lower-bound frames, making Upp disappear and the graph clear.
   * One FFT-bin tolerance accepts the legal boundary without expanding the
   * analyser's actual search range.
   */
  minimum_frequency_hz = APP_WAVEFORM_MIN_HZ -
      result->frequency_resolution_hz;
  if (((result->flags & (MEAS_FLAG_NO_SIGNAL | MEAS_FLAG_TOO_WEAK)) != 0U) ||
      (result->fundamental_frequency_hz < minimum_frequency_hz) ||
      (result->fundamental_frequency_hz > maximum_frequency_hz))
  {
    return 0U;
  }
  samples_per_period = result->sample_rate_hz /
                       result->fundamental_frequency_hz;
  count = (uint32_t)((samples_per_period * APP_WAVEFORM_PERIODS) + 0.5f);
  if (count > APP_WAVEFORM_MAX_SAMPLES)
  {
    count = APP_WAVEFORM_MAX_SAMPLES;
  }
  if (count > APP_FFT_SIZE)
  {
    count = APP_FFT_SIZE;
  }
  return (uint16_t)count;
}

void AppMeasurement_Init(void)
{
  memset(&s_result, 0, sizeof(s_result));
  s_adc_recovery_pending = 0U;
  s_last_overrun_count = BSP_ADC_DMA_GetOverrunCount();
  s_last_frame_ms = HAL_GetTick();
  SignalPreprocess_Init();
  if (SpectrumAnalyzer_Init() != 0)
  {
    Error_Handler();
  }
  ResultOutput_Init();
#if APP_GAIN_AUTO_CONTROL_ENABLED
  if (GainControl_Init() != 0)
  {
    Error_Handler();
  }
  s_gain_initialised = 1U;
#endif
  /* The F407 ADC calibrates itself at power-on; there is no software
   * offset calibration register to program, unlike the H7. */
  if (BSP_ADC_DMA_Start() != 0)
  {
    Error_Handler();
  }
}

void AppMeasurement_Process(void)
{
  const uint16_t *frame;
  uint32_t overrun_count;
  float *fft_input = SpectrumAnalyzer_GetInputBuffer();

  if (s_adc_recovery_pending != 0U)
  {
    if (BSP_ADC_DMA_Recover() == 0)
    {
      s_adc_recovery_pending = 0U;
      s_last_frame_ms = HAL_GetTick();
    }
    return;
  }
  frame = BSP_ADC_DMA_TakeFrame();
  if (frame == 0)
  {
    if ((HAL_GetTick() - s_last_frame_ms) > 100U)
    {
      s_adc_recovery_pending = 1U;
    }
    return;
  }
  s_last_frame_ms = HAL_GetTick();
  if (BSP_ADC_DMA_Stop() != 0)
  {
    s_adc_recovery_pending = 1U;
    return;
  }
  frame = align_interleaved_adcs(frame);

  memset(&s_result, 0, sizeof(s_result));
  s_result.sequence = ++s_sequence;
  s_result.sample_rate_hz = BSP_ADC_DMA_GetActualSampleRate();
  s_result.adc_reference_voltage_v = ADC_GetReferenceVoltage();
  s_result.fft_size = APP_FFT_SIZE;
  s_result.spectrum_bins = APP_OUTPUT_SPECTRUM_BINS;
  s_result.frequency_resolution_hz =
      s_result.sample_rate_hz / (float)APP_FFT_SIZE;

  (void)SignalPreprocess_Run(frame, fft_input, &s_result);
  overrun_count = BSP_ADC_DMA_GetOverrunCount();
  s_result.overrun_count = overrun_count;
  if (overrun_count != s_last_overrun_count)
  {
    s_result.flags |= MEAS_FLAG_OVERRUN;
  }
  s_last_overrun_count = overrun_count;

  DistortionAnalyzer_Run(fft_input, s_spectrum_rms, &s_result);

#if APP_GAIN_AUTO_CONTROL_ENABLED
  if (s_gain_initialised != 0U)
  {
    gain_control_action_t action = GainControl_Process(&s_result);
    if (action.publish_result == 0U)
    {
      /* Gain is changing; let the AD603 settle before showing data. */
      if (BSP_ADC_DMA_Start() != 0)
      {
        s_adc_recovery_pending = 1U;
      }
      return;
    }
  }
#endif

  if ((s_sequence % APP_SPECTRUM_SEND_DIVIDER) == 0)
  {
    uint16_t period_count = waveform_sample_count(&s_result);
    uint32_t period_start = (APP_FFT_SIZE - period_count) / 2U;
    (void)ResultOutput_Send(&s_result, s_spectrum_rms,
                            &frame[period_start], period_count);
  }
  if (BSP_ADC_DMA_Start() != 0)
  {
    s_adc_recovery_pending = 1U;
  }
}

uint32_t AppMeasurement_GetFrameCount(void)
{
  return s_sequence;
}
