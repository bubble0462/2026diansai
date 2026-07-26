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
    uint8_t i;

    if (channel > 3U)
    {
        channel = 0U;
    }

    /* CSR only selects the write target; mute others for exclusive output. */
    for (i = 0U; i < 4U; i++)
    {
        AD9959_EnableChannel(i);
        if (i == channel)
        {
            AD9959_Setwavefrequency(freq_hz);
            AD9959_Setwaveamplitute(freq_hz, (int)amp);
        }
        else
        {
            AD9959_SetAmplitude(0U);
        }
    }

    /* Re-select active channel and re-apply for a clean final IO_UPDATE. */
    AD9959_EnableChannel(channel);
    AD9959_Setwaveamplitute(freq_hz, (int)amp);
    Delay_ms(1U);
}
