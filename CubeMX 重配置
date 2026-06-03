# CubeMX 重新生成代码注意事项

当前工程为了满足设计目标，采用了“CubeMX 基础工程 + USER CODE 手写外设初始化 + Keil 工程手动加入用户模块”的方式。这样做的好处是工程能快速覆盖 AD9833、OLED、幅度控制、SCAN、COL 等全部功能；代价是重新用 CubeMX 生成代码后必须检查几个关键点。

## 1. 生成前设置

在 CubeMX 中打开 `DDSDesign-5.28-Latest.ioc` 后，建议确认：

- `System Core -> SYS -> Debug` 为 `Serial Wire`，不要选择 `No Debug`。
- `Project Manager -> Code Generator -> Keep User Code` 保持开启。
- `Project Manager -> Toolchain / IDE` 为 `MDK-ARM V5`。
- 栈大小保持 `0x800`，因为 OLED 缓冲和采样分析会占用一定 RAM。

当前 `.ioc` 已经写入：

```text
PA13.Mode=Serial_Wire
PA13.Signal=SYS_JTMS-SWDIO
PA14.Mode=Serial_Wire
PA14.Signal=SYS_JTCK-SWCLK
ProjectManager.StackSize=0x800
```

## 2. 生成后必须检查的源码

CubeMX 生成后，检查这些内容仍然存在：

| 文件 | 必须保留的内容 |
| --- | --- |
| `Core/Src/main.c` | `#include "app_config.h"`、`#include "signal_app.h"` |
| `Core/Src/main.c` | `ADC_HandleTypeDef hadc1;`、`UART_HandleTypeDef huart1;` |
| `Core/Src/main.c` | `MX_GPIO_Init()`、`MX_ADC1_Init()`、`MX_USART1_UART_Init()`、`SignalApp_Init()` |
| `Core/Src/main.c` | `while` 循环中调用 `SignalApp_Run()` |
| `Core/Src/main.c` | USER CODE 4 中的 `MX_GPIO_Init`、`MX_ADC1_Init`、`MX_USART1_UART_Init` 定义 |
| `Core/Src/stm32f1xx_hal_msp.c` | 使用 `__HAL_AFIO_REMAP_SWJ_NOJTAG()`，不要使用 `__HAL_AFIO_REMAP_SWJ_DISABLE()` |
| `Core/Inc/stm32f1xx_hal_conf.h` | `HAL_ADC_MODULE_ENABLED` 和 `HAL_UART_MODULE_ENABLED` 仍然启用 |

## 3. 生成后必须检查的 Keil 工程

打开 `MDK-ARM/DDSDesign-5.28-Latest.uvprojx`，确认这些源文件仍在工程里：

- `Core/Src/ad9833.c`
- `Core/Src/amplitude.c`
- `Core/Src/ssd1306.c`
- `Core/Src/signal_app.c`
- `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c`
- `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c`
- `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c`

如果 CubeMX 重新生成后删掉了这些文件引用，需要手动加回 Keil 工程。

## 4. 为什么没有完全依赖 CubeMX 配外设

本设计用到的 AD9833、SSD1306、MCP41010 都可以用软件 SPI/I2C 控制，且引脚可灵活调整。为了减少 CubeMX 配置项和外设冲突，固件把这些驱动集中在用户代码中：

- AD9833：软件 SPI，PB12/PB13/PB15/PB14。
- MCP41010：软件 SPI 共享 PB13/PB15，PB8 片选。
- OLED：软件 I2C，PB6/PB7。
- ADC1：手写初始化 PA0。
- USART1：手写初始化 PA9/PA10。

这种方式适合课程设计和快速验证。若后续要做成长期维护项目，可以在 CubeMX 中正式启用 ADC1、USART1、GPIO，并让 CubeMX 生成对应 `MX_ADC1_Init()` 和 `MX_USART1_UART_Init()`，再把 USER CODE 中的手写初始化迁移过去。

## 5. 一键检查建议

重新生成后，可以在 PowerShell 中运行：

```powershell
rg "SWJ_DISABLE|No_Debug|HAL_ADC_MODULE_ENABLED|HAL_UART_MODULE_ENABLED|SignalApp_Init|ad9833.c|amplitude.c|ssd1306.c|signal_app.c" .
```

期望结果：

- 不应出现 `SWJ_DISABLE` 或 `No_Debug`。
- 应出现 `HAL_ADC_MODULE_ENABLED`、`HAL_UART_MODULE_ENABLED`。
- 应出现 `SignalApp_Init`、`SignalApp_Run`。
- Keil 工程中应出现四个用户源文件名。
