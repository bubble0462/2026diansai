#ifndef PROGRAMMABLE_GAIN_H
#define PROGRAMMABLE_GAIN_H

#include <stdint.h>

typedef enum
{
  PROGRAMMABLE_GAIN_OK = 0,
  PROGRAMMABLE_GAIN_INVALID_CODE = 1,
  PROGRAMMABLE_GAIN_DAC_ERROR = 2
} programmable_gain_status_t;

programmable_gain_status_t ProgrammableGain_Init(void);
programmable_gain_status_t ProgrammableGain_SetCode(uint16_t code);
uint16_t ProgrammableGain_GetCode(void);

#endif
