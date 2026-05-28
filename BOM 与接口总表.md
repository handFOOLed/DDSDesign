# BOM 与接口总表

## 1. 核心器件

| 模块 | 推荐器件 | 用途 |
| --- | --- | --- |
| MCU | STM32F103C8T6 最小系统板 | 主控、ADC、OLED、串口、按键 |
| DDS | AD9833 模块，25 MHz MCLK | 产生 Sine/Ramp/Square |
| 显示 | SSD1306 OLED 128x64 I2C | 模式、参数、幅频曲线显示 |
| 幅度控制 | MCP41010 或兼容 SPI 数字电位器 | GEN 输出 1~3.3 Vpp 幅度控制 |
| 运放 | 高速轨到轨运放 | AD9833 后级放大/偏置/缓冲 |
| 检测器 | 峰值检波、包络检测或 RMS-to-DC | SCAN 高频幅度转直流 |
| 串口 | USB-TTL 模块 | 115200 串口人机交互 |
| 调试 | ST-Link | Keil5 下载和调试 |

## 2. MCU 引脚

| STM32 引脚 | 连接 | 固件宏 |
| --- | --- | --- |
| PB12 | AD9833 FSYNC | `AD9833_FSYNC_Pin` |
| PB13 | AD9833 SCLK / MCP41010 SCK | `AD9833_SCLK_Pin` |
| PB15 | AD9833 SDATA / MCP41010 SI | `AD9833_SDATA_Pin` |
| PB14 | AD9833 RESET | `AD9833_RESET_Pin` |
| PB8 | MCP41010 CS | `AMP_CS_Pin` |
| PB6 | OLED SCL | `OLED_SCL_Pin` |
| PB7 | OLED SDA | `OLED_SDA_Pin` |
| PA0 | ADC1_IN0, SCAN/COL 输入 | `ADC_INPUT_Pin` |
| PA1 | MODE 按键，低有效 | `KEY_MODE_Pin` |
| PA2 | WAVE 按键，低有效 | `KEY_WAVE_Pin` |
| PA3 | UP 按键，低有效 | `KEY_UP_Pin` |
| PA4 | DOWN 按键，低有效 | `KEY_DOWN_Pin` |
| PA9 | USART1 TX | 固定 USART1 |
| PA10 | USART1 RX | 固定 USART1 |
| PA13 | SWDIO | ST-Link 调试 |
| PA14 | SWCLK | ST-Link 调试 |

## 3. 串口命令

| 命令 | 说明 | 示例 |
| --- | --- | --- |
| `m gen` | 进入信号发生器模式 | `m gen` |
| `m scan` | 进入幅频扫描模式并开始扫描 | `m scan` |
| `m col` | 进入信号检测模式 | `m col` |
| `f <Hz>` | 设置 GEN 频率，限制 1~10000000 | `f 1000` |
| `w sine` | 设置正弦波 | `w sine` |
| `w ramp` | 设置斜坡/三角波 | `w ramp` |
| `w square` | 设置方波 | `w square` |
| `v <mV>` | 设置 GEN 目标 Vpp，限制 1000~3300 | `v 2000` |
| `scan` | 立即执行一次 SCAN | `scan` |

## 4. 可调参数

这些宏集中在 `Core/Inc/app_config.h`：

| 宏 | 默认值 | 用途 |
| --- | --- | --- |
| `AD9833_MCLK_HZ` | 25000000 | AD9833 模块主时钟 |
| `GEN_VPP_MIN_MV` | 1000 | GEN 最小目标幅度 |
| `GEN_VPP_MAX_MV` | 3300 | GEN 最大目标幅度 |
| `SCAN_INPUT_USES_ENVELOPE` | 1 | SCAN 是否使用外部检测器直流输出 |
| `SCAN_ENVELOPE_SAMPLES` | 32 | SCAN 每个频点平均采样次数 |
| `VPP_GAIN_NUM` | 1 | 幅度换算分子 |
| `VPP_GAIN_DEN` | 1 | 幅度换算分母 |

`COL_MIN_SPAN_ADC` 在 `Core/Src/signal_app.c`，默认 12 个 ADC 码，约对应 10 mVpp。

## 5. 模拟前端建议

GEN 输出：

- AD9833 输出后建议低通滤波，减少 DDS 镜像。
- 用 MCP41010 调节运放增益或衰减比例。
- 输出级需要能覆盖 1~3.3 Vpp。
- 10 MHz 下若要求 3.3 Vpp，运放压摆率和带宽必须足够。

SCAN 输入：

- 10 MHz 扫频建议外接峰值/包络/RMS 检测。
- PA0 采集检测器直流输出。
- 检测器输出用 `VPP_GAIN_NUM/DEN` 标定成实际幅度。

COL 输入：

- 输入必须偏置在 ADC 可采范围内。
- 大信号需要衰减，小信号需要可选放大。
- 入口加限流电阻和钳位保护，避免损坏 PA0。
