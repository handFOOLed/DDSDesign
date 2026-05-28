#include "signal_app.h"
#include "ad9833.h"
#include "amplitude.h"
#include "app_config.h"
#include "ssd1306.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart1;

typedef enum
{
  MODE_GEN = 0,
  MODE_SCAN,
  MODE_COL
} AppMode;

typedef struct
{
  uint16_t min_adc;
  uint16_t max_adc;
  uint32_t vpp_mv;
  uint32_t freq_hz;
  AD9833_Waveform waveform;
  uint8_t edge_count;
  uint8_t valid;
} CaptureResult;

#define CAPTURE_SAMPLES       192U
#define SCAN_POINTS           64U
#define UART_LINE_LEN         48U
#define COL_MIN_SPAN_ADC      12U

static AppMode mode = MODE_GEN;
static uint32_t gen_freq_hz = 1000U;
static uint16_t gen_vpp_mv = 3300U;
static AD9833_Waveform gen_wave = AD9833_WAVE_SINE;
static uint32_t scan_fc_hz;
static uint16_t scan_threshold_mv;
static uint8_t scan_fc_x;
static uint32_t scan_freqs[SCAN_POINTS];
static uint16_t scan_vpp[SCAN_POINTS];
static char uart_line[UART_LINE_LEN];
static uint8_t uart_len;
static uint32_t next_refresh_ms;
static uint16_t capture_samples[CAPTURE_SAMPLES];
static uint32_t capture_ticks[CAPTURE_SAMPLES];

static CaptureResult capture_signal(uint32_t sample_us);
static void analyze_capture(CaptureResult *r);

static void delay_us(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000UL);
  while ((DWT->CYCCNT - start) < ticks)
  {
  }
}

static void uart_puts(const char *s)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 100U);
}

static const char *mode_name(AppMode m)
{
  switch (m)
  {
    case MODE_SCAN:
      return "SCAN";
    case MODE_COL:
      return "COL";
    case MODE_GEN:
    default:
      return "GEN";
  }
}

static uint16_t adc_read(void)
{
  HAL_ADC_Start(&hadc1);
  HAL_ADC_PollForConversion(&hadc1, 10U);
  return (uint16_t)HAL_ADC_GetValue(&hadc1);
}

static uint32_t adc_to_mv(uint16_t adc)
{
  return ((uint32_t)adc * ADC_VREF_MV) / 4095U;
}

static uint32_t scaled_vpp_mv(uint16_t min_adc, uint16_t max_adc)
{
  uint32_t mv = adc_to_mv((uint16_t)(max_adc - min_adc));
  return (mv * VPP_GAIN_NUM) / VPP_GAIN_DEN;
}

static uint32_t scaled_level_mv(uint32_t mv)
{
  return (mv * VPP_GAIN_NUM) / VPP_GAIN_DEN;
}

static uint32_t scan_measure_amplitude_mv(uint32_t source_freq_hz)
{
#if SCAN_INPUT_USES_ENVELOPE
  uint32_t sum = 0U;
  uint16_t min_adc = 4095U;
  uint16_t max_adc = 0U;
  uint32_t settle_ms = (source_freq_hz < 100U) ? 120U : 20U;

  HAL_Delay(settle_ms);
  for (uint8_t i = 0; i < SCAN_ENVELOPE_SAMPLES; ++i)
  {
    uint16_t v = adc_read();
    if (v < min_adc)
    {
      min_adc = v;
    }
    if (v > max_adc)
    {
      max_adc = v;
    }
    sum += v;
    HAL_Delay(1U);
  }

  /* A DC detector should be steady. If ripple is present, averaging keeps the
   * curve stable while calibration constants convert detector voltage to Vpp.
   */
  (void)min_adc;
  (void)max_adc;
  return scaled_level_mv(adc_to_mv((uint16_t)(sum / SCAN_ENVELOPE_SAMPLES)));
#else
  uint32_t sample_us = 1000000UL / (source_freq_hz * 12UL);
  CaptureResult r = capture_signal(sample_us);
  return r.vpp_mv;
#endif
}

