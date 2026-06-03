# 基于 STM32F103C8T6 与 AD9833 的多功能信号仪设计报告

## 1. 设计目标

本设计实现一个三合一小型信号仪：

1. GEN Mode：信号发生器，输出 1 Hz 到 10 MHz 的 Sine、Ramp、Square，目标幅度 1 Vpp 到 3.3 Vpp，支持串口和按键人机交互。
2. SCAN Mode：幅频特性测试仪，由 AD9833 扫频，外部被测电路输出经检测后送入 STM32 ADC，在 OLED 上显示幅频特性曲线，动态调整比例尺，并计算/标注 -3 dB 频率 Fc。
3. COL Mode：示波器检测模式，不显示波形，只检测外部信号的波形类型、Vpp、Freq，目标输入 10 mVpp 到 3.3 Vpp、1 Hz 到 100 kHz。

工程使用 CubeMX 生成基础框架，使用 Keil5/MDK-ARM V5 工程构建。

## 2. 系统总体方案

```text
                       +----------------------+
按键 / 串口 ----------> | STM32F103C8T6       |
OLED <---------------- |                      |
                       | 软件SPI PB12/13/15  | --> AD9833 DDS
                       | 软件SPI PB8/13/15   | --> MCP41010 幅度控制
                       | ADC1_IN0 PA0        | <-- SCAN/COL 输入
                       +----------------------+
                                  |
                                  v
                     幅度控制 / 运放 / 外部检测电路
```

软件分层：

- `Core/Src/ad9833.c`：AD9833 频率字、波形寄存器控制。
- `Core/Src/amplitude.c`：MCP41010 数字电位器幅度控制。
- `Core/Src/ssd1306.c`：SSD1306 OLED 显示、文字和曲线绘制。
- `Core/Src/signal_app.c`：三模式状态机、串口命令、按键、SCAN 算法、COL 测量分类。
- `Core/Inc/app_config.h`：引脚、频率、幅度、校准参数集中配置。

## 3. 硬件设计

### 3.1 AD9833 信号源

AD9833 通过软件 SPI 控制：

- FSYNC：PB12
- SCLK：PB13
- SDATA：PB15
- RESET：PB14

固件默认 `AD9833_MCLK_HZ = 25000000`，适配常见 25 MHz AD9833 模块。频率字按公式计算：

```text
FreqReg = fout * 2^28 / MCLK
```

Sine、Ramp、Square 通过 AD9833 控制寄存器中的 `MODE`、`OPBITEN`、`DIV2` 等位实现。

### 3.2 幅度控制与 1~3.3 Vpp 输出

AD9833 原始输出幅度较小，不能直接保证 1~3.3 Vpp。因此本设计采用外部幅度控制和运放级：

```text
AD9833 OUT -> 滤波/耦合 -> MCP41010 可调增益/衰减 -> 高速运放 -> GEN OUT
```

MCP41010 连接：

- CS：PB8
- SCK：PB13
- SI：PB15

`Amplitude_SetVpp()` 将 1000~3300 mV 映射为 0~255 数字电位器码值。实际硬件中，运放增益、数字电位器连接方式和频率响应会引入误差，应按示波器实测结果修正映射。

### 3.3 SCAN 输入前端

STM32F103 ADC 不能直接采样 10 MHz 波形。因此如果 SCAN 需要扫到 10 MHz，外部电路输出必须先经过：

```text
被测电路输出 -> 峰值/包络/RMS 检测 -> RC 平滑 -> 0~3.3 V 限幅 -> PA0
```

固件默认 `SCAN_INPUT_USES_ENVELOPE = 1`，即 PA0 接收检测器直流输出。每个频点取 `SCAN_ENVELOPE_SAMPLES = 32` 次 ADC 平均值，提升曲线稳定性。

### 3.4 COL 输入前端

COL 模式要求识别 10 mVpp 到 3.3 Vpp 信号。输入前端建议：

```text
外部信号 -> 保护电阻 -> 可切换增益/衰减 -> 1.65 V 偏置 -> 钳位保护 -> PA0
```

PA0 必须始终保持在 0~3.3 V。10 mVpp 小信号建议前置放大，否则受 12 位 ADC 量化和噪声影响，分类稳定性会下降。

## 4. 软件设计

### 4.1 GEN Mode

GEN Mode 由 `apply_generator()` 统一应用输出参数：

- `AD9833_SetOutput(gen_freq_hz, gen_wave)` 设置频率和波形。
- `Amplitude_SetVpp(gen_vpp_mv)` 设置目标幅度控制码。

