#include "amplitude.h"
#include "app_config.h"

#define MCP41010_WRITE_DATA 0x11U

static uint8_t current_code;

static void amp_delay(void)
{
  for (volatile uint32_t i = 0; i < 12U; ++i)
  {
    __NOP();
  }
}

static void amp_write16(uint16_t word)
{
  HAL_GPIO_WritePin(AMP_CS_GPIO_Port, AMP_CS_Pin, GPIO_PIN_RESET);
  for (int8_t i = 15; i >= 0; --i)
  {
    HAL_GPIO_WritePin(AD9833_SCLK_GPIO_Port, AD9833_SCLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9833_SDATA_GPIO_Port, AD9833_SDATA_Pin,
                      (word & (1U << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    amp_delay();
    HAL_GPIO_WritePin(AD9833_SCLK_GPIO_Port, AD9833_SCLK_Pin, GPIO_PIN_SET);
    amp_delay();
  }
  HAL_GPIO_WritePin(AMP_CS_GPIO_Port, AMP_CS_Pin, GPIO_PIN_SET);
}

void Amplitude_Init(void)
{
  HAL_GPIO_WritePin(AMP_CS_GPIO_Port, AMP_CS_Pin, GPIO_PIN_SET);
  Amplitude_SetVpp(3300U);
}

void Amplitude_SetVpp(uint16_t vpp_mv)
{
  uint32_t code;

  if (vpp_mv < GEN_VPP_MIN_MV)
  {
    vpp_mv = GEN_VPP_MIN_MV;
  }
  if (vpp_mv > GEN_VPP_MAX_MV)
  {
    vpp_mv = GEN_VPP_MAX_MV;
  }

  code = ((uint32_t)(vpp_mv - GEN_VPP_MIN_MV) * 255UL) /
         (GEN_VPP_MAX_MV - GEN_VPP_MIN_MV);
  current_code = (uint8_t)code;
  amp_write16((uint16_t)((MCP41010_WRITE_DATA << 8) | current_code));
}

uint8_t Amplitude_GetCode(void)
{
  return current_code;
}
