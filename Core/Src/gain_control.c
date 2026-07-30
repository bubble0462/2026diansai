#include "gain_control.h"
#include "app_config.h"
#include "programmable_gain.h"
#include <string.h>

#if APP_GAIN_AUTO_CONTROL_ENABLED && !APP_GAIN_DAC_CODES_CALIBRATED
#error "Set calibrated DAC codes before enabling automatic gain control"
#endif

#if (APP_GAIN_DAC_TEST_CODE > APP_GAIN_DAC_MAX_CODE) || \
    (APP_GAIN_DAC_SAFE_CODE > APP_GAIN_DAC_MAX_CODE) || \
    (APP_GAIN_DAC_DETECT_CODE > APP_GAIN_DAC_MAX_CODE) || \
    (APP_GAIN_BAND_10_50_DAC_CODE > APP_GAIN_DAC_MAX_CODE) || \
    (APP_GAIN_BAND_50_100_DAC_CODE > APP_GAIN_DAC_MAX_CODE) || \
    (APP_GAIN_BAND_100_150_DAC_CODE > APP_GAIN_DAC_MAX_CODE) || \
    (APP_GAIN_BAND_150_200_DAC_CODE > APP_GAIN_DAC_MAX_CODE) || \
    (APP_GAIN_BAND_200_250_DAC_CODE > APP_GAIN_DAC_MAX_CODE)
#error "A programmable-gain DAC code exceeds the 12-bit range"
#endif

#define FREQUENCY_HISTORY_COUNT 3U

static const uint16_t s_band_dac_codes[5] =
{
  APP_GAIN_BAND_10_50_DAC_CODE,
  APP_GAIN_BAND_50_100_DAC_CODE,
  APP_GAIN_BAND_100_150_DAC_CODE,
  APP_GAIN_BAND_150_200_DAC_CODE,
  APP_GAIN_BAND_200_250_DAC_CODE
};

static gain_control_status_t s_status;
static float s_frequency_history[FREQUENCY_HISTORY_COUNT];
static uint8_t s_frequency_count;
static uint8_t s_frequency_position;

static gain_control_action_t make_action(uint8_t publish, uint16_t delay_ms)
{
  gain_control_action_t action;
  action.publish_result = publish;
  action.settle_delay_ms = delay_ms;
  return action;
}

static void reset_frequency_tracking(void)
{
  memset(s_frequency_history, 0, sizeof(s_frequency_history));
  s_frequency_count = 0U;
  s_frequency_position = 0U;
  s_status.candidate_band = GAIN_BAND_INVALID;
  s_status.candidate_count = 0U;
  s_status.revalidate_count = 0U;
  s_status.filtered_frequency_hz = 0.0f;
}

static float median_frequency(void)
{
  float a = s_frequency_history[0];
  float b = s_frequency_history[1];
  float c = s_frequency_history[2];
  float temporary;

  if (a > b)
  {
    temporary = a;
    a = b;
    b = temporary;
  }
  if (b > c)
  {
    temporary = b;
    b = c;
    c = temporary;
  }
  if (a > b)
  {
    b = a;
  }
  return b;
}

static uint8_t add_frequency(float frequency_hz)
{
  s_frequency_history[s_frequency_position] = frequency_hz;
  s_frequency_position = (uint8_t)((s_frequency_position + 1U) %
                                   FREQUENCY_HISTORY_COUNT);
  if (s_frequency_count < FREQUENCY_HISTORY_COUNT)
  {
    ++s_frequency_count;
  }
  if (s_frequency_count < FREQUENCY_HISTORY_COUNT)
  {
    return 0U;
  }
  s_status.filtered_frequency_hz = median_frequency();
  return 1U;
}

static gain_band_t nominal_band(float frequency_hz)
{
  if ((frequency_hz < APP_GAIN_MIN_FREQUENCY_HZ) ||
      (frequency_hz > APP_GAIN_MAX_FREQUENCY_HZ))
  {
    return GAIN_BAND_INVALID;
  }
  if (frequency_hz < 50000.0f)
  {
    return GAIN_BAND_10_50_KHZ;
  }
  if (frequency_hz < 100000.0f)
  {
    return GAIN_BAND_50_100_KHZ;
  }
  if (frequency_hz < 150000.0f)
  {
    return GAIN_BAND_100_150_KHZ;
  }
  if (frequency_hz < 200000.0f)
  {
    return GAIN_BAND_150_200_KHZ;
  }
  return GAIN_BAND_200_250_KHZ;
}

static float hysteresis_hz(const measurement_result_t *result)
{
  float value = APP_GAIN_HYSTERESIS_BINS * result->frequency_resolution_hz;
  return (value > APP_GAIN_HYSTERESIS_MIN_HZ) ?
         value : APP_GAIN_HYSTERESIS_MIN_HZ;
}