static CaptureResult capture_signal(uint32_t sample_us)
{
  CaptureResult r;

  if (sample_us < 1U)
  {
    sample_us = 1U;
  }

  r.min_adc = 4095U;
  r.max_adc = 0U;
  r.freq_hz = 0U;
  r.waveform = AD9833_WAVE_SINE;
  r.edge_count = 0U;
  r.valid = 0U;

  for (uint16_t i = 0; i < CAPTURE_SAMPLES; ++i)
  {
    uint16_t v = adc_read();
    capture_ticks[i] = DWT->CYCCNT;
    capture_samples[i] = v;
    if (v < r.min_adc)
    {
      r.min_adc = v;
    }
    if (v > r.max_adc)
    {
      r.max_adc = v;
    }
    delay_us(sample_us);
  }

  analyze_capture(&r);
  return r;
}

static void analyze_capture(CaptureResult *r)
{
  uint32_t sum = 0U;
  uint32_t period_sum_ticks = 0U;
  uint16_t threshold;
  uint16_t span = (uint16_t)(r->max_adc - r->min_adc);
  uint16_t low_lim;
  uint16_t high_lim;
  uint16_t mid_lo;
  uint16_t mid_hi;
  uint16_t highs = 0U;
  uint16_t lows = 0U;
  uint16_t near_mid = 0U;
  uint16_t big_steps = 0U;
  uint16_t rising_steps = 0U;
  uint16_t falling_steps = 0U;
  uint16_t prev_edge = 0xFFFFU;

  r->vpp_mv = scaled_vpp_mv(r->min_adc, r->max_adc);
  r->freq_hz = 0U;
  r->waveform = AD9833_WAVE_SINE;
  r->edge_count = 0U;
  r->valid = (span >= COL_MIN_SPAN_ADC) ? 1U : 0U;

  for (uint16_t i = 0; i < CAPTURE_SAMPLES; ++i)
  {
    sum += capture_samples[i];
  }
  threshold = (uint16_t)(sum / CAPTURE_SAMPLES);

  for (uint16_t i = 1; i < CAPTURE_SAMPLES; ++i)
  {
    int16_t delta = (int16_t)capture_samples[i] - (int16_t)capture_samples[i - 1U];
    if (delta > (int16_t)(span / 5U))
    {
      big_steps++;
    }
    if (delta > (int16_t)(span / 80U + 1U))
    {
      rising_steps++;
    }
    else if (delta < -(int16_t)(span / 80U + 1U))
    {
      falling_steps++;
    }

    if (capture_samples[i - 1U] < threshold && capture_samples[i] >= threshold)
    {
      if (prev_edge != 0xFFFFU)
      {
        period_sum_ticks += capture_ticks[i] - capture_ticks[prev_edge];
      }
      prev_edge = i;
      r->edge_count++;
    }
  }

  if (r->edge_count >= 2U && period_sum_ticks > 0U)
  {
    uint32_t period_ticks = period_sum_ticks / (uint32_t)(r->edge_count - 1U);
    if (period_ticks > 0U)
    {
      r->freq_hz = SystemCoreClock / period_ticks;
    }
  }

  if (r->valid)
  {
    low_lim = (uint16_t)(r->min_adc + span / 5U);
    high_lim = (uint16_t)(r->max_adc - span / 5U);
    mid_lo = (uint16_t)(threshold - span / 10U);
    mid_hi = (uint16_t)(threshold + span / 10U);

    for (uint16_t i = 0; i < CAPTURE_SAMPLES; ++i)
    {
      if (capture_samples[i] <= low_lim)
      {
        lows++;
      }
      if (capture_samples[i] >= high_lim)
      {
        highs++;
      }
      if (capture_samples[i] >= mid_lo && capture_samples[i] <= mid_hi)
      {
        near_mid++;
      }
    }

    if ((highs + lows) > (CAPTURE_SAMPLES * 7U / 10U) || big_steps > (CAPTURE_SAMPLES / 20U))
    {
      r->waveform = AD9833_WAVE_SQUARE;
    }
    else if ((rising_steps > (CAPTURE_SAMPLES / 3U) && falling_steps < (CAPTURE_SAMPLES / 8U)) ||
             (falling_steps > (CAPTURE_SAMPLES / 3U) && rising_steps < (CAPTURE_SAMPLES / 8U)) ||
             near_mid > (CAPTURE_SAMPLES / 3U))
    {
      r->waveform = AD9833_WAVE_RAMP;
    }
    else
    {
      r->waveform = AD9833_WAVE_SINE;
    }
  }
}

