#include "result_output.h"
#include "app_config.h"
#include "usart.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TWO_PI_F                 6.2831853071795864769f
#define SQRT_TWO_F               1.4142135623730950488f
#define TJC_COMMAND_BUFFER_SIZE  768U
#define TJC_MAX_COMPONENTS       3U
#define TJC_FIT_COEFFICIENTS     (1U + (2U * TJC_MAX_COMPONENTS))
#define TJC_MAX_FIT_SAMPLES      1024U
#define TJC_DISPLAY_HARMONIC_MIN_DB (-40.0f)
#define TJC_WAVEFORM_SYNTH_MAX_ORDER APP_MAX_HARMONICS
#define TJC_GRAPH_BASELINE       12U
#define TJC_UPP_AVG_SAMPLES      5U
#define TJC_RMS_AVG_SAMPLES      5U
#define TJC_UPP_PHASE_BINS       128U
#define TJC_TX_TIMEOUT_MARGIN_MS 50U
#define TJC_TX_TIMEOUT_MIN_MS    100U
#define TJC_RX_DMA_BUFFER_SIZE   512U

typedef struct
{
  measurement_result_t result;
  uint16_t waveform_count;
  uint16_t waveform[APP_WAVEFORM_MAX_SAMPLES];
} display_snapshot_t;

typedef struct
{
  uint8_t order;
  float frequency_hz;
  float model_frequency_hz;
  float rms_v;
} display_component_t;

typedef struct
{
  float samples[TJC_UPP_AVG_SAMPLES];
  float sum;
  uint8_t count;
  uint8_t position;
} value_average_t;

typedef enum
{
  TX_IDLE = 0,
  TX_VALUES,
  TX_CLEAR,
  TX_ADDT,
  TX_WAIT_READY,
  TX_GRAPH,
  TX_WAIT_FINISH,
  TX_REFRESH_START
} tx_state_t;

static display_snapshot_t s_snapshots[2];
static uint8_t s_pending_index;
static uint8_t s_pending_valid;
static uint8_t s_active_index;
static uint8_t s_active_valid;
static uint8_t s_tx_buffer[TJC_COMMAND_BUFFER_SIZE];
static uint8_t s_graph[APP_TJC_WAVEFORM_POINTS];
static uint8_t s_rx_dma_buffer[TJC_RX_DMA_BUFFER_SIZE];
static uint16_t s_rx_dma_read_index;
static uint8_t s_rx_event[5];
static uint8_t s_rx_event_count;
static uint8_t s_rx_tail[4];
static uint8_t s_rx_tail_count;
static volatile uint8_t s_tx_busy;
static volatile uint8_t s_tx_complete;
static volatile uint8_t s_ready_response;
static volatile uint8_t s_finish_response;
static volatile uint8_t s_uart_recovery_requested;
static volatile tx_state_t s_tx_state;
static volatile uint8_t s_periods = 3U;
static volatile uint8_t s_graph_mode;
static volatile uint8_t s_force_refresh = 1U;
static volatile uint8_t s_reset_display_scale;
static uint8_t s_values_pending;
static uint8_t s_graph_pending;
static uint16_t s_value_length;
static uint8_t s_graph_available;
static uint8_t s_graph_visible;
static uint8_t s_graph_invalid_frames;
static uint32_t s_next_update_ms;
static uint32_t s_state_deadline_ms;
static uint32_t s_dropped_count;
static uint32_t s_uart_recovery_count;
volatile uint32_t dbg_rx_complete;
volatile uint32_t dbg_rx_error;

static void consume_dma_rx(void);
static value_average_t s_upp_average;
static value_average_t s_rms_average;
static uint32_t s_upp_phase_sums[TJC_UPP_PHASE_BINS];
static uint16_t s_upp_phase_counts[TJC_UPP_PHASE_BINS];
static float s_display_scale_v;

static int time_reached(uint32_t now, uint32_t target)
{
  return ((int32_t)(now - target) >= 0) ? 1 : 0;
}

static int uart_start_receive(void)
{
  HAL_StatusTypeDef status =
      HAL_UART_Receive_DMA(&huart1, s_rx_dma_buffer,
                           TJC_RX_DMA_BUFFER_SIZE);
  if (status == HAL_OK)
  {
    s_rx_dma_read_index = 0U;
    return 0;
  }
  if (status != HAL_BUSY)
  {
    s_uart_recovery_requested = 1U;
    return -1;
  }
  return 0;
}

static int start_transmit(const uint8_t *data, uint16_t length,
                          tx_state_t state)
{
  uint32_t transfer_ms;
  if ((length == 0U) || (s_tx_busy != 0U))
  {
    return -1;
  }
  s_tx_state = state;
  s_tx_complete = 0U;
  s_tx_busy = 1U;
  transfer_ms = ((uint32_t)length * 10U * 1000U +
                 APP_UART_BAUDRATE - 1U) / APP_UART_BAUDRATE;
  transfer_ms += TJC_TX_TIMEOUT_MARGIN_MS;
  if (transfer_ms < TJC_TX_TIMEOUT_MIN_MS)
  {
    transfer_ms = TJC_TX_TIMEOUT_MIN_MS;
  }
  if (HAL_UART_Transmit(&huart1, data, length, transfer_ms) != HAL_OK)
  {
    s_tx_busy = 0U;
    s_tx_state = TX_IDLE;
    s_uart_recovery_requested = 1U;
    ++s_dropped_count;
    return -1;
  }
  s_tx_busy = 0U;
  s_tx_complete = 1U;
  return 0;
}

