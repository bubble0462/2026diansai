#include "AD9959_Outset.h"
#include "AD9959.h"
#include "delay.h"
#include <stdint.h>

#define CSR     0x00U
#define FR1     0x01U
#define CFR     0x03U
#define CFTW0   0x04U
#define ACR     0x06U

static void AD9959_EnableChannel(uint8_t channel)
{
    uint8_t csr = 0x10U;

    if (channel > 3U)
    {
        channel = 0U;
    }

    csr = (uint8_t)(0x10U << channel);
    WriteToAD9959ViaSpi(CSR, 1U, &csr, 0U);
}

void AD9959_enablechannel0(void)
{
    AD9959_EnableChannel(0U);
}

void AD9959_enablechannel1(void)
{
    AD9959_EnableChannel(1U);
}

void AD9959_enablechannel2(void)
{
    AD9959_EnableChannel(2U);
}

void AD9959_enablechannel3(void)
{
    AD9959_EnableChannel(3U);
}

void AD9959_Setwavefrequency(double f)
{
    uint8_t ChannelFrequencyTuningWord1data[4];
    uint8_t ChannelFunctionRegisterdata[3] = {0x00U, 0x23U, 0x35U};
    uint8_t FunctionRegister1data[3] = {0xD0U, 0x00U, 0x00U};

    WriteToAD9959ViaSpi(FR1, 3U, FunctionRegister1data, 0U);
    WriteToAD9959ViaSpi(CFR, 3U, ChannelFunctionRegisterdata, 0U);
    WrFrequencyTuningWorddata(f, ChannelFrequencyTuningWord1data);
    WriteToAD9959ViaSpi(CFTW0, 4U, ChannelFrequencyTuningWord1data, 1U);
}

void AD9959_Setwaveamplitute(double f, int a)
{
    uint8_t ChannelFrequencyTuningWorddata[4];
    uint8_t AmplitudeControlRegister[3];

    if (a > 1023)
    {
        a = 1023;
    }
    if (a < 0)
    {
        a = 0;
    }

    AmplitudeControlRegister[0] = 0x00U;
    AmplitudeControlRegister[1] = (uint8_t)(0x10U | ((uint16_t)a >> 8));
    AmplitudeControlRegister[2] = (uint8_t)((uint16_t)a & 0xFFU);

    WriteToAD9959ViaSpi(ACR, 3U, AmplitudeControlRegister, 0U);
    WrFrequencyTuningWorddata(f, ChannelFrequencyTuningWorddata);
    WriteToAD9959ViaSpi(CFTW0, 4U, ChannelFrequencyTuningWorddata, 1U);
}

void AD9959_SetAmplitude(uint16_t amp)
{
    uint8_t acr_data[3];

    if (amp > 1023U)
    {
        amp = 1023U;
    }

    acr_data[0] = 0x00U;
    acr_data[1] = (uint8_t)(0x10U | (amp >> 8));
    acr_data[2] = (uint8_t)(amp & 0xFFU);

    WriteToAD9959ViaSpi(ACR, 3U, acr_data, 1U);
}

void AD9959_SelectChannelOnly(uint8_t channel, double freq_hz, uint16_t amp)
{
    uint8_t all_channels = 0xF0U;
    uint8_t channel_function[3] = {0x00U, 0x23U, 0x35U};
    uint8_t frequency_word[4];
    uint8_t muted_amplitude[3] = {0x00U, 0x10U, 0x00U};
    uint8_t active_amplitude[3];

    if (channel > 3U)
    {
        channel = 0U;
    }
    if (amp > 1023U)
    {
        amp = 1023U;
    }

    WrFrequencyTuningWorddata(freq_hz, frequency_word);
    active_amplitude[0] = 0x00U;
    active_amplitude[1] = (uint8_t)(0x10U | (amp >> 8));
    active_amplitude[2] = (uint8_t)(amp & 0xFFU);

    /* Stage identical single-tone settings and mute every channel. */
    WriteToAD9959ViaSpi(CSR, 1U, &all_channels, 0U);
    WriteToAD9959ViaSpi(CFR, 3U, channel_function, 0U);
    WriteToAD9959ViaSpi(CFTW0, 4U, frequency_word, 0U);
    WriteToAD9959ViaSpi(ACR, 3U, muted_amplitude, 0U);

    /* Overwrite only the requested channel's buffered amplitude, then latch. */
    AD9959_EnableChannel(channel);
    WriteToAD9959ViaSpi(ACR, 3U, active_amplitude, 1U);
    Delay_ms(1U);
}
