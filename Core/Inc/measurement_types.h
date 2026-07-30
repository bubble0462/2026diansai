#ifndef MEASUREMENT_TYPES_H
#define MEASUREMENT_TYPES_H

#include <stdint.h>
#include "app_config.h"

typedef enum
{
  DISTORTION_LEVEL_UNKNOWN = 0,
  DISTORTION_LEVEL_LOW = 1,
  DISTORTION_LEVEL_MEDIUM = 2,
  DISTORTION_LEVEL_HIGH = 3
} distortion_level_t;

typedef enum
{
  DISTORTION_TYPE_UNKNOWN = 0,
  DISTORTION_TYPE_NONE = 1,
  DISTORTION_TYPE_EVEN_HARMONIC = 2,
  DISTORTION_TYPE_ODD_HARMONIC = 3,
  DISTORTION_TYPE_MIXED_HARMONIC = 4,
  DISTORTION_TYPE_NOISE_DOMINATED = 5,
  DISTORTION_TYPE_CLIPPING = 6
} distortion_type_t;

enum
{
  MEAS_FLAG_NONE                    = 0U,
  MEAS_FLAG_NO_SIGNAL               = (1UL << 0),
  MEAS_FLAG_TOO_WEAK                = (1UL << 1),
  MEAS_FLAG_CLIPPING                = (1UL << 2),
  MEAS_FLAG_DC_OVERLOAD             = (1UL << 3),
  MEAS_FLAG_OVERRUN                 = (1UL << 4),
  MEAS_FLAG_INSUFFICIENT_RESOLUTION = (1UL << 5),
  MEAS_FLAG_ALIAS_RISK              = (1UL << 6)
};

typedef struct
{
  uint8_t order;
  uint8_t valid;
  uint16_t reserved;
  float theoretical_frequency_hz;
  float measured_frequency_hz;
  float amplitude_rms_v;
  float relative_db;
} harmonic_result_t;

typedef struct
{
  uint32_t sequence;
  float sample_rate_hz;
  float adc_reference_voltage_v;
  uint16_t fft_size;
  uint16_t spectrum_bins;
  float frequency_resolution_hz;
  float dc_voltage_v;
  float input_rms_v;
  float fundamental_frequency_hz;
  float fundamental_rms_v;
  float thd_percent;
  float thdn_percent;
  uint32_t flags;
  uint32_t overrun_count;
  uint8_t harmonic_count;
  uint8_t distortion_level;
  uint8_t distortion_type;
  uint8_t reserved;
  harmonic_result_t harmonics[APP_HARMONIC_SLOTS];
} measurement_result_t;

#endif
