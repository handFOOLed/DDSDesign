#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "main.h"

/*
 * Hardware map. Adjust here if your wiring is different.
 *
 * AD9833  : bit-banged SPI, FSYNC/SCLK/SDATA/RESET on GPIOB.
 * AMP     : MCP41010-compatible digital potentiometer shares SCLK/SDATA.
 * OLED    : SSD1306 128x64, bit-banged I2C, address 0x3C.
 * ADC CH0 : PA0 measures the signal after the external circuit / COL input.
 * USART1  : PA9 TX, PA10 RX, 115200-8-N-1 for command input.
 * Keys    : active-low buttons with internal pull-ups.
 */

#define AD9833_FSYNC_GPIO_Port GPIOB
#define AD9833_FSYNC_Pin       GPIO_PIN_12
#define AD9833_SCLK_GPIO_Port  GPIOB
#define AD9833_SCLK_Pin        GPIO_PIN_13
#define AD9833_SDATA_GPIO_Port GPIOB
#define AD9833_SDATA_Pin       GPIO_PIN_15
#define AD9833_RESET_GPIO_Port GPIOB
#define AD9833_RESET_Pin       GPIO_PIN_14

#define AMP_CS_GPIO_Port       GPIOB
#define AMP_CS_Pin             GPIO_PIN_8

#define OLED_SCL_GPIO_Port     GPIOB
#define OLED_SCL_Pin           GPIO_PIN_6
#define OLED_SDA_GPIO_Port     GPIOB
#define OLED_SDA_Pin           GPIO_PIN_7

#define KEY_MODE_GPIO_Port     GPIOA
#define KEY_MODE_Pin           GPIO_PIN_1
#define KEY_WAVE_GPIO_Port     GPIOA
#define KEY_WAVE_Pin           GPIO_PIN_2
#define KEY_UP_GPIO_Port       GPIOA
#define KEY_UP_Pin             GPIO_PIN_3
#define KEY_DOWN_GPIO_Port     GPIOA
#define KEY_DOWN_Pin           GPIO_PIN_4

#define ADC_INPUT_CHANNEL      ADC_CHANNEL_0
#define ADC_INPUT_GPIO_Port    GPIOA
#define ADC_INPUT_Pin          GPIO_PIN_0

#define APP_UART_BAUDRATE      115200U
#define AD9833_MCLK_HZ         25000000UL
#define ADC_VREF_MV            3300U
#define GEN_VPP_MIN_MV         1000U
#define GEN_VPP_MAX_MV         3300U

/* SCAN mode:
 * 1 = PA0 receives the DC output of an external peak/envelope/RMS detector.
 *     This is the recommended setting for sweeps up to 10 MHz.
 * 0 = PA0 receives the biased AC waveform directly, useful only at lower
 *     frequencies where the STM32F103 ADC can sample enough of the waveform.
 */
#define SCAN_INPUT_USES_ENVELOPE 1U
#define SCAN_ENVELOPE_SAMPLES    32U

/* External amplitude path calibration. The firmware reports source-side
 * amplitude as measured_mV * VPP_GAIN_NUM / VPP_GAIN_DEN. Keep 1:1 until your
 * front-end is known.
 */
#define VPP_GAIN_NUM           1U
#define VPP_GAIN_DEN           1U

#endif
