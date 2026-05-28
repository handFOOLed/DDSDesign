# STM32F103C8T6 + AD9833 信号发生器/幅频特性测试仪/示波器

本工程面向 CubeMX + Keil5，核心器件为 STM32F103C8T6、AD9833、SSD1306 OLED，并预留一个 MCP41010 兼容数字电位器作为幅度控制执行器。固件提供三个模式：

- GEN Mode：信号发生器，1 Hz 到 10 MHz，支持 Sine/Ramp/Square，支持串口输入频率、幅度和波形。
- SCAN Mode：幅频特性测试仪，AD9833 扫频，采集外部电路输出幅度，在 OLED 上自适应比例尺绘制曲线并标注 -3 dB 频率 Fc。
- COL Mode：示波器检测模式，不显示波形，只显示外部信号类型、Vpp、Freq，目标输入范围 10 mVpp 到 3.3 Vpp、1 Hz 到 100 kHz。

## 引脚分配

| Function | STM32F103C8T6 Pin | Notes |
| --- | --- | --- |
| AD9833 FSYNC | PB12 | Software SPI chip select |
| AD9833 SCLK | PB13 | Software SPI clock |
| AD9833 RESET | PB14 | AD9833 reset |
| AD9833 SDATA | PB15 | Software SPI data |
| MCP41010 CS | PB8 | Generator amplitude control, shares PB13/PB15 |
| OLED SCL | PB6 | SSD1306 I2C clock, bit-banged |
| OLED SDA | PB7 | SSD1306 I2C data, bit-banged |
| ADC input | PA0 / ADC1_IN0 | SCAN output or COL external input |
| USART1 TX | PA9 | 115200-8-N-1 command terminal |
| USART1 RX | PA10 | 115200-8-N-1 command terminal |
| MODE key | PA1 | Active low, internal pull-up |
| WAVE key | PA2 | Active low, internal pull-up |
| UP key | PA3 | Active low, internal pull-up |
| DOWN key | PA4 | Active low, internal pull-up |

所有引脚、AD9833 主时钟、幅度校准系数都集中在 `Core/Inc/app_config.h`。

## 串口人机交互

USART1 使用 115200-8-N-1。可以用 USB-TTL 模块连接 PA9/PA10。

```text
m gen|scan|col
f <1..10000000>
w sine|ramp|square
v <1000..3300>
scan
```

按键也能操作：

- MODE：GEN -> SCAN -> COL 循环切换。
- WAVE：GEN 模式下切换 Sine/Ramp/Square。
- UP/DOWN：GEN 模式下按 10 倍步进调节频率。

## GEN 幅度输出设计

AD9833 模块的原始输出幅度通常达不到 1 Vpp 到 3.3 Vpp，也没有片内幅度寄存器。因此本设计把“幅度可调”放在 AD9833 后级模拟电路中实现：

```text
AD9833 OUT
  -> AC coupling / low-pass reconstruction
  -> MCP41010 digital potentiometer gain/attenuation network
  -> rail-to-rail high-speed op amp, biased at 1.65 V
  -> 1 Vpp to 3.3 Vpp output
```

推荐实现方式：

- MCP41010 由 PB8/PB13/PB15 控制，PB13/PB15 与 AD9833 软件 SPI 共享。
- 运放选带宽足够的轨到轨型号；若真要 10 MHz、3.3 Vpp，增益带宽和压摆率要留足余量。
- `Amplitude_SetVpp()` 将 1000 mV 到 3300 mV 映射为 0 到 255 的数字电位器码值。
- 实际电路会有非线性和频响误差，建议烧录后用示波器测 1 Vpp、2 Vpp、3.3 Vpp 三点，再按实测值修正 `Core/Src/amplitude.c` 的映射公式。

## SCAN 幅频测试设计

SCAN 模式使用 AD9833 输出正弦扫频信号，信号进入你提供的外部电路，外部电路输出接 PA0。固件做 64 点对数近似扫频，保存每个频点的幅度，再根据本次扫描的最小/最大幅度自适应拉伸曲线，使幅度变化不大时图像也能显示清楚。

重要限制：

- STM32F103 ADC 不能直接采样 10 MHz 波形。如果扫频上限需要到 10 MHz，外部电路输出应先经过峰值/包络/RMS 检测，PA0 采集检测后的直流幅度。
- 若只测试较低频率，可以直接把被测电路输出经偏置和保护后接 PA0。
- 固件默认 `SCAN_INPUT_USES_ENVELOPE = 1`，即按外部检测器直流输出测幅；每个频点取 32 次 ADC 平均，让 OLED 曲线更稳定。
- 如果改成低频直接采样交流波形，可在 `Core/Inc/app_config.h` 中把 `SCAN_INPUT_USES_ENVELOPE` 改为 `0`。
- `VPP_GAIN_NUM`、`VPP_GAIN_DEN` 用于把 PA0 测得的幅度换算成实际幅度。
- Fc 判定使用参考幅度 0.707 倍作为 -3 dB 阈值；固件会在跨过阈值的相邻两个频点之间线性插值，并在 OLED 上画阈值横线和 Fc 竖线。

## COL 信号检测设计

COL 模式采样 PA0 并估算：

- Vpp：通过 ADC 最大值和最小值差值换算。
- Freq：用 DWT 周期计数记录过均值上升沿之间的真实时间；固件会在 1us、2us、5us、20us、100us、1ms、10ms 多个采样窗口之间择优，以覆盖 1 Hz 到 100 kHz。
- Wave：根据高/低电平停留比例、跳变幅度、单调斜率比例和中心区域分布粗分类为 Square、Ramp 或 Sine。

外部输入必须满足：

- PA0 电压始终在 0 V 到 3.3 V 之间。
- 交流信号先偏置到 1.65 V 附近。
- 对 10 mVpp 小信号，建议加入前置放大或可切换增益，否则 12 位 ADC 分辨率下分类可靠性会下降。
- `COL_MIN_SPAN_ADC` 当前为 12 个 ADC 码，约等于 10 mV，用来过滤噪声造成的误判。

## Keil5 工程

Keil 工程 `MDK-ARM/DDSDesign-5.28-Latest.uvprojx` 已加入：

- `Core/Src/ad9833.c`
- `Core/Src/amplitude.c`
- `Core/Src/ssd1306.c`
- `Core/Src/signal_app.c`
- HAL ADC/UART 驱动源文件

打开 `.uvprojx` 后可直接在 Keil5 中构建。当前电脑环境未检测到 Keil/ARM 编译器，因此本仓库内只完成了工程文件和源码一致性检查；最终仍需在你的 Keil5 环境里 Build 和烧录验证。