static gain_band_t band_with_hysteresis(float frequency_hz,
                                        float hysteresis)
{
  static const float lower_edges[5] =
  {
    10000.0f, 50000.0f, 100000.0f, 150000.0f, 200000.0f
  };
  static const float upper_edges[5] =
  {
    50000.0f, 100000.0f, 150000.0f, 200000.0f, 250000.0f
  };
  uint32_t index;

  if (s_status.active_band == GAIN_BAND_INVALID)
  {
    return nominal_band(frequency_hz);
  }
  index = (uint32_t)s_status.active_band - 1U;
  if ((frequency_hz >= (lower_edges[index] - hysteresis)) &&
      (frequency_hz <= (upper_edges[index] + hysteresis)))
  {
    return s_status.active_band;
  }
  return nominal_band(frequency_hz);
}

static uint8_t measurement_is_valid(const measurement_result_t *result)
{
  const uint32_t invalid_flags = MEAS_FLAG_NO_SIGNAL |
                                 MEAS_FLAG_TOO_WEAK |
                                 MEAS_FLAG_CLIPPING |
                                 MEAS_FLAG_DC_OVERLOAD;
  return (((result->flags & invalid_flags) == 0U) &&
          (result->fundamental_frequency_hz >= APP_GAIN_MIN_FREQUENCY_HZ) &&
          (result->fundamental_frequency_hz <= APP_GAIN_MAX_FREQUENCY_HZ) &&
          (result->fundamental_rms_v > 0.0f)) ? 1U : 0U;
}

static uint8_t write_code(uint16_t code)
{
  if (ProgrammableGain_GetCode() == code)
  {
    s_status.dac_code = code;
    return 0U;
  }
  if (ProgrammableGain_SetCode(code) != PROGRAMMABLE_GAIN_OK)
  {
    s_status.state = GAIN_CONTROL_FAULT;
    return 0U;
  }
  s_status.dac_code = code;
  return 1U;
}

static gain_control_action_t enter_safe_state(void)
{
  uint8_t changed = write_code(APP_GAIN_DAC_SAFE_CODE);
  reset_frequency_tracking();
  s_status.active_band = GAIN_BAND_INVALID;
  s_status.invalid_count = 0U;
  if (s_status.state != GAIN_CONTROL_FAULT)
  {
    s_status.state = GAIN_CONTROL_SAFE;
  }
  return make_action(0U, changed ? APP_GAIN_SETTLE_MS : 0U);
}

static gain_control_action_t enter_detect_state(void)
{
  uint8_t changed = write_code(APP_GAIN_DAC_DETECT_CODE);
  reset_frequency_tracking();
  s_status.active_band = GAIN_BAND_INVALID;
  s_status.invalid_count = 0U;
  if (s_status.state != GAIN_CONTROL_FAULT)
  {
    s_status.state = GAIN_CONTROL_DETECTING;
  }
  return make_action(0U, changed ? APP_GAIN_SETTLE_MS : 0U);
}

static gain_control_action_t enter_clip_safe_state(void)
{
  uint8_t changed = write_code(APP_GAIN_DAC_SAFE_CODE);
  reset_frequency_tracking();
  s_status.active_band = GAIN_BAND_INVALID;
  s_status.invalid_count = 0U;
  if (s_status.state != GAIN_CONTROL_FAULT)
  {
    s_status.state = GAIN_CONTROL_CLIP_SAFE;
  }
  return make_action(0U, changed ? APP_GAIN_SETTLE_MS : 0U);
}

static gain_control_action_t select_band(gain_band_t band)
{
  uint8_t changed;

  if ((band < GAIN_BAND_10_50_KHZ) ||
      (band > GAIN_BAND_200_250_KHZ))
  {
    return enter_safe_state();
  }
  changed = write_code(s_band_dac_codes[(uint32_t)band - 1U]);
  if (s_status.state == GAIN_CONTROL_FAULT)
  {
    return make_action(0U, 0U);
  }
  s_status.active_band = band;
  s_status.candidate_band = GAIN_BAND_INVALID;
  s_status.candidate_count = 0U;
  s_status.invalid_count = 0U;
  if (changed != 0U)
  {
    s_status.state = GAIN_CONTROL_DISCARDING;
    s_status.discard_frames_remaining = APP_GAIN_DISCARD_FRAMES;
    s_status.revalidate_count = 0U;
    return make_action(0U, APP_GAIN_SETTLE_MS);
  }
  s_status.state = GAIN_CONTROL_TRACKING;
  return make_action(1U, 0U);
}

int GainControl_Init(void)
{
  memset(&s_status, 0, sizeof(s_status));
  reset_frequency_tracking();
  s_status.dac_code = ProgrammableGain_GetCode();

#if APP_GAIN_AUTO_CONTROL_ENABLED
  if (ProgrammableGain_SetCode(APP_GAIN_DAC_DETECT_CODE) != PROGRAMMABLE_GAIN_OK)
  {
    s_status.state = GAIN_CONTROL_FAULT;
    return -1;
  }
  s_status.dac_code = APP_GAIN_DAC_DETECT_CODE;
  s_status.state = GAIN_CONTROL_DETECTING;
#else
  s_status.state = GAIN_CONTROL_DISABLED;
#endif
  return 0;
}