static void recover_uart(void)
{
  (void)HAL_UART_Abort(&huart1);
  s_tx_busy = 0U;
  s_tx_complete = 0U;
  s_ready_response = 0U;
  s_finish_response = 0U;
  s_tx_state = TX_IDLE;
  s_values_pending = 0U;
  s_graph_pending = 3U;
  s_force_refresh = 1U;
  s_uart_recovery_requested = 0U;
  ++s_uart_recovery_count;
  (void)uart_start_receive();
}

static uint16_t append_text(uint16_t length, const char *name,
                            const char *text)
{
  int written;
  if (length >= (TJC_COMMAND_BUFFER_SIZE - 4U)) return length;
  written = snprintf((char *)&s_tx_buffer[length],
                     TJC_COMMAND_BUFFER_SIZE - length,
                     "%s.txt=\"%s\"", name, text);
  if ((written <= 0) ||
      ((uint32_t)written > (TJC_COMMAND_BUFFER_SIZE - length - 3U)))
  {
    return length;
  }
  length = (uint16_t)(length + (uint16_t)written);
  s_tx_buffer[length++] = 0xFFU;
  s_tx_buffer[length++] = 0xFFU;
  s_tx_buffer[length++] = 0xFFU;
  return length;
}

static void format_fixed(char *buffer, size_t size, float value,
                         uint32_t decimals, const char *unit)
{
  uint32_t scale = (decimals == 3U) ? 1000U :
                   ((decimals == 2U) ? 100U : 10U);
  uint32_t scaled;
  if (!isfinite(value) || (value < 0.0f))
  {
    (void)snprintf(buffer, size, "-- %s", unit);
    return;
  }
  scaled = (uint32_t)(value * (float)scale + 0.5f);
  (void)snprintf(buffer, size, "%lu.%0*lu %s",
                 (unsigned long)(scaled / scale), (int)decimals,
                 (unsigned long)(scaled % scale), unit);
}

static uint8_t collect_components(const measurement_result_t *result,
                                  display_component_t *components)
{
  uint8_t count = 0U;
  uint32_t index;
  if (isfinite(result->fundamental_frequency_hz) &&
      isfinite(result->fundamental_rms_v) &&
      (result->fundamental_frequency_hz > 0.0f) &&
      (result->fundamental_rms_v > 0.0f))
  {
    components[count].order = 1U;
    components[count].frequency_hz = result->fundamental_frequency_hz;
    components[count].model_frequency_hz = result->fundamental_frequency_hz;
    components[count].rms_v = result->fundamental_rms_v *
                              APP_VOLTAGE_GLOBAL_SCALE *
                              APP_SPECTRUM_SCALE;
    ++count;
  }
  for (index = 1U; index < APP_MAX_HARMONICS; ++index)
  {
    const harmonic_result_t *harmonic = &result->harmonics[index];
    if ((harmonic->valid == 0U) ||
        !isfinite(harmonic->measured_frequency_hz) ||
        !isfinite(harmonic->amplitude_rms_v) ||
        (harmonic->measured_frequency_hz <= 0.0f) ||
        (harmonic->amplitude_rms_v <= 0.0f))
    {
      continue;
    }
    /* Do not let a noise-level harmonic deform the display curve. */
    if ((harmonic->order > 1U) &&
        (!isfinite(harmonic->relative_db) ||
         (harmonic->relative_db < TJC_DISPLAY_HARMONIC_MIN_DB)))
    {
      continue;
    }
    if (count >= TJC_MAX_COMPONENTS) break;
    components[count].order = harmonic->order;
    components[count].frequency_hz = harmonic->measured_frequency_hz;
    /* A periodic reconstruction must use exact integer harmonics. */
    components[count].model_frequency_hz =
        result->fundamental_frequency_hz * (float)harmonic->order;
    components[count].rms_v = harmonic->amplitude_rms_v *
                              APP_VOLTAGE_GLOBAL_SCALE *
                              APP_SPECTRUM_SCALE;
    ++count;
    if (count >= TJC_MAX_COMPONENTS) break;
  }
  return count;
}

static float update_average(value_average_t *average, float value,
                            uint8_t window_size)
{
  if (!isfinite(value) || (value < 0.0f)) return value;
  if (average->count < window_size)
  {
    ++average->count;
  }
  else
  {
    average->sum -= average->samples[average->position];
  }
  average->samples[average->position] = value;
  average->sum += value;
  average->position = (uint8_t)((average->position + 1U) %
                                window_size);
  return average->sum / (float)average->count;
}

static float update_trimmed_average(value_average_t *average, float value)
{
  float minimum;
  float maximum;
  float sum;
  float previous;
  float change_threshold;
  uint32_t index;
  if (!isfinite(value) || (value < 0.0f)) return value;
  if (average->count != 0U)
  {
    previous = average->sum / (float)average->count;
    change_threshold = fmaxf(0.005f, previous * 0.15f);
    if (fabsf(value - previous) > change_threshold)
    {
      memset(average, 0, sizeof(*average));
    }
  }
  (void)update_average(average, value, TJC_UPP_AVG_SAMPLES);
  if (average->count < 3U) return average->sum / (float)average->count;
  minimum = average->samples[0];
  maximum = average->samples[0];
  sum = average->samples[0];
  for (index = 1U; index < average->count; ++index)
  {
    float sample = average->samples[index];
    if (sample < minimum) minimum = sample;
    if (sample > maximum) maximum = sample;
    sum += sample;
  }
  return (sum - minimum - maximum) / (float)(average->count - 2U);
}

