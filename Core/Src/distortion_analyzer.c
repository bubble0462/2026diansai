#include "distortion_analyzer.h"
#include "spectrum_analyzer.h"
#include <math.h>
#include <string.h>

#define HARMONIC_SEARCH_RADIUS  1U
#define HARMONIC_BAND_RADIUS    3U
#define FUNDAMENTAL_EXCLUSION_RADIUS 10U
#define NOISE_GUARD_RADIUS      4U
#define NOISE_OUTER_RADIUS      32U
#define NOISE_SAMPLE_CAPACITY   64U
#define SUBHARMONIC_MIN_RATIO   0.20f

static uint32_t find_local_peak(uint32_t center, uint32_t radius)
{
  uint32_t first = (center > radius) ? (center - radius) : 1U;
  uint32_t last = center + radius;
  return SpectrumAnalyzer_FindPeak(first, last);
}

static uint32_t absolute_difference(uint32_t left, uint32_t right)
{
  return (left > right) ? (left - right) : (right - left);
}

static uint32_t choose_fundamental_bin(uint32_t strongest_bin,
                                       uint32_t first_bin,
                                       uint32_t last_bin)
{
  float strongest_rms =
      SpectrumAnalyzer_BandRms(strongest_bin, HARMONIC_BAND_RADIUS);
  uint32_t order;

  /*
   * A legal G-problem waveform may have a harmonic larger than its
   * fundamental.  Searching only for the largest FFT peak then labels H2/H3
   * as f1 and produces octave jumps.  Check integer subharmonics from the
   * lowest possible candidate upward (largest divisor first).  The candidate
   * must contain a real spectral line of at least 20% of the strongest
   * component, so a pure 20 kHz sine is not mistaken for a noisy 10 kHz
   * fundamental.
   */
  for (order = APP_MAX_HARMONICS; order >= 2U; --order)
  {
    uint32_t expected_bin =
        (strongest_bin + (order / 2U)) / order;
    uint32_t candidate_bin;
    float candidate_rms;
    if ((expected_bin < first_bin) || (expected_bin > last_bin))
    {
      continue;
    }
    candidate_bin = find_local_peak(expected_bin, HARMONIC_SEARCH_RADIUS);
    if (absolute_difference(candidate_bin * order, strongest_bin) >
        (order + HARMONIC_SEARCH_RADIUS))
    {
      continue;
    }
    candidate_rms =
        SpectrumAnalyzer_BandRms(candidate_bin, HARMONIC_BAND_RADIUS);
    if (candidate_rms >= (strongest_rms * SUBHARMONIC_MIN_RATIO))
    {
      return candidate_bin;
    }
  }
  return strongest_bin;
}

static int is_harmonic_guard_bin(uint32_t bin, uint32_t fundamental_bin)
{
  uint32_t order;
  for (order = 1U; order <= APP_MAX_HARMONICS; ++order)
  {
    uint32_t harmonic_bin = fundamental_bin * order;
    if (harmonic_bin >= (APP_FFT_SIZE / 2U))
    {
      break;
    }
    if (absolute_difference(bin, harmonic_bin) <= NOISE_GUARD_RADIUS)
    {
      return 1;
    }
  }
  return 0;
}

static float median(float *values, uint32_t count)
{
  uint32_t i;
  for (i = 1U; i < count; ++i)
  {
    float value = values[i];
    uint32_t position = i;
    while ((position > 0U) && (values[position - 1U] > value))
    {
      values[position] = values[position - 1U];
      --position;
    }
    values[position] = value;
  }
  if ((count & 1U) != 0U)
  {
    return values[count / 2U];
  }
  return 0.5f * (values[(count / 2U) - 1U] + values[count / 2U]);
}

static float estimate_noise_floor(const float *spectrum_rms,
                                  uint32_t center_bin,
                                  uint32_t fundamental_bin,
                                  uint32_t first_bin,
                                  uint32_t last_bin)
{
  float samples[NOISE_SAMPLE_CAPACITY];
  uint32_t count = 0U;
  uint32_t local_first = (center_bin > NOISE_OUTER_RADIUS) ?
                         (center_bin - NOISE_OUTER_RADIUS) : first_bin;
  uint32_t local_last = center_bin + NOISE_OUTER_RADIUS;
  uint32_t bin;

  if (local_first < first_bin)
  {
    local_first = first_bin;
  }
  if (local_last > last_bin)
  {
    local_last = last_bin;
  }
  for (bin = local_first;
       (bin <= local_last) && (count < NOISE_SAMPLE_CAPACITY);
       ++bin)
  {
    if ((absolute_difference(bin, center_bin) <= NOISE_GUARD_RADIUS) ||
        is_harmonic_guard_bin(bin, fundamental_bin))
    {
      continue;
    }
    samples[count++] = spectrum_rms[bin];
  }

  if (count < 8U)
  {
    uint32_t span = last_bin - first_bin + 1U;
    uint32_t stride = (span / NOISE_SAMPLE_CAPACITY) + 1U;
    count = 0U;
    for (bin = first_bin;
         (bin <= last_bin) && (count < NOISE_SAMPLE_CAPACITY);
         bin += stride)
    {
      if (!is_harmonic_guard_bin(bin, fundamental_bin))
      {
        samples[count++] = spectrum_rms[bin];
      }
    }
  }
  return (count > 0U) ? median(samples, count) : 0.0f;
}

