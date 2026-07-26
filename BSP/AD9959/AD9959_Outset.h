#ifndef _AD9959_OUTSET_H
#define _AD9959_OUTSET_H

#include <stdint.h>

void AD9959_enablechannel0(void);
void AD9959_enablechannel1(void);
void AD9959_enablechannel2(void);
void AD9959_enablechannel3(void);
void AD9959_Setwavefrequency(double f);
void AD9959_Setwaveamplitute(double f, int a);
void AD9959_SetAmplitude(uint16_t amp);
void AD9959_SelectChannelOnly(uint8_t channel, double freq_hz, uint16_t amp);

#endif
