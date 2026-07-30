#ifndef DISTORTION_ANALYZER_H
#define DISTORTION_ANALYZER_H

#include "measurement_types.h"

void DistortionAnalyzer_Run(float *windowed,
                            float *spectrum_rms,
                            measurement_result_t *result);

#endif