static float folded_peak_to_peak_codes(const display_snapshot_t *snapshot)
{
  float phase = 0.0f;
  float phase_step;
  float minimum = 4095.0f;
  float maximum = 0.0f;
  uint32_t populated = 0U;
  uint32_t index;
  if ((snapshot->waveform_count == 0U) ||
      (snapshot->result.sample_rate_hz <= 0.0f) ||
      (snapshot->result.fundamental_frequency_hz <= 0.0f))
  {
    return -1.0f;
  }
  memset(s_upp_phase_sums, 0, sizeof(s_upp_phase_sums));
  memset(s_upp_phase_counts, 0, sizeof(s_upp_phase_counts));
  phase_step = snapshot->result.fundamental_frequency_hz *
               (float)TJC_UPP_PHASE_BINS /
               snapshot->result.sample_rate_hz;
  for (index = 0U; index < snapshot->waveform_count; ++index)
  {
    uint32_t bin = (uint32_t)phase;
    if (bin >= TJC_UPP_PHASE_BINS) bin = TJC_UPP_PHASE_BINS - 1U;
    s_upp_phase_sums[bin] += snapshot->waveform[index];
    ++s_upp_phase_counts[bin];
    phase += phase_step;
    while (phase >= (float)TJC_UPP_PHASE_BINS)
    {
      phase -= (float)TJC_UPP_PHASE_BINS;
    }
  }
  for (index = 0U; index < TJC_UPP_PHASE_BINS; ++index)
  {
    if (s_upp_phase_counts[index] != 0U)
    {
      float value = (float)s_upp_phase_sums[index] /
                    (float)s_upp_phase_counts[index];
      if (value < minimum) minimum = value;
      if (value > maximum) maximum = value;
      ++populated;
    }
  }
  return (populated >= 3U) ? (maximum - minimum) : -1.0f;
}

static void reset_value_averages(void)
{
  memset(&s_upp_average, 0, sizeof(s_upp_average));
  memset(&s_rms_average, 0, sizeof(s_rms_average));
}

static int solve_linear(float matrix[TJC_FIT_COEFFICIENTS]
                                    [TJC_FIT_COEFFICIENTS],
                        float vector[TJC_FIT_COEFFICIENTS],
                        float solution[TJC_FIT_COEFFICIENTS],
                        uint32_t size)
{
  uint32_t column;
  uint32_t row;
  uint32_t pivot;
  for (column = 0U; column < size; ++column)
  {
    float largest = fabsf(matrix[column][column]);
    pivot = column;
    for (row = column + 1U; row < size; ++row)
    {
      float value = fabsf(matrix[row][column]);
      if (value > largest) { largest = value; pivot = row; }
    }
    if (largest < 1.0e-8f) return -1;
    if (pivot != column)
    {
      uint32_t k;
      float temporary;
      for (k = column; k < size; ++k)
      {
        temporary = matrix[column][k];
        matrix[column][k] = matrix[pivot][k];
        matrix[pivot][k] = temporary;
      }
      temporary = vector[column];
      vector[column] = vector[pivot];
      vector[pivot] = temporary;
    }
    for (row = column + 1U; row < size; ++row)
    {
      uint32_t k;
      float factor = matrix[row][column] / matrix[column][column];
      for (k = column; k < size; ++k)
      {
        matrix[row][k] -= factor * matrix[column][k];
      }
      vector[row] -= factor * vector[column];
    }
  }
  for (row = size; row-- > 0U;)
  {
    uint32_t k;
    float value = vector[row];
    for (k = row + 1U; k < size; ++k)
    {
      value -= matrix[row][k] * solution[k];
    }
    solution[row] = value / matrix[row][row];
  }
  return 0;
}

static int fit_components(const display_snapshot_t *snapshot,
                          const display_component_t *components,
                          uint8_t component_count,
                          float *coefficients)
{
  float matrix[TJC_FIT_COEFFICIENTS][TJC_FIT_COEFFICIENTS] = {{0.0f}};
  float vector[TJC_FIT_COEFFICIENTS] = {0.0f};
  float row[TJC_FIT_COEFFICIENTS];
  uint32_t coefficient_count;
  uint32_t fit_count;
  uint32_t sample;
  uint32_t i;
  uint32_t j;
  float volts_per_code;
  if ((component_count == 0U) ||
      (components[0].order != 1U) ||
      (snapshot->result.sample_rate_hz <= 0.0f)) return -1;
  coefficient_count = 1U + (2U * component_count);
  if (snapshot->waveform_count < coefficient_count) return -1;
  volts_per_code = snapshot->result.adc_reference_voltage_v *
                   APP_ADC_VOLTAGE_GAIN * APP_VOLTAGE_GLOBAL_SCALE /
                   APP_ADC_FULL_SCALE;
  fit_count = snapshot->waveform_count;
  if (fit_count > TJC_MAX_FIT_SAMPLES) fit_count = TJC_MAX_FIT_SAMPLES;
  for (sample = 0U; sample < fit_count; ++sample)
  {
    uint32_t source = (fit_count == 1U) ? 0U :
        (sample * (snapshot->waveform_count - 1U)) / (fit_count - 1U);
    float phase = TWO_PI_F * snapshot->result.fundamental_frequency_hz *
                  (float)source / snapshot->result.sample_rate_hz;
    float voltage = (float)snapshot->waveform[source] * volts_per_code;
    row[0] = 1.0f;
    /* Fit every detected component.  The task signal contains a fundamental
     * plus up to two harmonics, so Upp must be computed from their combined
     * amplitudes and relative phases rather than H1 alone. */
    for (i = 0U; i < component_count; ++i)
    {
      float harmonic_phase = phase * (float)components[i].order;
      row[1U + 2U * i] = sinf(harmonic_phase);
      row[2U + 2U * i] = cosf(harmonic_phase);
    }
    for (i = 0U; i < coefficient_count; ++i)
    {
      vector[i] += row[i] * voltage;
      for (j = 0U; j < coefficient_count; ++j)
      {
        matrix[i][j] += row[i] * row[j];
      }
    }
  }
  memset(coefficients, 0, sizeof(float) * TJC_FIT_COEFFICIENTS);
  return solve_linear(matrix, vector, coefficients, coefficient_count);
}

