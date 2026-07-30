#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*
 * F407VGT6 distortion / waveform-reconstruction configuration.
 *
 * Experimental sampling chain: ADC1 + ADC2 dual regular interleaved on
 * PA1 (ADC12_IN1), free-running one-shot DMA capture.  Each ADC converts
 * at a combined measured rate of approximately 2.118 MSPS.
 * This precision-first setting gives each ADC 15 acquisition cycles and
 * keeps the alternating interleave spacing close to uniform (13/14 cycles).
 */
#define APP_ADC_INTERLEAVED_ENABLED  1
/* DWT: 2048 combined samples take about 139264 core cycles at 144 MHz. */
#define APP_ADC_SAMPLE_RATE_HZ       2117647.059f
#define APP_FFT_SIZE                 4096U
#define APP_ADC_FRAME_SAMPLES        APP_FFT_SIZE
#define APP_ADC_DMA_WORD_COUNT       (APP_ADC_FRAME_SAMPLES / 2U)
#define APP_DMA_SAMPLE_COUNT         APP_ADC_FRAME_SAMPLES
#define APP_SPECTRUM_BINS            ((APP_FFT_SIZE / 2U) + 1U)
#define APP_OUTPUT_SPECTRUM_BINS     ((APP_FFT_SIZE / 8U) + 1U)
#define APP_MAX_HARMONICS            5U
#define APP_HARMONIC_SLOTS           10U
#define APP_ADC_REFERENCE_V          3.300f
#define APP_ADC_FULL_SCALE           4095.0f
#define APP_ADC_VOLTAGE_GAIN         1.0f
#define APP_MIN_FREQUENCY_HZ         10000.0f
#define APP_MAX_FUNDAMENTAL_HZ       500000.0f
#define APP_MAX_FREQUENCY_HZ         2000000.0f
#define APP_EDGE_TOLERANCE_BINS      5U
#define APP_LOWPASS_TAP_COUNT        63U
#define APP_NO_SIGNAL_RMS_V          0.002f
/*
 * 50 mVpp sine is only 17.68 mVrms before the analogue-chain roll-off.
 * The old 20 mVrms weak-signal threshold therefore rejected a valid
 * minimum-level input, especially near 500 kHz.  Keep a conservative
 * margin above the measured noise floor while accepting the task range.
 */
#define APP_WEAK_SIGNAL_RMS_V        0.005f
#define APP_CLIP_LOW_CODE            4U
#define APP_CLIP_HIGH_CODE           4091U
#define APP_CLIP_SAMPLE_LIMIT        4U
#define APP_LOW_THD_PERCENT          1.0f
#define APP_HIGH_THD_PERCENT         5.0f
#define APP_HARMONIC_SNR_FACTOR      3.98107171f
#define APP_HARMONIC_MIN_DB          (-75.0f)
#define APP_TYPE_MIN_PERCENT         0.05f
#define APP_NOISE_POWER_RATIO        4.0f
#define APP_HARMONIC_POWER_RATIO     4.0f
#define APP_SPECTRUM_SEND_DIVIDER    1U
#define APP_UART_BAUDRATE            115200U
#define APP_WAVEFORM_MIN_HZ          APP_MIN_FREQUENCY_HZ
/* Capped to keep the two display snapshots (each carries this many uint16
 * waveform samples) within the 128 KB main SRAM alongside the FFT buffers.
 * The least-squares fit only consumes TJC_MAX_FIT_SAMPLES points anyway. */
#define APP_WAVEFORM_MAX_SAMPLES     2048U
#define APP_WAVEFORM_PERIODS         8U

/*
 * TJC serial display (diansai.HMI) and measurement calibration.
 * The waveform control id is 1 (verified on the H7 reference firmware).
 * Waveform points cover the screen width (799 pixels).
 */
#define APP_TJC_WAVEFORM_ID           1U
#define APP_TJC_WAVEFORM_POINTS       799U
#define APP_TJC_VALUE_PERIOD_MS       500U
#define APP_TJC_TRANSFER_TIMEOUT_MS   500U
#define APP_TJC_GRAPH_INVALID_FRAMES  3U
#define APP_VOLTAGE_GLOBAL_SCALE      1.0f
/*
 * Bench calibration at 100 kHz:
 *   250 mVpp -> 247.00 mVpp, 88.39 mVrms -> 85.45 mVrms.
 * No analogue front end was connected during this measurement, so do not
 * apply an assumed analogue low-pass response.
 */
#define APP_UPP_SCALE                 1.032803f
#define APP_RMS_SCALE                 1.034407f
#define APP_SPECTRUM_SCALE            1.0f
#define APP_FREQUENCY_SCALE           1.0f

/*
 * AD603 programmable-gain control via DAC channel 1 (PA4).
 * AD603: Gain(dB) = G_pin + 40 * (V_GPOS - V_GNEG).  The DAC drives
 * GNEG, so a larger DAC code lowers the gain.  The codes below are
 * placeholders that MUST be calibrated against a real analogue board
 * (signal source + oscilloscope) before APP_GAIN_AUTO_CONTROL_ENABLED
 * is turned on.  AUTO control stays disabled until calibration.
 */
#define APP_GAIN_AUTO_CONTROL_ENABLED 0
#define APP_GAIN_DAC_MAX_CODE         4095U
#define APP_GAIN_DAC_TEST_CODE        2048U
#define APP_GAIN_DAC_SAFE_CODE        2048U
#define APP_GAIN_DAC_DETECT_CODE      2048U
#define APP_GAIN_BAND_10_50_DAC_CODE    2048U
#define APP_GAIN_BAND_50_100_DAC_CODE   2048U
#define APP_GAIN_BAND_100_150_DAC_CODE  2048U
#define APP_GAIN_BAND_150_200_DAC_CODE  2048U
#define APP_GAIN_BAND_200_250_DAC_CODE  2048U
#define APP_GAIN_MIN_FREQUENCY_HZ     10000.0f
#define APP_GAIN_MAX_FREQUENCY_HZ     250000.0f
#define APP_GAIN_HYSTERESIS_BINS      5U
#define APP_GAIN_HYSTERESIS_MIN_HZ    2000.0f
#define APP_GAIN_SETTLE_MS            20U
#define APP_GAIN_DISCARD_FRAMES       2U
#define APP_GAIN_INVALID_FRAMES       8U
#define APP_GAIN_CONFIRM_FRAMES       3U
#define APP_GAIN_REVALIDATE_FRAMES    3U

#endif