串口命令：

```text
f <1..10000000>
w sine|ramp|square
v <1000..3300>
m gen
```

按键：

- MODE：切换模式。
- WAVE：切换 Sine/Ramp/Square。
- UP/DOWN：按 10 倍步进调节频率。

OLED 显示当前模式、波形、频率、目标 Vpp。

### 4.2 SCAN Mode

SCAN Mode 流程：

1. 使用 64 个近似对数频点，从 10 Hz 到 10 MHz 扫描。
2. 每个频点调用 AD9833 输出正弦波。
3. 等待外部检测器稳定。
4. 读取 PA0 幅度并保存到 `scan_vpp[]`。
5. 以首个频点幅度为参考，计算 -3 dB 阈值 `0.707 * ref_vpp`。
6. 找到曲线跨过阈值的相邻频点，线性插值得到 Fc。
7. OLED 按本次扫描的最小/最大幅度动态拉伸 Y 轴，绘制曲线、阈值横线和 Fc 竖线。

关键实现：

- `scan_measure_amplitude_mv()`：测量每个频点的幅度。
- `draw_scan_curve()`：动态缩放和 OLED 绘图。
- `run_scan()`：扫频控制、Fc 计算和插值。

### 4.3 COL Mode

COL Mode 不显示波形，只显示：

- `Wave:Sine/Ramp/Square/Weak`
- `Vpp:xxxmV`
- `Freq:xxxHz`

测量算法：

1. 多采样窗口自动尝试：1us、2us、5us、20us、100us、1ms、10ms。
2. 选取上升沿数量合适的采样窗口，兼顾 100 kHz 高频和 1 Hz 低频。
3. 用 DWT 周期计数记录相邻上升沿间隔，计算频率。
4. 用 ADC 最大/最小值差计算 Vpp。
5. 根据高低电平停留比例、大跳变数量、单调斜率比例和中心区域分布判断 Square、Ramp、Sine。
6. 当幅度跨度低于 `COL_MIN_SPAN_ADC = 12` 时显示 `Weak`，避免噪声误判。

## 5. CubeMX 与 Keil5 工程配置

`.ioc` 已设置：

```text
PA13.Mode=Serial_Wire
PA13.Signal=SYS_JTMS-SWDIO
PA14.Mode=Serial_Wire
PA14.Signal=SYS_JTCK-SWCLK
ProjectManager.StackSize=0x800
```

`stm32f1xx_hal_msp.c` 使用：

```c
__HAL_AFIO_REMAP_SWJ_NOJTAG();
```

这样只关闭 JTAG，保留 SWD，便于 Keil/ST-Link 下载调试。

Keil 工程已加入：

- `ad9833.c`
- `amplitude.c`
- `ssd1306.c`
- `signal_app.c`
- HAL ADC/UART 驱动文件

## 6. 需求对应关系

| 需求 | 实现证据 |
| --- | --- |
| GEN 输出 Sine/Ramp/Square | `AD9833_SetOutput()` 和 `AD9833_Waveform` |
| GEN 频率 1 Hz~10 MHz | 串口 `f` 命令限幅，AD9833 频率字计算 |
| GEN 幅度 1~3.3 Vpp | `Amplitude_SetVpp()` + MCP41010 + 外部运放幅度级 |
| 人机交互 | USART1 命令、MODE/WAVE/UP/DOWN 按键、OLED 显示 |
| SCAN 幅频曲线 | `run_scan()` + `draw_scan_curve()` |
| SCAN 动态比例尺 | 按 `min_vpp/max_vpp` 拉伸曲线 |
| SCAN -3 dB Fc | 0.707 阈值 + 相邻频点线性插值 + OLED 标注 |
| COL 显示波形名称 | `analyze_capture()` 分类 Sine/Ramp/Square/Weak |
| COL 显示 Vpp/Freq | ADC min/max 换算 Vpp，DWT 上升沿周期算频率 |

## 7. 验收方法

详见：

- `ACCEPTANCE_TEST_CN.md`
- `BOM_AND_INTERFACE_CN.md`
- `HARDWARE_DESIGN_CN.md`
- `CUBEMX_REGENERATION_CN.md`

当前环境未检测到 Keil `UV4` 或 ARM 编译器，因此本仓库内已完成源码、工程引用、关键配置的一致性检查；最终编译、下载、示波器标定需要在 Keil5 与实际硬件环境中完成。