static float synthesized_value(const display_component_t *components,
                               uint8_t component_count,
                               const float *coefficients,
                               float time_s)
{
  float value = coefficients[0];
  uint32_t index;
  for (index = 0U; index < component_count; ++index)
  {
    if (components[index].order > TJC_WAVEFORM_SYNTH_MAX_ORDER) continue;
    float phase = TWO_PI_F *
                  components[index].model_frequency_hz * time_s;
    value += coefficients[1U + 2U * index] * sinf(phase) +
             coefficients[2U + 2U * index] * cosf(phase);
  }
  return value;
}

static void build_waveform_graph(const display_snapshot_t *snapshot,
                                 const display_component_t *components,
                                 uint8_t component_count,
                                 const float *coefficients)
{
  float duration_s = (float)s_periods /
                      snapshot->result.fundamental_frequency_hz;
  float start_time_s = 0.0f;
  float minimum = 1.0e9f;
  float maximum = -1.0e9f;
  float center;
  float span;
  uint32_t point;
  uint32_t index;
  for (index = 0U; index < component_count; ++index)
  {
    if (components[index].order == 1U)
    {
      float phase = atan2f(coefficients[2U + 2U * index],
                           coefficients[1U + 2U * index]);
      start_time_s = -phase /
                     (TWO_PI_F * components[index].model_frequency_hz);
      if (start_time_s < 0.0f)
      {
        start_time_s += 1.0f / components[index].model_frequency_hz;
      }
      break;
    }
  }
  for (point = 0U; point < APP_TJC_WAVEFORM_POINTS; ++point)
  {
    float time_s = start_time_s + duration_s * (float)point /
                   (float)(APP_TJC_WAVEFORM_POINTS - 1U);
    float value = synthesized_value(components, component_count,
                                    coefficients, time_s);
    if (value < minimum) minimum = value;
    if (value > maximum) maximum = value;
  }
  center = 0.5f * (minimum + maximum);
  span = fmaxf(maximum - minimum, 0.020f);
  if ((s_display_scale_v <= 0.0f) ||
      (fabsf(span - s_display_scale_v) > 0.20f * s_display_scale_v))
  {
    s_display_scale_v = span;
  }
  else
  {
    s_display_scale_v = 0.8f * s_display_scale_v + 0.2f * span;
  }
  for (point = 0U; point < APP_TJC_WAVEFORM_POINTS; ++point)
  {
    float time_s = start_time_s + duration_s * (float)point /
                   (float)(APP_TJC_WAVEFORM_POINTS - 1U);
    float value;
    float normalized;
    int display;
    value = synthesized_value(components, component_count,
                              coefficients, time_s);
    normalized = (value - center) / (0.65f * s_display_scale_v);
    if (normalized > 1.0f) normalized = 1.0f;
    if (normalized < -1.0f) normalized = -1.0f;
    display = (int)(128.0f + normalized * 111.0f + 0.5f);
    if (display < 0) display = 0;
    if (display > 255) display = 255;
    s_graph[point] = (uint8_t)display;
  }
}

static void build_spectrum_graph(const display_component_t *components,
                                 uint8_t component_count)
{
  float maximum = 0.0f;
  float axis_max_hz;
  uint32_t point;
  uint32_t index;
  memset(s_graph, TJC_GRAPH_BASELINE, sizeof(s_graph));
  for (index = 0U; index < component_count; ++index)
  {
    if (components[index].rms_v > maximum) maximum = components[index].rms_v;
  }
  if (maximum <= 0.0f) return;
  axis_max_hz = components[0].frequency_hz * (float)APP_MAX_HARMONICS;
  if (axis_max_hz <= 0.0f) return;
  for (index = 0U; index < component_count; ++index)
  {
    int x = (int)(components[index].frequency_hz *
                  (float)(APP_TJC_WAVEFORM_POINTS - 1U) / axis_max_hz + 0.5f);
    uint8_t height = (uint8_t)(16.0f +
        220.0f * components[index].rms_v / maximum);
    if ((x < 1) || (x >= (int)APP_TJC_WAVEFORM_POINTS - 1)) continue;
    point = (uint32_t)x;
    s_graph[point - 1U] = TJC_GRAPH_BASELINE;
    s_graph[point] = height;
    s_graph[point + 1U] = TJC_GRAPH_BASELINE;
  }
}

