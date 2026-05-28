#include "ad9833.h"
#include "app_config.h"

#define AD9833_B28       0x2000U
#define AD9833_RESET     0x0100U
#define AD9833_OPBITEN   0x0020U
#define AD9833_DIV2      0x0008U
#define AD9833_MODE      0x0002U
#define AD9833_FREQ0     0x4000U

static void ad9833_delay(void)
{
  for (volatile uint32_t i = 0; i < 12U; ++i)
  {
    __NOP();
  }
}

static void ad9833_write16(uint16_t word)
{
  HAL_GPIO_WritePin(AD9833_FSYNC_GPIO_Port, AD9833_FSYNC_Pin, GPIO_PIN_RESET);
  for (int8_t i = 15; i >= 0; --i)
  {
    HAL_GPIO_WritePin(AD9833_SCLK_GPIO_Port, AD9833_SCLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9833_SDATA_GPIO_Port, AD9833_SDATA_Pin,
                      (word & (1U << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    ad9833_delay();
    HAL_GPIO_WritePin(AD9833_SCLK_GPIO_Port, AD9833_SCLK_Pin, GPIO_PIN_SET);
    ad9833_delay();
  }
  HAL_GPIO_WritePin(AD9833_FSYNC_GPIO_Port, AD9833_FSYNC_Pin, GPIO_PIN_SET);
}

void AD9833_Init(void)
{
  HAL_GPIO_WritePin(AD9833_FSYNC_GPIO_Port, AD9833_FSYNC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AD9833_SCLK_GPIO_Port, AD9833_SCLK_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AD9833_RESET_GPIO_Port, AD9833_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(2);
  ad9833_write16(AD9833_B28 | AD9833_RESET);
}

void AD9833_SetOutput(uint32_t freq_hz, AD9833_Waveform waveform)
{
  uint32_t tuning_word;
  uint16_t control = AD9833_B28;

  if (freq_hz < 1UL)
  {
    freq_hz = 1UL;
  }
  if (freq_hz > 10000000UL)
  {
    freq_hz = 10000000UL;
  }

  tuning_word = (uint32_t)(((uint64_t)freq_hz << 28) / AD9833_MCLK_HZ);

  switch (waveform)
  {
    case AD9833_WAVE_RAMP:
      control |= AD9833_MODE;
      break;
    case AD9833_WAVE_SQUARE:
      control |= AD9833_OPBITEN | AD9833_DIV2;
      break;
    case AD9833_WAVE_SINE:
    default:
      break;
  }

  ad9833_write16(AD9833_B28 | AD9833_RESET);
  ad9833_write16(AD9833_FREQ0 | (uint16_t)(tuning_word & 0x3FFFU));
  ad9833_write16(AD9833_FREQ0 | (uint16_t)((tuning_word >> 14) & 0x3FFFU));
  ad9833_write16(control);
  HAL_GPIO_WritePin(AD9833_RESET_GPIO_Port, AD9833_RESET_Pin, GPIO_PIN_RESET);
}

const char *AD9833_WaveName(AD9833_Waveform waveform)
{
  switch (waveform)
  {
    case AD9833_WAVE_RAMP:
      return "Ramp";
    case AD9833_WAVE_SQUARE:
      return "Square";
    case AD9833_WAVE_SINE:
    default:
      return "Sine";
  }
}
