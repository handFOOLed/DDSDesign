#ifndef AD9833_H
#define AD9833_H

#include <stdint.h>

typedef enum
{
  AD9833_WAVE_SINE = 0,
  AD9833_WAVE_RAMP,
  AD9833_WAVE_SQUARE
} AD9833_Waveform;

void AD9833_Init(void);
void AD9833_SetOutput(uint32_t freq_hz, AD9833_Waveform waveform);
const char *AD9833_WaveName(AD9833_Waveform waveform);

#endif