static void reverse_graph(void)
{
  uint32_t left;
  for (left = 0U; left < (APP_TJC_WAVEFORM_POINTS / 2U); ++left)
  {
    uint32_t right = APP_TJC_WAVEFORM_POINTS - 1U - left;
    uint8_t temporary = s_graph[left];
    s_graph[left] = s_graph[right];
    s_graph[right] = temporary;
  }
}

static float calculated_upp(const display_component_t *components,
                            uint8_t component_count,
                            const float *coefficients)
{
  float minimum = 1.0e9f;
  float maximum = -1.0e9f;
  uint32_t point;
  for (point = 0U; point < 2048U; ++point)
  {
    float time_s = (float)point /
        (2048.0f * components[0].model_frequency_hz);
    float value = synthesized_value(components, component_count,
                                    coefficients, time_s);
    if (value < minimum) minimum = value;
    if (value > maximum) maximum = value;
  }
  return maximum - minimum;
}

static uint16_t build_value_commands(const display_snapshot_t *snapshot,
                                     const display_component_t *components,
                                     uint8_t component_count,
                                     const float *coefficients,
                                     int fit_valid)
{
  char text[32];
  uint16_t length = 0U;
  uint32_t index;
  float direct_upp = 0.0f;
  float calculated_rms = 0.0f;
  int measurement_valid =
      ((snapshot->result.flags & (MEAS_FLAG_NO_SIGNAL | MEAS_FLAG_TOO_WEAK |
                                  MEAS_FLAG_CLIPPING |
                                  MEAS_FLAG_DC_OVERLOAD)) == 0U);
  if (measurement_valid)
  {
    format_fixed(text, sizeof(text),
                 snapshot->result.fundamental_frequency_hz *
                 APP_FREQUENCY_SCALE / 1000.0f,
                 3U, "kHz");
    length = append_text(length, "tf1", text);
    /*
     * Upp is taken directly from the captured ADC waveform (phase-folded
     * min/max).  This avoids the small but systematic loss of the FFT window
     * coherent-gain compensation that made the synthesised value read ~3.5%
     * low (241 mV instead of 250 mV).  The reconstructed waveform shown on
     * screen still uses the harmonic fit for a smooth shape.
     */
    if (snapshot->waveform_count != 0U)
    {
      if (fit_valid)
      {
        /*
         * Evaluate the fitted fundamental + harmonic model densely.  This
         * avoids missing extrema at 500 kHz (only 4.8 ADC samples/period)
         * and remains a true composite-waveform Upp when H2/H3 are present.
         */
        direct_upp = calculated_upp(components, component_count,
                                    coefficients) * APP_UPP_SCALE;
      }
      else
      {
        direct_upp = folded_peak_to_peak_codes(snapshot) *
                     snapshot->result.adc_reference_voltage_v *
                     APP_ADC_VOLTAGE_GAIN * APP_VOLTAGE_GLOBAL_SCALE *
                     APP_UPP_SCALE / APP_ADC_FULL_SCALE;
      }
      direct_upp = update_trimmed_average(&s_upp_average, direct_upp);
      format_fixed(text, sizeof(text), direct_upp * 1000.0f, 2U, "mV");
      length = append_text(length, "tupp", text);
    }
    else
    {
      length = append_text(length, "tupp", "-- mV");
      memset(&s_upp_average, 0, sizeof(s_upp_average));
    }
    {
      float rms = update_average(&s_rms_average,
          snapshot->result.input_rms_v * APP_VOLTAGE_GLOBAL_SCALE *
          APP_RMS_SCALE, TJC_RMS_AVG_SAMPLES);
      format_fixed(text, sizeof(text), rms * 1000.0f, 2U, "mV");
    }
    length = append_text(length, "turms", text);
  }
  else
  {
    length = append_text(length, "tf1", "-- kHz");
    length = append_text(length, "tupp", "-- mV");
    length = append_text(length, "turms", "-- mV");
    reset_value_averages();
  }
  for (index = 0U; index < TJC_MAX_COMPONENTS; ++index)
  {
    char name[5];
    char control[5];
    (void)snprintf(control, sizeof(control), "h%lun",
                   (unsigned long)(index + 1U));
    if (measurement_valid && (index < component_count))
    {
      (void)snprintf(name, sizeof(name), "H%u", components[index].order);
      length = append_text(length, control, name);
      control[2] = 'f';
      format_fixed(text, sizeof(text), components[index].frequency_hz *
                   APP_FREQUENCY_SCALE / 1000.0f,
                   3U, "kHz");
      length = append_text(length, control, text);
      control[2] = 'a';
      format_fixed(text, sizeof(text), components[index].rms_v *
                   SQRT_TWO_F * 1000.0f, 2U, "mV");
      length = append_text(length, control, text);
      calculated_rms += components[index].rms_v * components[index].rms_v;
    }
    else
    {
      length = append_text(length, control, "");
      control[2] = 'f';
      length = append_text(length, control, "-- kHz");
      control[2] = 'a';
      length = append_text(length, control, "-- mV");
    }
  }
  if (measurement_valid && (component_count != 0U))
  {
    calculated_rms = sqrtf(calculated_rms);
    if (fit_valid)
    {
      format_fixed(text, sizeof(text),
                    calculated_upp(components, component_count, coefficients) *
                    APP_UPP_SCALE * 1000.0f, 2U, "mV");
      length = append_text(length, "tcupp", text);
    }
    else
    {
      length = append_text(length, "tcupp", "-- mV");
    }
    format_fixed(text, sizeof(text), calculated_rms * 1000.0f,
                 2U, "mV");
    length = append_text(length, "tcrms", text);
  }
  else
  {
    length = append_text(length, "tcupp", "-- mV");
    length = append_text(length, "tcrms", "-- mV");
  }
  return length;
}

