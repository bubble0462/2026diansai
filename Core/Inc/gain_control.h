#ifndef GAIN_CONTROL_H
#define GAIN_CONTROL_H

#include <stdint.h>
#include "measurement_types.h"

typedef enum
{
  GAIN_BAND_INVALID = 0,
  GAIN_BAND_10_50_KHZ = 1,
  GAIN_BAND_50_100_KHZ = 2,
  GAIN_BAND_100_150_KHZ = 3,
  GAIN_BAND_150_200_KHZ = 4,
  GAIN_BAND_200_250_KHZ = 5
} gain_band_t;

typedef enum
{
  GAIN_CONTROL_DISABLED = 0,
  GAIN_CONTROL_DETECTING = 1,
  GAIN_CONTROL_CONFIRMING = 2,
  GAIN_CONTROL_DISCARDING = 3,
  GAIN_CONTROL_REVALIDATING = 4,
  GAIN_CONTROL_TRACKING = 5,
  GAIN_CONTROL_CLIP_SAFE = 6,
  GAIN_CONTROL_SAFE = 7,
  GAIN_CONTROL_FAULT = 8
} gain_control_state_t;

typedef struct
{
  uint8_t publish_result;
  uint16_t settle_delay_ms;
} gain_control_action_t;

typedef struct
{
  gain_control_state_t state;
  gain_band_t active_band;
  gain_band_t candidate_band;
  float filtered_frequency_hz;
  uint16_t dac_code;
  uint8_t candidate_count;
  uint8_t invalid_count;
  uint8_t discard_frames_remaining;
  uint8_t revalidate_count;
} gain_control_status_t;

int GainControl_Init(void);
gain_control_action_t GainControl_Process(const measurement_result_t *result);
const gain_control_status_t *GainControl_GetStatus(void);

#endif