static CaptureResult capture_col_signal(void)
{
  static const uint32_t sample_windows_us[] = {1U, 2U, 5U, 20U, 100U, 1000U, 10000U};
  CaptureResult best = {0};
  best.min_adc = 4095U;

  for (uint8_t i = 0; i < (sizeof(sample_windows_us) / sizeof(sample_windows_us[0])); ++i)
  {
    CaptureResult r = capture_signal(sample_windows_us[i]);
    if (!r.valid)
    {
      best = r;
      continue;
    }
    if (r.edge_count >= 2U && r.edge_count <= 12U)
    {
      return r;
    }
    if (r.freq_hz > 0U && (best.freq_hz == 0U || r.edge_count > best.edge_count))
    {
      best = r;
    }
    else if (best.freq_hz == 0U && r.vpp_mv > best.vpp_mv)
    {
      best = r;
    }
  }
  return best;
}

static uint32_t scan_freq_at(uint8_t index)
{
  static const uint32_t table[SCAN_POINTS] = {
    10,12,15,18,22,27,33,39,47,56,68,82,100,120,150,180,
    220,270,330,390,470,560,680,820,1000,1200,1500,1800,2200,2700,3300,3900,
    4700,5600,6800,8200,10000,12000,15000,18000,22000,27000,33000,39000,47000,56000,68000,82000,
    100000,120000,150000,180000,220000,270000,330000,390000,470000,560000,680000,820000,1000000,2200000,4700000,10000000
  };
  return table[index];
}

static void show_gen(void)
{
  char line[24];
  OLED_Clear();
  OLED_PrintAt(0, 0, "GEN Mode");
  snprintf(line, sizeof(line), "Wave:%s", AD9833_WaveName(gen_wave));
  OLED_PrintAt(0, 12, line);
  snprintf(line, sizeof(line), "Freq:%luHz", gen_freq_hz);
  OLED_PrintAt(0, 24, line);
  snprintf(line, sizeof(line), "Vpp:%umV", gen_vpp_mv);
  OLED_PrintAt(0, 36, line);
  OLED_PrintAt(0, 52, "UART: f/w/v/m");
  OLED_Update();
}

static void show_col(void)
{
  CaptureResult r = capture_col_signal();
  char line[24];
  OLED_Clear();
  OLED_PrintAt(0, 0, "COL Mode");
  snprintf(line, sizeof(line), "Wave:%s", r.valid ? AD9833_WaveName(r.waveform) : "Weak");
  OLED_PrintAt(0, 14, line);
  snprintf(line, sizeof(line), "Vpp:%lumV", r.vpp_mv);
  OLED_PrintAt(0, 28, line);
  snprintf(line, sizeof(line), "Freq:%luHz", r.freq_hz);
  OLED_PrintAt(0, 42, line);
  OLED_Update();
}

static void draw_scan_curve(void)
{
  uint16_t min_vpp = 65535U;
  uint16_t max_vpp = 0U;
  uint16_t span_vpp;
  char line[28];

  for (uint8_t i = 0; i < SCAN_POINTS; ++i)
  {
    if (scan_vpp[i] < min_vpp)
    {
      min_vpp = scan_vpp[i];
    }
    if (scan_vpp[i] > max_vpp)
    {
      max_vpp = scan_vpp[i];
    }
  }
  if (max_vpp <= min_vpp)
  {
    max_vpp = (uint16_t)(min_vpp + 1U);
  }
  span_vpp = (uint16_t)(max_vpp - min_vpp);

  OLED_Clear();
  OLED_DrawRect(0, 10, 128, 44);
  if (scan_threshold_mv >= min_vpp && scan_threshold_mv <= max_vpp)
  {
    uint8_t th_y = (uint8_t)(52U - (((uint32_t)(scan_threshold_mv - min_vpp) * 40U) / span_vpp));
    OLED_DrawLine(1, th_y, 126, th_y);
  }
  for (uint8_t i = 1; i < SCAN_POINTS; ++i)
  {
    uint8_t x0 = (uint8_t)(((uint16_t)(i - 1U) * 126U) / (SCAN_POINTS - 1U));
    uint8_t x1 = (uint8_t)(((uint16_t)i * 126U) / (SCAN_POINTS - 1U));
    uint8_t y0 = (uint8_t)(52U - (((uint32_t)(scan_vpp[i - 1U] - min_vpp) * 40U) / span_vpp));
    uint8_t y1 = (uint8_t)(52U - (((uint32_t)(scan_vpp[i] - min_vpp) * 40U) / span_vpp));
    OLED_DrawLine(x0, y0, x1, y1);
  }

  if (scan_fc_hz > 0U)
  {
    OLED_DrawLine(scan_fc_x, 10, scan_fc_x, 53);
  }

  snprintf(line, sizeof(line), "SCAN %u-%umV", min_vpp, max_vpp);
  OLED_PrintAt(0, 0, line);
  snprintf(line, sizeof(line), "Fc=%luHz", scan_fc_hz);
  OLED_PrintAt(0, 56, line);
  OLED_Update();
}

