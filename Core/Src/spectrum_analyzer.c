#include "spectrum_analyzer.h"
#include "signal_preprocess.h"
#include <math.h>

#define TWO_PI_F 6.2831853071795864769f
static float s_fft_real[APP_FFT_SIZE];
static float s_fft_imag[APP_FFT_SIZE];

int SpectrumAnalyzer_Init(void)
{
  return ((APP_FFT_SIZE != 0U) &&
          ((APP_FFT_SIZE & (APP_FFT_SIZE - 1U)) == 0U)) ? 0 : -1;
}

float *SpectrumAnalyzer_GetInputBuffer(void)
{
  return s_fft_real;
}

static void fft_radix2_real(const float *input)
{
  uint32_t i;
  uint32_t j = 0U;
  uint32_t length;
  /* When the caller wrote directly into s_fft_real via
   * SpectrumAnalyzer_GetInputBuffer(), skip the self-copy and only zero
   * the imaginary work array. */
  if (input != s_fft_real)
  {
    for (i = 0U; i < APP_FFT_SIZE; ++i)
    {
      s_fft_real[i] = input[i];
    }
  }
  for (i = 0U; i < APP_FFT_SIZE; ++i)
  {
    s_fft_imag[i] = 0.0f;
  }
  for (i = 1U; i < APP_FFT_SIZE; ++i)
  {
    uint32_t bit = APP_FFT_SIZE >> 1U;
    while ((j & bit) != 0U) { j ^= bit; bit >>= 1U; }
    j ^= bit;
    if (i < j)
    {
      float temp = s_fft_real[i]; s_fft_real[i] = s_fft_real[j]; s_fft_real[j] = temp;
      temp = s_fft_imag[i]; s_fft_imag[i] = s_fft_imag[j]; s_fft_imag[j] = temp;
    }
  }
  for (length = 2U; length <= APP_FFT_SIZE; length <<= 1U)
  {
    uint32_t half = length >> 1U;
    float angle = -TWO_PI_F / (float)length;
    float wlen_r = cosf(angle);
    float wlen_i = sinf(angle);
    uint32_t base;
    for (base = 0U; base < APP_FFT_SIZE; base += length)
    {
      float wr = 1.0f;
      float wi = 0.0f;
      uint32_t k;
      for (k = 0U; k < half; ++k)
      {
        uint32_t even = base + k;
        uint32_t odd = even + half;
        float tr = wr * s_fft_real[odd] - wi * s_fft_imag[odd];
        float ti = wr * s_fft_imag[odd] + wi * s_fft_real[odd];
        float ur = s_fft_real[even];
        float ui = s_fft_imag[even];
        float next_wr;
        s_fft_real[even] = ur + tr;
        s_fft_imag[even] = ui + ti;
        s_fft_real[odd] = ur - tr;
        s_fft_imag[odd] = ui - ti;
        next_wr = wr * wlen_r - wi * wlen_i;
        wi = wr * wlen_i + wi * wlen_r;
        wr = next_wr;
      }
    }
  }
}

static float bin_power(uint32_t bin)
{
  return (s_fft_real[bin] * s_fft_real[bin]) +
         (s_fft_imag[bin] * s_fft_imag[bin]);
}

void SpectrumAnalyzer_Transform(float *windowed, float *spectrum_rms)
{
  uint32_t bin;
  float coherent_gain = 0.5f;
  float scale = 2.0f / ((float)APP_FFT_SIZE * coherent_gain * 1.41421356237f);
  fft_radix2_real(windowed);
  spectrum_rms[0] = fabsf(s_fft_real[0]) / (float)APP_FFT_SIZE;
  for (bin = 1U; bin < (APP_FFT_SIZE / 2U); ++bin)
    spectrum_rms[bin] = sqrtf(bin_power(bin)) * scale;
  spectrum_rms[APP_FFT_SIZE / 2U] =
      fabsf(s_fft_real[APP_FFT_SIZE / 2U]) /
      ((float)APP_FFT_SIZE * coherent_gain);
}

float SpectrumAnalyzer_BandRms(uint32_t center_bin, uint32_t radius)
{
  uint32_t first = (center_bin > radius) ? (center_bin - radius) : 1U;
  uint32_t last = center_bin + radius;
  uint32_t bin;
  float sum_power = 0.0f;
  float power_gain = SignalPreprocess_GetWindowPowerGain();
  if (last >= (APP_FFT_SIZE / 2U)) last = (APP_FFT_SIZE / 2U) - 1U;
  for (bin = first; bin <= last; ++bin) sum_power += bin_power(bin);
  return (1.41421356237f * sqrtf(sum_power)) /
         ((float)APP_FFT_SIZE * sqrtf(power_gain));
}

float SpectrumAnalyzer_RmsExcludingBand(uint32_t first_bin, uint32_t last_bin,
                                        uint32_t excluded_center,
                                        uint32_t excluded_radius)
{
  uint32_t bin;
  uint32_t excluded_first = (excluded_center > excluded_radius) ?
                            (excluded_center - excluded_radius) : 1U;
  uint32_t excluded_last = excluded_center + excluded_radius;
  float sum_power = 0.0f;
  float power_gain = SignalPreprocess_GetWindowPowerGain();
  if (first_bin < 1U) first_bin = 1U;
  if (last_bin >= (APP_FFT_SIZE / 2U)) last_bin = (APP_FFT_SIZE / 2U) - 1U;
  for (bin = first_bin; bin <= last_bin; ++bin)
    if ((bin < excluded_first) || (bin > excluded_last)) sum_power += bin_power(bin);
  return (1.41421356237f * sqrtf(sum_power)) /
         ((float)APP_FFT_SIZE * sqrtf(power_gain));
}

float SpectrumAnalyzer_InterpolateBin(uint32_t center_bin)
{
  float left, center, right, denominator, delta = 0.0f;
  if ((center_bin == 0U) || (center_bin >= (APP_FFT_SIZE / 2U))) return (float)center_bin;
  left = sqrtf(bin_power(center_bin - 1U));
  center = sqrtf(bin_power(center_bin));
  right = sqrtf(bin_power(center_bin + 1U));
  denominator = left + (2.0f * center) + right;
  if (fabsf(denominator) > 1.0e-12f)
  {
    /* Three-bin estimator matched to the Hann analysis window. */
    delta = 2.0f * (right - left) / denominator;
    if (delta > 0.5f) delta = 0.5f;
    if (delta < -0.5f) delta = -0.5f;
  }
  return (float)center_bin + delta;
}

uint32_t SpectrumAnalyzer_FindPeak(uint32_t first_bin, uint32_t last_bin)
{
  uint32_t bin, best = first_bin;
  float best_power = 0.0f;
  if (last_bin >= (APP_FFT_SIZE / 2U)) last_bin = (APP_FFT_SIZE / 2U) - 1U;
  for (bin = first_bin; bin <= last_bin; ++bin)
  {
    float power = bin_power(bin);
    if (power > best_power) { best_power = power; best = bin; }
  }
  return best;
}
