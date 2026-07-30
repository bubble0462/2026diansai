#include "programmable_gain.h"
#include "app_config.h"
#include "dac.h"

/*
 * AD603 programmable-gain driver via DAC channel 1 (PA4).
 *
 * AD603 transfer: Gain(dB) = G_pin + 40 * (V_GPOS - V_GNEG).  The DAC
 * output drives GNEG, so a larger DAC code raises V_GNEG, shrinks the
 * differential control voltage and therefore REDUCES the gain.  The
 * actual code-to-gain mapping must be calibrated on the analogue board.
 */

static uint16_t s_dac_code;

programmable_gain_status_t ProgrammableGain_Init(void)
{
  if (APP_GAIN_DAC_TEST_CODE > APP_GAIN_DAC_MAX_CODE)
  {
    return PROGRAMMABLE_GAIN_INVALID_CODE;
  }
  if (HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R,
                       APP_GAIN_DAC_TEST_CODE) != HAL_OK)
  {
    return PROGRAMMABLE_GAIN_DAC_ERROR;
  }
  if (HAL_DAC_Start(&hdac, DAC_CHANNEL_1) != HAL_OK)
  {
    return PROGRAMMABLE_GAIN_DAC_ERROR;
  }
  s_dac_code = APP_GAIN_DAC_TEST_CODE;
  return PROGRAMMABLE_GAIN_OK;
}

programmable_gain_status_t ProgrammableGain_SetCode(uint16_t code)
{
  if (code > APP_GAIN_DAC_MAX_CODE)
  {
    return PROGRAMMABLE_GAIN_INVALID_CODE;
  }
  if (HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, code) != HAL_OK)
  {
    return PROGRAMMABLE_GAIN_DAC_ERROR;
  }
  s_dac_code = code;
  return PROGRAMMABLE_GAIN_OK;
}

uint16_t ProgrammableGain_GetCode(void)
{
  return s_dac_code;
}