static void prepare_active_snapshot(void)
{
  display_snapshot_t *snapshot;
  display_component_t components[TJC_MAX_COMPONENTS];
  float coefficients[TJC_FIT_COEFFICIENTS] = {0.0f};
  uint8_t component_count;
  uint8_t graph_mode;
  uint8_t graph_updated = 0U;
  int fit_valid;
  uint16_t length;
  if (s_pending_valid == 0U) return;
  s_active_index = s_pending_index;
  s_active_valid = 1U;
  s_pending_valid = 0U;
  snapshot = &s_snapshots[s_active_index];
  graph_mode = s_graph_mode;
  if (s_reset_display_scale != 0U)
  {
    s_display_scale_v = 0.0f;
    s_reset_display_scale = 0U;
  }
  component_count =
      ((snapshot->result.fundamental_frequency_hz > 0.0f) &&
       (snapshot->result.fundamental_rms_v > 0.0f)) ?
      collect_components(&snapshot->result, components) : 0U;
  fit_valid = ((component_count != 0U) &&
      (snapshot->result.fundamental_frequency_hz > 0.0f) &&
      (fit_components(snapshot, components, component_count,
                       coefficients) == 0)) ? 1 : 0;
  memset(s_tx_buffer, 0, sizeof(s_tx_buffer));
  length = build_value_commands(snapshot, components, component_count,
                                coefficients, fit_valid);
  if (fit_valid && (graph_mode == 0U))
  {
    build_waveform_graph(snapshot, components, component_count, coefficients);
    s_graph_available = 1U;
    graph_updated = 1U;
  }
  else if ((graph_mode != 0U) && (component_count != 0U))
  {
    build_spectrum_graph(components, component_count);
    s_graph_available = 1U;
    graph_updated = 1U;
  }
  else
  {
    /*
     * Keep the last successful graph through short analysis dropouts.
     * A clear is only requested after several consecutive frames without
     * a usable component.  This prevents a single bad FFT/fit frame from
     * alternating cle/addt and visibly flashing the display.
     */
    if (s_graph_invalid_frames < APP_TJC_GRAPH_INVALID_FRAMES)
    {
      ++s_graph_invalid_frames;
    }
    if (s_graph_invalid_frames >= APP_TJC_GRAPH_INVALID_FRAMES)
    {
      memset(s_graph, 0, sizeof(s_graph));
      s_graph_available = 0U;
    }
  }
  if (graph_updated != 0U)
  {
    /*
     * TJC draws the addt payload in the opposite horizontal direction.
     * Reverse only a newly generated graph.  Reversing the retained graph
     * after a failed fit made it flip left/right on every failed frame.
     */
    reverse_graph();
    s_graph_invalid_frames = 0U;
  }
  s_value_length = length;
  s_values_pending = (length != 0U) ? 1U : 0U;
  if (graph_updated != 0U)
  {
    /* A full-width addt frame replaces the previous graph without flicker. */
    s_graph_pending = 2U;
  }
  else if (s_graph_available == 0U)
  {
    s_graph_pending = (s_graph_visible != 0U) ? 1U : 0U;
  }
  else
  {
    /* Retained graph is already visible; do not upload it again. */
    s_graph_pending = 0U;
  }
}

static void handle_event(void)
{
  if ((s_rx_event_count == 3U) && (s_rx_event[0] == 0xA5U) &&
      (s_rx_event[2] == 0x5AU))
  {
    if (s_rx_event[1] == 0x00U) s_force_refresh = 1U;
  }
  else if ((s_rx_event_count == 4U) && (s_rx_event[0] == 0xA5U) &&
           (s_rx_event[3] == 0x5AU))
  {
    if (s_rx_event[1] == 0x04U)
    {
      if ((s_rx_event[2] == 1U) || (s_rx_event[2] == 3U))
      {
        s_periods = s_rx_event[2];
        s_force_refresh = 1U;
      }
    }
    else if (s_rx_event[1] == 0x05U)
    {
      s_graph_mode = (s_rx_event[2] != 0U) ? 1U : 0U;
      s_reset_display_scale = 1U;
      s_force_refresh = 1U;
    }
  }
}

