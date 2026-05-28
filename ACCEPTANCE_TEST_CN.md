# 验收与调试步骤

## 1. Keil5 构建前检查

打开 `MDK-ARM/DDSDesign-5.28-Latest.uvprojx`，确认工程包含这些用户源文件：

- `Core/Src/ad9833.c`
- `Core/Src/amplitude.c`
- `Core/Src/ssd1306.c`
- `Core/Src/signal_app.c`

确认 `Options for Target -> C/C++` 中启用 C99。当前工程文件里 `uC99` 已经为 `1`。

## 2. 基础接线检查

供电前检查：

- STM32、AD9833、OLED、MCP41010、外部模拟电路必须共地。
- PA0 输入必须限制在 0 V 到 3.3 V。
- COL 输入若是交流信号，必须偏置到 1.65 V 附近。
- 若使用 ST-Link 调试，SWDIO/SWCLK 不要被外设占用；固件只关闭 JTAG，保留 SWD。

## 3. GEN Mode 验收

串口连接 USART1，115200-8-N-1。

步骤：

1. 复位后 OLED 应显示 `GEN Mode`。
2. 串口输入 `w sine`，GEN OUT 应为正弦。
3. 串口输入 `w ramp`，GEN OUT 应为三角/斜坡。
4. 串口输入 `w square`，GEN OUT 应为方波。
5. 串口输入 `f 1`、`f 1000`、`f 1000000`、`f 10000000`，用频率计或示波器检查频率。
6. 串口输入 `v 1000`、`v 2000`、`v 3300`，检查 MCP41010 后级输出幅度是否随命令变化。

判定：

- OLED 显示的波形、频率、Vpp 与命令一致。
- 频率范围可覆盖 1 Hz 到 10 MHz。
- 幅度是否准确取决于外部增益级标定；若偏差大，调整 `Core/Src/amplitude.c` 映射。

## 4. SCAN Mode 验收

推荐接线：

```text
GEN OUT -> 被测外部电路 -> 峰值/包络/RMS检测 -> PA0
```

步骤：

1. 串口输入 `m scan` 或按 MODE 进入 SCAN。
2. OLED 显示 `SCAN running...`。
3. 扫描完成后 OLED 应显示幅频曲线。
4. 顶部显示本次动态比例尺，例如 `SCAN 820-3030mV`。
5. OLED 上应出现 -3dB 阈值横线。
6. 若幅度跨过 -3dB 阈值，应出现 Fc 竖线和 `Fc=xxxxHz`。

判定：

- 幅度变化较小时，曲线仍能被动态拉伸到可读高度。
- Fc 是跨阈值相邻扫频点的线性插值结果，不只是粗略频点。
- 若没有跨过 -3dB 阈值，`Fc=0Hz` 属于正常结果。

## 5. COL Mode 验收

推荐接线：

```text
外部信号 -> 保护/偏置/可选放大 -> PA0
```

步骤：

1. 串口输入 `m col` 或按 MODE 进入 COL。
2. 输入 1 kHz、1 Vpp 正弦波，OLED 应显示 `Wave:Sine`，并显示 Vpp/Freq。
3. 输入 1 kHz、1 Vpp 方波，OLED 应显示 `Wave:Square`。
4. 输入 1 kHz、1 Vpp 三角/斜坡波，OLED 应显示 `Wave:Ramp`。
5. 依次测试 1 Hz、100 Hz、10 kHz、100 kHz，确认频率显示随输入变化。
6. 输入小于约 10 mVpp 或断开输入时，OLED 可能显示 `Wave:Weak`，表示信号低于可靠分类阈值。

判定：

- COL 不显示波形，只显示波形名称、Vpp、Freq，符合目标要求。
- 小信号 10 mVpp 附近建议打开前置放大，否则分类稳定性会受噪声影响。

## 6. 常见问题

- OLED 无显示：检查 PB6/PB7、OLED 地址是否为 0x3C、上拉电阻是否存在。
- AD9833 无输出：检查 PB12/PB13/PB15/PB14、MCLK 是否为 25 MHz、模块供电是否正确。
- 串口无回显：检查 PA9/PA10 是否交叉连接到 USB-TTL，波特率是否 115200。
- 无法调试：确认 `stm32f1xx_hal_msp.c` 中使用 `__HAL_AFIO_REMAP_SWJ_NOJTAG()`，保留 SWD。
- SCAN 高频曲线异常：10 MHz 扫频必须使用外部检测器输出直流幅度，不能直接把 10 MHz 交流波形送 PA0。
