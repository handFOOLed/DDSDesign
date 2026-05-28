#ifndef AMPLITUDE_H
#define AMPLITUDE_H

#include <stdint.h>

void Amplitude_Init(void);
void Amplitude_SetVpp(uint16_t vpp_mv);
uint8_t Amplitude_GetCode(void);

#endif
