#ifndef _AD9959_H
#define _AD9959_H

#include "main.h"
#include <stdint.h>

/* Keep the verified driver names bound to CubeMX's generated board pinout. */
#define CS_PORT         AD9959_CS_GPIO_Port
#define CS_PIN          AD9959_CS_Pin
#define SCLK_PORT       AD9959_SCLK_GPIO_Port
#define SCLK_PIN        AD9959_SCLK_Pin
#define IO_UPDATE_PORT  AD9959_IO_UPDATE_GPIO_Port
#define IO_UPDATE_PIN   AD9959_IO_UPDATE_Pin
#define PS0_PORT        AD9959_PS0_GPIO_Port
#define PS0_PIN         AD9959_PS0_Pin
#define PS1_PORT        AD9959_PS1_GPIO_Port
#define PS1_PIN         AD9959_PS1_Pin
#define PS2_PORT        AD9959_PS2_GPIO_Port
#define PS2_PIN         AD9959_PS2_Pin
#define PS3_PORT        AD9959_PS3_GPIO_Port
#define PS3_PIN         AD9959_PS3_Pin
#define RESET_PORT      AD9959_RESET_GPIO_Port
#define RESET_PIN       AD9959_RESET_Pin
#define PWR_PORT        AD9959_PWR_GPIO_Port
#define PWR_PIN         AD9959_PWR_Pin
#define SDIO0_PORT      AD9959_SDIO0_GPIO_Port
#define SDIO0_PIN       AD9959_SDIO0_Pin
#define SDIO1_PORT      AD9959_SDIO1_GPIO_Port
#define SDIO1_PIN       AD9959_SDIO1_Pin
#define SDIO2_PORT      AD9959_SDIO2_GPIO_Port
#define SDIO2_PIN       AD9959_SDIO2_Pin
#define SDIO3_PORT      AD9959_SDIO3_GPIO_Port
#define SDIO3_PIN       AD9959_SDIO3_Pin

#define SCLK_1      HAL_GPIO_WritePin(SCLK_PORT, SCLK_PIN, GPIO_PIN_SET)
#define SCLK_0      HAL_GPIO_WritePin(SCLK_PORT, SCLK_PIN, GPIO_PIN_RESET)
#define CS_1        HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET)
#define CS_0        HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET)
#define IO_UPDATE_1 HAL_GPIO_WritePin(IO_UPDATE_PORT, IO_UPDATE_PIN, GPIO_PIN_SET)
#define IO_UPDATE_0 HAL_GPIO_WritePin(IO_UPDATE_PORT, IO_UPDATE_PIN, GPIO_PIN_RESET)
#define SDIO0_1     HAL_GPIO_WritePin(SDIO0_PORT, SDIO0_PIN, GPIO_PIN_SET)
#define SDIO0_0     HAL_GPIO_WritePin(SDIO0_PORT, SDIO0_PIN, GPIO_PIN_RESET)
#define PS0_1       HAL_GPIO_WritePin(PS0_PORT, PS0_PIN, GPIO_PIN_SET)
#define PS0_0       HAL_GPIO_WritePin(PS0_PORT, PS0_PIN, GPIO_PIN_RESET)
#define PS1_1       HAL_GPIO_WritePin(PS1_PORT, PS1_PIN, GPIO_PIN_SET)
#define PS1_0       HAL_GPIO_WritePin(PS1_PORT, PS1_PIN, GPIO_PIN_RESET)
#define PS2_1       HAL_GPIO_WritePin(PS2_PORT, PS2_PIN, GPIO_PIN_SET)
#define PS2_0       HAL_GPIO_WritePin(PS2_PORT, PS2_PIN, GPIO_PIN_RESET)
#define PS3_1       HAL_GPIO_WritePin(PS3_PORT, PS3_PIN, GPIO_PIN_SET)
#define PS3_0       HAL_GPIO_WritePin(PS3_PORT, PS3_PIN, GPIO_PIN_RESET)
#define SDIO1_1     HAL_GPIO_WritePin(SDIO1_PORT, SDIO1_PIN, GPIO_PIN_SET)
#define SDIO1_0     HAL_GPIO_WritePin(SDIO1_PORT, SDIO1_PIN, GPIO_PIN_RESET)
#define SDIO2_1     HAL_GPIO_WritePin(SDIO2_PORT, SDIO2_PIN, GPIO_PIN_SET)
#define SDIO2_0     HAL_GPIO_WritePin(SDIO2_PORT, SDIO2_PIN, GPIO_PIN_RESET)
#define SDIO3_1     HAL_GPIO_WritePin(SDIO3_PORT, SDIO3_PIN, GPIO_PIN_SET)
#define SDIO3_0     HAL_GPIO_WritePin(SDIO3_PORT, SDIO3_PIN, GPIO_PIN_RESET)
#define PWR_1       HAL_GPIO_WritePin(PWR_PORT, PWR_PIN, GPIO_PIN_SET)
#define PWR_0       HAL_GPIO_WritePin(PWR_PORT, PWR_PIN, GPIO_PIN_RESET)
#define RESET_1     HAL_GPIO_WritePin(RESET_PORT, RESET_PIN, GPIO_PIN_SET)
#define RESET_0     HAL_GPIO_WritePin(RESET_PORT, RESET_PIN, GPIO_PIN_RESET)

void AD9959_Init(void);
void IO_update(void);
void WriteToAD9959ViaSpi(uint8_t RegisterAddress, uint8_t NumberofRegisters,
                         uint8_t *RegisterData, uint8_t temp);
void WrFrequencyTuningWorddata(double f, uint8_t *ChannelFrequencyTuningWorddata);
void WrPhaseOffsetTuningWorddata(double f, uint8_t *ChannelPhaseOffsetTuningWorddata);

#endif