gain_control_action_t GainControl_Process(const measurement_result_t *result)
{
  gain_band_t requested_band;

  if (s_status.state == GAIN_CONTROL_DISABLED)
  {
    return make_action(1U, 0U);
  }
  if (s_status.state == GAIN_CONTROL_FAULT)
  {
    return make_action(0U, 0U);
  }
  if ((result->flags & MEAS_FLAG_CLIPPING) != 0U)
  {
    return enter_clip_safe_state();
  }
  if ((result->flags & MEAS_FLAG_DC_OVERLOAD) != 0U)
  {
    return enter_safe_state();
  }
  if (s_status.state == GAIN_CONTROL_CLIP_SAFE)
  {
    if ((result->flags & (MEAS_FLAG_NO_SIGNAL | MEAS_FLAG_TOO_WEAK)) != 0U)
    {
      if (s_status.invalid_count < 255U)
      {
        ++s_status.invalid_count;
      }
      if (s_status.invalid_count >= APP_GAIN_INVALID_FRAMES)
      {
        return enter_detect_state();
      }
    }
    else
    {
      s_status.invalid_count = 0U;
    }
    return make_action(0U, 0U);
  }
  if (s_status.state == GAIN_CONTROL_SAFE)
  {
    if (measurement_is_valid(result) != 0U)
    {
      return enter_detect_state();
    }
    if ((result->flags & (MEAS_FLAG_NO_SIGNAL | MEAS_FLAG_TOO_WEAK)) != 0U)
    {
      if (s_status.invalid_count < 255U)
      {
        ++s_status.invalid_count;
      }
      if (s_status.invalid_count >= APP_GAIN_INVALID_FRAMES)
      {
        return enter_detect_state();
      }
    }
    else
    {
      s_status.invalid_count = 0U;
    }
    return make_action(0U, 0U);
  }
  if (s_status.state == GAIN_CONTROL_DISCARDING)
  {
    if (s_status.discard_frames_remaining > 0U)
    {
      --s_status.discard_frames_remaining;
    }
    if (s_status.discard_frames_remaining == 0U)
    {
      s_status.state = GAIN_CONTROL_REVALIDATING;
      s_status.revalidate_count = 0U;
      reset_frequency_tracking();
    }
    return make_action(0U, 0U);
  }
  if (measurement_is_valid(result) == 0U)
  {
    if (s_status.invalid_count < 255U)
    {
      ++s_status.invalid_count;
    }
    s_status.candidate_band = GAIN_BAND_INVALID;
    s_status.candidate_count = 0U;
    if (s_status.invalid_count >= APP_GAIN_INVALID_FRAMES)
    {
      if ((result->flags & (MEAS_FLAG_NO_SIGNAL | MEAS_FLAG_TOO_WEAK)) != 0U)
      {
        return enter_detect_state();
      }
      return enter_safe_state();
    }
    return make_action(0U, 0U);
  }
  s_status.invalid_count = 0U;

  if (add_frequency(result->fundamental_frequency_hz) == 0U)
  {
    return make_action(0U, 0U);
  }
  requested_band = band_with_hysteresis(s_status.filtered_frequency_hz,
                                        hysteresis_hz(result));

  if (s_status.state == GAIN_CONTROL_REVALIDATING)
  {
    if (requested_band == s_status.active_band)
    {
      if (s_status.revalidate_count < 255U)
      {
        ++s_status.revalidate_count;
      }
      if (s_status.revalidate_count >= APP_GAIN_REVALIDATE_FRAMES)
      {
        s_status.state = GAIN_CONTROL_TRACKING;
        return make_action(1U, 0U);
      }
    }
    else
    {
      s_status.state = GAIN_CONTROL_CONFIRMING;
      s_status.candidate_band = requested_band;
      s_status.candidate_count = 1U;
      s_status.revalidate_count = 0U;
    }
    return make_action(0U, 0U);
  }

  if ((s_status.state == GAIN_CONTROL_TRACKING) &&
      (requested_band == s_status.active_band))
  {
    s_status.candidate_band = GAIN_BAND_INVALID;
    s_status.candidate_count = 0U;
    return make_action(1U, 0U);
  }
  if (requested_band == GAIN_BAND_INVALID)
  {
    return enter_safe_state();
  }
  if (requested_band != s_status.candidate_band)
  {
    s_status.candidate_band = requested_band;
    s_status.candidate_count = 1U;
    s_status.state = GAIN_CONTROL_CONFIRMING;
    return make_action(0U, 0U);
  }
  if (s_status.candidate_count < 255U)
  {
    ++s_status.candidate_count;
  }
  if (s_status.candidate_count < APP_GAIN_CONFIRM_FRAMES)
  {
    return make_action(0U, 0U);
  }
  return select_band(requested_band);
}

const gain_control_status_t *GainControl_GetStatus(void)
{
  return &s_status;
}