static void consume_rx_byte(uint8_t value)
{
  if (s_rx_tail_count < 4U) s_rx_tail[s_rx_tail_count++] = value;
  else
  {
    s_rx_tail[0] = s_rx_tail[1];
    s_rx_tail[1] = s_rx_tail[2];
    s_rx_tail[2] = s_rx_tail[3];
    s_rx_tail[3] = value;
  }
  if ((s_rx_tail_count == 4U) && (s_rx_tail[1] == 0xFFU) &&
      (s_rx_tail[2] == 0xFFU) && (s_rx_tail[3] == 0xFFU))
  {
    if (s_rx_tail[0] == 0xFEU) s_ready_response = 1U;
    if (s_rx_tail[0] == 0xFDU) s_finish_response = 1U;
    if (s_rx_tail[0] == 0x01U)
    {
      if ((s_tx_state == TX_ADDT) || (s_tx_state == TX_WAIT_READY))
      {
        s_ready_response = 1U;
      }
      else if ((s_tx_state == TX_GRAPH) || (s_tx_state == TX_WAIT_FINISH))
      {
        s_finish_response = 1U;
      }
    }
  }
  if (value == 0xA5U)
  {
    s_rx_event[0] = value;
    s_rx_event_count = 1U;
  }
  else if (s_rx_event_count != 0U)
  {
    if (s_rx_event_count < sizeof(s_rx_event))
    {
      s_rx_event[s_rx_event_count++] = value;
    }
    if (value == 0x5AU)
    {
      handle_event();
      s_rx_event_count = 0U;
    }
    else if (s_rx_event_count >= sizeof(s_rx_event))
    {
      s_rx_event_count = 0U;
    }
  }
}

static void consume_dma_rx(void)
{
  uint16_t write_index;
  if ((huart1.hdmarx == NULL) ||
      (huart1.RxState != HAL_UART_STATE_BUSY_RX))
  {
    return;
  }
  write_index = (uint16_t)(TJC_RX_DMA_BUFFER_SIZE -
      __HAL_DMA_GET_COUNTER(huart1.hdmarx));
  if (write_index >= TJC_RX_DMA_BUFFER_SIZE)
  {
    write_index = 0U;
  }
  while (s_rx_dma_read_index != write_index)
  {
    consume_rx_byte(s_rx_dma_buffer[s_rx_dma_read_index]);
    ++dbg_rx_complete;
    ++s_rx_dma_read_index;
    if (s_rx_dma_read_index >= TJC_RX_DMA_BUFFER_SIZE)
    {
      s_rx_dma_read_index = 0U;
    }
  }
}

void ResultOutput_Init(void)
{
  memset(s_snapshots, 0, sizeof(s_snapshots));
  memset(s_graph, 128, sizeof(s_graph));
  memset(s_tx_buffer, 0, sizeof(s_tx_buffer));
  memset(s_rx_event, 0, sizeof(s_rx_event));
  memset(s_rx_tail, 0, sizeof(s_rx_tail));
  memset(s_rx_dma_buffer, 0, sizeof(s_rx_dma_buffer));
  reset_value_averages();
  s_pending_index = 0U;
  s_pending_valid = 0U;
  s_active_index = 0U;
  s_active_valid = 0U;
  s_tx_busy = 0U;
  s_tx_complete = 0U;
  s_ready_response = 0U;
  s_finish_response = 0U;
  s_uart_recovery_requested = 0U;
  s_tx_state = TX_IDLE;
  s_periods = 3U;
  s_graph_mode = 0U;
  s_force_refresh = 1U;
  s_reset_display_scale = 0U;
  s_values_pending = 0U;
  s_graph_pending = 0U;
  s_value_length = 0U;
  s_rx_event_count = 0U;
  s_rx_tail_count = 0U;
  s_rx_dma_read_index = 0U;
  s_state_deadline_ms = 0U;
  s_display_scale_v = 0.0f;
  s_next_update_ms = HAL_GetTick() + 500U;
  s_dropped_count = 0U;
  s_uart_recovery_count = 0U;
  dbg_rx_complete = 0U;
  dbg_rx_error = 0U;
  s_graph_available = 0U;
  s_graph_visible = 0U;
  s_graph_invalid_frames = 0U;
  (void)uart_start_receive();
}

int ResultOutput_Send(const measurement_result_t *result,
                      const float *spectrum_rms,
                      const uint16_t *waveform_samples,
                      uint16_t waveform_sample_count)
{
  display_snapshot_t *snapshot;
  uint8_t target = (s_active_valid != 0U) ?
                   (uint8_t)(1U - s_active_index) :
                   (uint8_t)(1U - s_pending_index);
  (void)spectrum_rms;
  if (waveform_sample_count > APP_WAVEFORM_MAX_SAMPLES)
  {
    waveform_sample_count = APP_WAVEFORM_MAX_SAMPLES;
  }
  snapshot = &s_snapshots[target];
  snapshot->result = *result;
  snapshot->waveform_count = waveform_sample_count;
  if ((waveform_sample_count != 0U) && (waveform_samples != 0))
  {
    memcpy(snapshot->waveform, waveform_samples,
           (size_t)waveform_sample_count * sizeof(uint16_t));
  }
  if (s_pending_valid != 0U) ++s_dropped_count;
  s_pending_index = target;
  s_pending_valid = 1U;
  return 0;
}

