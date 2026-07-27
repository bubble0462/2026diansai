#include "AD9959.h"
#include "delay.h"

/* REFCLK 25 MHz x PLL 20 = 500 MHz system clock (diansai-basic) */
#define system_clk 500000000.0

#define CSR 0x00U
#define FR1 0x01U

void AD9959_Init(void)
{
    uint8_t all_channels = 0xF0U;
    uint8_t function_register_1[3] = {0xD0U, 0x00U, 0x00U};

    PWR_0;
    CS_1;
    SCLK_0;
    IO_UPDATE_0;
    PS0_0;
    PS1_0;
    PS2_0;
    PS3_0;
    SDIO0_0;
    SDIO1_0;
    SDIO2_0;
    SDIO3_0;

    /* The module reference oscillator can start later than the MCU. */
    RESET_0;
    Delay_ms(100U);
    RESET_1;
    Delay_ms(1U);
    RESET_0;
    Delay_ms(10U);

    /* Select 1-bit SDIO, enable all write targets, then start PLL x20. */
    WriteToAD9959ViaSpi(CSR, 1U, &all_channels, 0U);
    WriteToAD9959ViaSpi(FR1, 3U, function_register_1, 1U);
    Delay_ms(10U);
}

void WrFrequencyTuningWorddata(double f, uint8_t *ChannelFrequencyTuningWorddata)
{
    uint32_t y;
    double x = 4294967296.0 / system_clk;

    f = f * x;
    y = (uint32_t)f;

    ChannelFrequencyTuningWorddata[0] = (uint8_t)(y >> 24);
    ChannelFrequencyTuningWorddata[1] = (uint8_t)(y >> 16);
    ChannelFrequencyTuningWorddata[2] = (uint8_t)(y >> 8);
    ChannelFrequencyTuningWorddata[3] = (uint8_t)(y >> 0);
}

void IO_update(void)
{
    IO_UPDATE_0;
    Delay_us(2U);
    IO_UPDATE_1;
    Delay_us(2U);
    IO_UPDATE_0;
}

void WriteToAD9959ViaSpi(uint8_t RegisterAddress, uint8_t NumberofRegisters,
                         uint8_t *RegisterData, uint8_t temp)
{
    uint8_t ControlValue = RegisterAddress;
    uint8_t ValueToWrite;
    uint8_t RegisterIndex;
    uint8_t i;

    CS_0;

    for (i = 0U; i < 8U; i++)
    {
        SCLK_0;
        if ((ControlValue & 0x80U) == 0x80U)
        {
            SDIO0_1;
        }
        else
        {
            SDIO0_0;
        }
        SCLK_1;
        ControlValue <<= 1;
    }
    SCLK_0;

    for (RegisterIndex = 0U; RegisterIndex < NumberofRegisters; RegisterIndex++)
    {
        ValueToWrite = RegisterData[RegisterIndex];
        for (i = 0U; i < 8U; i++)
        {
            SCLK_0;
            if ((ValueToWrite & 0x80U) == 0x80U)
            {
                SDIO0_1;
            }
            else
            {
                SDIO0_0;
            }
            SCLK_1;
            ValueToWrite <<= 1;
        }
        SCLK_0;
    }

    CS_1;

    if (temp == 1U)
    {
        IO_update();
    }
}

void WrPhaseOffsetTuningWorddata(double f, uint8_t *ChannelPhaseOffsetTuningWorddata)
{
    uint32_t y;
    double x = 16384.0 / 360.0;

    f = f * x;
    y = (uint32_t)f;

    ChannelPhaseOffsetTuningWorddata[0] = (uint8_t)(y >> 8);
    ChannelPhaseOffsetTuningWorddata[1] = (uint8_t)(y >> 0);
}