static uint8_t classify_distortion_type(const measurement_result_t *result,
                                        float even_square_sum,
                                        float odd_square_sum,
                                        float residual_rms)
{
  float harmonic_square_sum = even_square_sum + odd_square_sum;
  float noise_square = (residual_rms * residual_rms) - harmonic_square_sum;

  if ((result->flags & MEAS_FLAG_CLIPPING) != 0U)
  {
    return DISTORTION_TYPE_CLIPPING;
  }
  if ((result->flags & (MEAS_FLAG_NO_SIGNAL | MEAS_FLAG_TOO_WEAK |
                        MEAS_FLAG_DC_OVERLOAD | MEAS_FLAG_OVERRUN)) != 0U)
  {
    return DISTORTION_TYPE_UNKNOWN;
  }
  if ((result->thd_percent < APP_TYPE_MIN_PERCENT) &&
      (result->thdn_percent < APP_TYPE_MIN_PERCENT))
  {
    return DISTORTION_TYPE_NONE;
  }
  if (noise_square < 0.0f)
  {
    noise_square = 0.0f;
  }
  if ((result->thdn_percent >= APP_TYPE_MIN_PERCENT) &&
      ((harmonic_square_sum <= 0.0f) ||
       (noise_square >= (APP_NOISE_POWER_RATIO * harmonic_square_sum))))
  {
    return DISTORTION_TYPE_NOISE_DOMINATED;
  }
  if (harmonic_square_sum <= 0.0f)
  {
    return DISTORTION_TYPE_NONE;
  }
  if (even_square_sum >= (APP_HARMONIC_POWER_RATIO * odd_square_sum))
  {
    return DISTORTION_TYPE_EVEN_HARMONIC;
  }
  if (odd_square_sum >= (APP_HARMONIC_POWER_RATIO * even_square_sum))
  {
    return DISTORTION_TYPE_ODD_HARMONIC;
  }
  return DISTORTION_TYPE_MIXED_HARMONIC;
}