static void run_scan(void)
{
  uint16_t ref_vpp;
  scan_fc_hz = 0U;
  scan_fc_x = 0U;
  scan_threshold_mv = 0U;

  OLED_Clear();
  OLED_PrintAt(0, 0, "SCAN running...");
  OLED_Update();

  for (uint8_t i = 0; i < SCAN_POINTS; ++i)
  {
    uint32_t f = scan_freq_at(i);
    uint32_t amp_mv;
    scan_freqs[i] = f;
    AD9833_SetOutput(f, AD9833_WAVE_SINE);
    amp_mv = scan_measure_amplitude_mv(f);
    scan_vpp[i] = (uint16_t)((amp_mv > 65535U) ? 65535U : amp_mv);
  }

  ref_vpp = scan_vpp[0];
  scan_threshold_mv = (uint16_t)(((uint32_t)ref_vpp * 707U) / 1000U);
  for (uint8_t i = 1; i < SCAN_POINTS; ++i)
  {
    if (scan_vpp[i] <= scan_threshold_mv)
    {
      uint32_t f0 = scan_freqs[i - 1U];
      uint32_t f1 = scan_freqs[i];
      uint16_t a0 = scan_vpp[i - 1U];
      uint16_t a1 = scan_vpp[i];
      uint32_t ratio = 1000U;
      if (a0 > a1)
      {
        ratio = ((uint32_t)(a0 - scan_threshold_mv) * 1000UL) / (uint32_t)(a0 - a1);
      }
      if (ratio > 1000U)
      {
        ratio = 1000U;
      }
      scan_fc_hz = f0 + (((f1 - f0) * ratio) / 1000UL);
      scan_fc_x = (uint8_t)((((uint16_t)(i - 1U) * 126U) +
                             (((uint16_t)126U * (uint16_t)ratio) / 1000U)) /
                            (SCAN_POINTS - 1U));
      break;
    }
  }
  draw_scan_curve();
}

static uint8_t key_pressed(GPIO_TypeDef *port, uint16_t pin)
{
  static uint32_t last_ms;
  if (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET && (HAL_GetTick() - last_ms) > 180U)
  {
    last_ms = HAL_GetTick();
    return 1U;
  }
  return 0U;
}

static void apply_generator(void)
{
  AD9833_SetOutput(gen_freq_hz, gen_wave);
  Amplitude_SetVpp(gen_vpp_mv);
}

static void handle_keys(void)
{
  if (key_pressed(KEY_MODE_GPIO_Port, KEY_MODE_Pin))
  {
    mode = (AppMode)((mode + 1U) % 3U);
    if (mode == MODE_SCAN)
    {
      run_scan();
      next_refresh_ms = HAL_GetTick() + 2000U;
    }
  }
  if (mode == MODE_GEN)
  {
    if (key_pressed(KEY_WAVE_GPIO_Port, KEY_WAVE_Pin))
    {
      gen_wave = (AD9833_Waveform)((gen_wave + 1U) % 3U);
      apply_generator();
      show_gen();
    }
    if (key_pressed(KEY_UP_GPIO_Port, KEY_UP_Pin))
    {
      gen_freq_hz = (gen_freq_hz < 1000000UL) ? gen_freq_hz * 10UL : 10000000UL;
      apply_generator();
      show_gen();
    }
    if (key_pressed(KEY_DOWN_GPIO_Port, KEY_DOWN_Pin))
    {
      gen_freq_hz = (gen_freq_hz > 10UL) ? gen_freq_hz / 10UL : 1UL;
      apply_generator();
      show_gen();
    }
  }
}