void ResultOutput_Process(void)
{
  uint32_t now = HAL_GetTick();
  consume_dma_rx();
  if (s_uart_recovery_requested != 0U)
  {
    recover_uart();
  }
  if (s_tx_complete != 0U)
  {
    s_tx_complete = 0U;
    if (s_tx_state == TX_VALUES) s_tx_state = TX_IDLE;
    else if (s_tx_state == TX_CLEAR)
    {
      s_tx_state = TX_IDLE;
      s_graph_visible = 0U;
      s_graph_pending = 0U;
    }
    else if (s_tx_state == TX_ADDT)
    {
      s_tx_state = TX_WAIT_READY;
      s_state_deadline_ms = now + APP_TJC_TRANSFER_TIMEOUT_MS;
    }
    else if (s_tx_state == TX_GRAPH)
    {
      s_tx_state = TX_WAIT_FINISH;
      s_state_deadline_ms = now + APP_TJC_TRANSFER_TIMEOUT_MS;
    }
    else if (s_tx_state == TX_REFRESH_START)
    {
      s_tx_state = TX_IDLE;
      s_graph_visible = 1U;
      s_graph_pending = 0U;
    }
  }
  if ((s_tx_state == TX_WAIT_READY) && (s_ready_response != 0U))
  {
    s_ready_response = 0U;
    (void)start_transmit(s_graph, APP_TJC_WAVEFORM_POINTS, TX_GRAPH);
    return;
  }
  if ((s_tx_state == TX_WAIT_FINISH) && (s_finish_response != 0U))
  {
    s_finish_response = 0U;
    s_tx_state = TX_IDLE;
    s_graph_pending = 3U;
  }
  if ((s_tx_state == TX_WAIT_READY) &&
      time_reached(now, s_state_deadline_ms))
  {
    s_ready_response = 0U;
    if (start_transmit(s_graph, APP_TJC_WAVEFORM_POINTS, TX_GRAPH) == 0)
    {
      return;
    }
    s_tx_state = TX_IDLE;
    s_graph_pending = 3U;
    ++s_dropped_count;
  }
  if ((s_tx_state == TX_WAIT_FINISH) &&
      time_reached(now, s_state_deadline_ms))
  {
    s_tx_state = TX_IDLE;
    s_finish_response = 0U;
    s_graph_pending = 3U;
    ++s_dropped_count;
  }
  if ((s_tx_state != TX_IDLE) || (s_tx_busy != 0U)) return;
  if (s_graph_pending == 3U)
  {
    static const uint8_t refresh_start_command[] =
        {'r','e','f','_','s','t','a','r',0xFFU,0xFFU,0xFFU};
    memcpy(s_tx_buffer, refresh_start_command, sizeof(refresh_start_command));
    (void)start_transmit(s_tx_buffer, sizeof(refresh_start_command),
                         TX_REFRESH_START);
    return;
  }
  if ((s_pending_valid != 0U) && (s_values_pending == 0U) &&
      (s_graph_pending == 0U) &&
      (s_force_refresh != 0U || time_reached(now, s_next_update_ms)))
  {
    prepare_active_snapshot();
    s_force_refresh = 0U;
    s_next_update_ms = now + APP_TJC_VALUE_PERIOD_MS;
  }
  if (s_values_pending != 0U)
  {
    if (s_value_length != 0U)
    {
      s_values_pending = 0U;
      (void)start_transmit(s_tx_buffer, s_value_length, TX_VALUES);
      return;
    }
    s_values_pending = 0U;
  }
  if (s_graph_pending == 1U)
  {
    static const uint8_t clear_command[] =
        {'c','l','e',' ','1',',','0',0xFFU,0xFFU,0xFFU};
    memcpy(s_tx_buffer, clear_command, sizeof(clear_command));
    if (start_transmit(s_tx_buffer, sizeof(clear_command), TX_CLEAR) == 0)
    {
      s_graph_pending = 2U;
    }
    return;
  }
  if (s_graph_pending == 2U)
  {
    static const uint8_t refresh_stop_command[] =
        {'r','e','f','_','s','t','o','p',0xFFU,0xFFU,0xFFU};
    int written;
    uint16_t length;
    memcpy(s_tx_buffer, refresh_stop_command, sizeof(refresh_stop_command));
    length = sizeof(refresh_stop_command);
    written = snprintf((char *)&s_tx_buffer[length],
                       sizeof(s_tx_buffer) - length,
                       "addt %u,0,%u", APP_TJC_WAVEFORM_ID,
                       APP_TJC_WAVEFORM_POINTS);
    if (written <= 0) { s_graph_pending = 0U; return; }
    length = (uint16_t)(length + (uint16_t)written);
    s_tx_buffer[length++] = 0xFFU;
    s_tx_buffer[length++] = 0xFFU;
    s_tx_buffer[length++] = 0xFFU;
    s_ready_response = 0U;
    (void)start_transmit(s_tx_buffer, length, TX_ADDT);
  }
}

void ResultOutput_TxComplete(void)
{
  s_tx_busy = 0U;
  s_tx_complete = 1U;
}

void ResultOutput_RxComplete(void)
{
  consume_dma_rx();
}

void ResultOutput_UartError(void)
{
  ++dbg_rx_error;
  /* In DMA mode every UART error aborts reception in the HAL.  Defer the
   * restart to the main loop so recovery never races the DMA abort callback. */
  s_uart_recovery_requested = 1U;
}

uint32_t ResultOutput_GetDroppedCount(void)
{
  return s_dropped_count;
}

uint32_t ResultOutput_GetUartRecoveryCount(void)
{
  return s_uart_recovery_count;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart->Instance == USART1) ResultOutput_TxComplete();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart->Instance == USART1) ResultOutput_RxComplete();
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart->Instance == USART1) ResultOutput_RxComplete();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  if (uart->Instance == USART1) ResultOutput_UartError();
}