void DistortionAnalyzer_Run(float *windowed,
                            float *spectrum_rms,
                            measurement_result_t *result)
{
  uint32_t first_bin;
  uint32_t analysis_last_bin;
  uint32_t fundamental_last_bin;
  uint32_t strongest_bin;
  uint32_t fundamental_bin;
  uint32_t order;
  float delta_f = result->frequency_resolution_hz;
  float harmonic_square_sum = 0.0f;
  float even_square_sum = 0.0f;
  float odd_square_sum = 0.0f;
  float residual_rms;
  float minimum_harmonic_rms;

  memset(result->harmonics, 0, sizeof(result->harmonics));
  result->harmonic_count = 0U;
  result->distortion_level = DISTORTION_LEVEL_UNKNOWN;
  result->distortion_type = DISTORTION_TYPE_UNKNOWN;
  result->fundamental_frequency_hz = 0.0f;
  result->fundamental_rms_v = 0.0f;
  result->thd_percent = 0.0f;
  result->thdn_percent = 0.0f;

  SpectrumAnalyzer_Transform(windowed, spectrum_rms);
  if ((result->flags & (MEAS_FLAG_NO_SIGNAL | MEAS_FLAG_TOO_WEAK |
                        MEAS_FLAG_CLIPPING | MEAS_FLAG_DC_OVERLOAD |
                        MEAS_FLAG_OVERRUN)) != 0U)
  {
    if ((result->flags & MEAS_FLAG_CLIPPING) != 0U)
    {
      result->distortion_type = DISTORTION_TYPE_CLIPPING;
    }
    return;
  }

  first_bin = (uint32_t)floorf(APP_MIN_FREQUENCY_HZ / delta_f);
  if (first_bin < 1U) first_bin = 1U;
  analysis_last_bin = (uint32_t)floorf(APP_MAX_FREQUENCY_HZ / delta_f);
  fundamental_last_bin = (uint32_t)floorf(
      (APP_MAX_FUNDAMENTAL_HZ +
       ((float)APP_EDGE_TOLERANCE_BINS * delta_f)) / delta_f);
  if (fundamental_last_bin > analysis_last_bin)
  {
    fundamental_last_bin = analysis_last_bin;
  }
  strongest_bin = SpectrumAnalyzer_FindPeak(first_bin, fundamental_last_bin);
  fundamental_bin = choose_fundamental_bin(strongest_bin, first_bin,
                                            fundamental_last_bin);

  result->fundamental_frequency_hz =
      SpectrumAnalyzer_InterpolateBin(fundamental_bin) * delta_f;
  result->fundamental_rms_v = SpectrumAnalyzer_BandRms(fundamental_bin,
                                                       HARMONIC_BAND_RADIUS);
  if (result->fundamental_frequency_hz < (4.0f * delta_f))
  {
    result->flags |= MEAS_FLAG_INSUFFICIENT_RESOLUTION;
  }

  minimum_harmonic_rms = result->fundamental_rms_v *
                         powf(10.0f, APP_HARMONIC_MIN_DB / 20.0f);
  for (order = 1U; order <= APP_MAX_HARMONICS; ++order)
  {
    harmonic_result_t *harmonic = &result->harmonics[order - 1U];
    float theoretical = result->fundamental_frequency_hz * (float)order;
    uint32_t expected_bin;
    uint32_t peak_bin;
    float amplitude;
    float noise_floor;

    harmonic->order = (uint8_t)order;
    harmonic->theoretical_frequency_hz = theoretical;
    if ((theoretical > (APP_MAX_FREQUENCY_HZ +
                        ((float)APP_EDGE_TOLERANCE_BINS * delta_f))) ||
        (theoretical >= (0.5f * result->sample_rate_hz)))
    {
      harmonic->valid = 0U;
      continue;
    }

    expected_bin = (uint32_t)((theoretical / delta_f) + 0.5f);
    peak_bin = find_local_peak(expected_bin, HARMONIC_SEARCH_RADIUS);
    amplitude = SpectrumAnalyzer_BandRms(peak_bin, HARMONIC_BAND_RADIUS);

    if (order > 1U)
    {
      noise_floor = estimate_noise_floor(spectrum_rms, expected_bin,
                                         fundamental_bin, first_bin,
                                         analysis_last_bin);
      if ((spectrum_rms[peak_bin] <
           (noise_floor * APP_HARMONIC_SNR_FACTOR)) ||
          (amplitude < minimum_harmonic_rms))
      {
        harmonic->valid = 0U;
        continue;
      }
    }

    harmonic->valid = 1U;
    harmonic->measured_frequency_hz =
        SpectrumAnalyzer_InterpolateBin(peak_bin) * delta_f;
    harmonic->amplitude_rms_v = amplitude;
    if ((order > 1U) && (result->fundamental_rms_v > 0.0f))
    {
      harmonic->relative_db = 20.0f * log10f(amplitude /
                                              result->fundamental_rms_v);
      harmonic_square_sum += amplitude * amplitude;
      if ((order & 1U) == 0U)
      {
        even_square_sum += amplitude * amplitude;
      }
      else
      {
        odd_square_sum += amplitude * amplitude;
      }
    }
    ++result->harmonic_count;
  }

  if (result->fundamental_rms_v <= 0.0f)
  {
    result->flags |= MEAS_FLAG_NO_SIGNAL;
    return;
  }

  result->thd_percent = 100.0f * sqrtf(harmonic_square_sum) /
                        result->fundamental_rms_v;
  residual_rms = SpectrumAnalyzer_RmsExcludingBand(first_bin,
                                                    analysis_last_bin,
                                                    fundamental_bin,
                                                    FUNDAMENTAL_EXCLUSION_RADIUS);
  result->thdn_percent = 100.0f * residual_rms /
                         result->fundamental_rms_v;
  if (result->thdn_percent < result->thd_percent)
  {
    result->thdn_percent = result->thd_percent;
  }

  result->distortion_type = classify_distortion_type(result,
                                                      even_square_sum,
                                                      odd_square_sum,
                                                      residual_rms);

  if (result->thd_percent < APP_LOW_THD_PERCENT)
  {
    result->distortion_level = DISTORTION_LEVEL_LOW;
  }
  else if (result->thd_percent < APP_HIGH_THD_PERCENT)
  {
    result->distortion_level = DISTORTION_LEVEL_MEDIUM;
  }
  else
  {
    result->distortion_level = DISTORTION_LEVEL_HIGH;
  }
}