static void print_help(void)
{
  uart_puts("\r\nCommands:\r\n");
  uart_puts("  m gen|scan|col\r\n");
  uart_puts("  f <1..10000000>\r\n");
  uart_puts("  w sine|ramp|square\r\n");
  uart_puts("  v <1000..3300>  (sets digital amplitude control)\r\n");
  uart_puts("  scan\r\n> ");
}

static void handle_command(char *line)
{
  char *arg = strchr(line, ' ');
  if (arg != NULL)
  {
    *arg++ = '\0';
  }

  if (strcmp(line, "m") == 0 && arg != NULL)
  {
    if (strcmp(arg, "gen") == 0)
    {
      mode = MODE_GEN;
      show_gen();
    }
    else if (strcmp(arg, "scan") == 0)
    {
      mode = MODE_SCAN;
      run_scan();
    }
    else if (strcmp(arg, "col") == 0)
    {
      mode = MODE_COL;
      show_col();
    }
  }
  else if (strcmp(line, "f") == 0 && arg != NULL)
  {
    uint32_t f = (uint32_t)strtoul(arg, NULL, 10);
    if (f < 1UL)
    {
      f = 1UL;
    }
    if (f > 10000000UL)
    {
      f = 10000000UL;
    }
    gen_freq_hz = f;
    apply_generator();
    show_gen();
  }
  else if (strcmp(line, "w") == 0 && arg != NULL)
  {
    if (strcmp(arg, "ramp") == 0)
    {
      gen_wave = AD9833_WAVE_RAMP;
    }
    else if (strcmp(arg, "square") == 0)
    {
      gen_wave = AD9833_WAVE_SQUARE;
    }
    else
    {
      gen_wave = AD9833_WAVE_SINE;
    }
    apply_generator();
    show_gen();
  }
  else if (strcmp(line, "v") == 0 && arg != NULL)
  {
    uint32_t v = (uint32_t)strtoul(arg, NULL, 10);
    if (v < GEN_VPP_MIN_MV)
    {
      v = GEN_VPP_MIN_MV;
    }
    if (v > GEN_VPP_MAX_MV)
    {
      v = GEN_VPP_MAX_MV;
    }
    gen_vpp_mv = (uint16_t)v;
    apply_generator();
    show_gen();
  }
  else if (strcmp(line, "scan") == 0)
  {
    mode = MODE_SCAN;
    run_scan();
  }
  else
  {
    print_help();
    return;
  }

  uart_puts("\r\nOK ");
  uart_puts(mode_name(mode));
  uart_puts("\r\n> ");
}

static void poll_uart(void)
{
  uint8_t ch;
  while (HAL_UART_Receive(&huart1, &ch, 1U, 0U) == HAL_OK)
  {
    if (ch == '\r' || ch == '\n')
    {
      if (uart_len > 0U)
      {
        uart_line[uart_len] = '\0';
        handle_command(uart_line);
        uart_len = 0U;
      }
    }
    else if (uart_len < (UART_LINE_LEN - 1U))
    {
      uart_line[uart_len++] = (char)ch;
      HAL_UART_Transmit(&huart1, &ch, 1U, 10U);
    }
  }
}

void SignalApp_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  AD9833_Init();
  Amplitude_Init();
  OLED_Init();
  HAL_ADCEx_Calibration_Start(&hadc1);
  apply_generator();
  show_gen();
  print_help();
}

void SignalApp_Run(void)
{
  handle_keys();
  poll_uart();

  if (HAL_GetTick() >= next_refresh_ms)
  {
    next_refresh_ms = HAL_GetTick() + ((mode == MODE_COL) ? 350U : 1000U);
    if (mode == MODE_GEN)
    {
      show_gen();
    }
    else if (mode == MODE_COL)
    {
      show_col();
    }
  }
}
