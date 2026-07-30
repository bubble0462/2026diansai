#include "signal_preprocess.h"
#include "adc.h"
#include <math.h>

#define TWO_PI_F 6.2831853071795864769f

/* Hann window: CPU-only, recomputed in Init().  Kept in main SRAM (the CCM
 * is fully used by the FFT real/imag work arrays).  The de-DC scratch
 * buffer s_centered was merged out: the 63-tap FIR recomputes each
 * (raw[i-tap]-mean)*volts_per_code term on the fly, trading a little CPU
 * for 32 KB of RAM that the 8192-point pipeline could not otherwise fit. */
static float s_window[APP_FFT_SIZE];
static float s_window_power_gain;
static float s_lowpass_coefficients[APP_LOWPASS_TAP_COUNT] =
{
  2.6439479134e-05f, -6.3597824805e-05f, 1.1541771241e-04f, -1.7413192037e-04f,
  2.2301284383e-04f, -2.3528564775e-04f, 1.7493282618e-04f, -9.2293323441e-18f,
  -3.3129704137e-04f, 8.5182116708e-04f, -1.5748250482e-03f, 2.4825880502e-03f,
  -3.5161042082e-03f, 4.5678092215e-03f, -5.4793115073e-03f, 6.0457346713e-03f,
  -6.0275884057e-03f, 5.1701223033e-03f, -3.2289982538e-03f, 7.3456556596e-17f,
  4.6504390286e-03f, -1.0757696684e-02f, 1.8238325890e-02f, -2.6879792246e-02f,
  3.6343075015e-02f, -4.6179197671e-02f, 5.5859061148e-02f, -6.4814215887e-02f,
  7.2484674155e-02f, -7.8368761801e-02f, 8.2069514751e-02f, 9.1665567176e-01f,
  8.2069514751e-02f, -7.8368761801e-02f, 7.2484674155e-02f, -6.4814215887e-02f,
  5.5859061148e-02f, -4.6179197671e-02f, 3.6343075015e-02f, -2.6879792246e-02f,
  1.8238325890e-02f, -1.0757696684e-02f, 4.6504390286e-03f, 7.3456556596e-17f,
  -3.2289982538e-03f, 5.1701223033e-03f, -6.0275884057e-03f, 6.0457346713e-03f,
  -5.4793115073e-03f, 4.5678092215e-03f, -3.5161042082e-03f, 2.4825880502e-03f,
  -1.5748250482e-03f, 8.5182116708e-04f, -3.3129704137e-04f, -9.2293323441e-18f,
  1.7493282618e-04f, -2.3528564775e-04f, 2.2301284383e-04f, -1.7413192037e-04f,
  1.1541771241e-04f, -6.3597824805e-05f, 2.6439479134e-05f
};

void SignalPreprocess_Init(void)
{
  uint32_t i;
  float sum_square = 0.0f;

  for (i = 0U; i < APP_FFT_SIZE; ++i)
  {
    float phase = TWO_PI_F * (float)i / (float)(APP_FFT_SIZE - 1U);
    s_window[i] = 0.5f - 0.5f * cosf(phase);
    sum_square += s_window[i] * s_window[i];
  }
  s_window_power_gain = sum_square / (float)APP_FFT_SIZE;
}

uint32_t SignalPreprocess_Run(const uint16_t *raw,
                              float *windowed,
                              measurement_result_t *result)
{
  uint32_t i;
  uint32_t clip_count = 0U;
  uint32_t flags = MEAS_FLAG_ALIAS_RISK;
  float mean_code = 0.0f;
  float sum_square = 0.0f;
  const float volts_per_code = (ADC_GetReferenceVoltage() * APP_ADC_VOLTAGE_GAIN) /
                               APP_ADC_FULL_SCALE;

  for (i = 0U; i < APP_FFT_SIZE; ++i)
  {
    mean_code += (float)raw[i];
    if ((raw[i] <= APP_CLIP_LOW_CODE) || (raw[i] >= APP_CLIP_HIGH_CODE))
    {
      ++clip_count;
    }
  }
  mean_code /= (float)APP_FFT_SIZE;
  result->dc_voltage_v = mean_code * volts_per_code;

  for (i = 0U; i < APP_FFT_SIZE; ++i)
  {
    float centered = ((float)raw[i] - mean_code) * volts_per_code;
    sum_square += centered * centered;
  }
  /* 63-tap FIR low-pass.  The centered value is recomputed from raw[]
   * instead of buffering it, which saves the 32 KB s_centered array. */
  for (i = 0U; i < APP_FFT_SIZE; ++i)
  {
    uint32_t tap;
    float filtered = 0.0f;
    uint32_t count = (i + 1U < APP_LOWPASS_TAP_COUNT) ?
                     (i + 1U) : APP_LOWPASS_TAP_COUNT;
    for (tap = 0U; tap < count; ++tap)
    {
      float centered = ((float)raw[i - tap] - mean_code) * volts_per_code;
      filtered += s_lowpass_coefficients[tap] * centered;
    }
    windowed[i] = filtered;
  }
  for (i = 0U; i < APP_FFT_SIZE; ++i)
  {
    windowed[i] *= s_window[i];
  }
  result->input_rms_v = sqrtf(sum_square / (float)APP_FFT_SIZE);

  if (result->input_rms_v < APP_NO_SIGNAL_RMS_V)
  {
    flags |= MEAS_FLAG_NO_SIGNAL;
  }
  else if (result->input_rms_v < APP_WEAK_SIGNAL_RMS_V)
  {
    flags |= MEAS_FLAG_TOO_WEAK;
  }
  if (clip_count >= APP_CLIP_SAMPLE_LIMIT)
  {
    flags |= MEAS_FLAG_CLIPPING;
  }
  if ((result->dc_voltage_v < 0.20f) || (result->dc_voltage_v > 3.10f))
  {
    flags |= MEAS_FLAG_DC_OVERLOAD;
  }

  result->flags = flags;
  return flags;
}

float SignalPreprocess_GetWindowPowerGain(void)
{
  return s_window_power_gain;
}
