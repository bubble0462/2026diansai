#ifndef SPECTRUM_ANALYZER_H
#define SPECTRUM_ANALYZER_H

#include <stdint.h>
#include "measurement_types.h"

int SpectrumAnalyzer_Init(void);
/* Returns the internal real-input buffer so callers can write the windowed
 * waveform directly into it, avoiding a separate 32 KB working array. */
float *SpectrumAnalyzer_GetInputBuffer(void);
void SpectrumAnalyzer_Transform(float *windowed, float *spectrum_rms);
float SpectrumAnalyzer_BandRms(uint32_t center_bin, uint32_t radius);
float SpectrumAnalyzer_RmsExcludingBand(uint32_t first_bin,
                                        uint32_t last_bin,
                                        uint32_t excluded_center,
                                        uint32_t excluded_radius);
float SpectrumAnalyzer_InterpolateBin(uint32_t center_bin);
uint32_t SpectrumAnalyzer_FindPeak(uint32_t first_bin, uint32_t last_bin);

#endif
