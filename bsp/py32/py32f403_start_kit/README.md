# PY32F403 Start Kit BSP

本 BSP 支持 `py32f403_start_kit`，使用 PY32F403V1DT6。

| 项目         | 默认配置                                              |
| ------------ | ----------------------------------------------------- |
| MCU          | PY32F403V1DT6，Cortex-M4F（`SOC_PY32F403xD`）         |
| Flash / SRAM | 384 KiB / 64 KiB                                      |
| 系统时钟     | HSE 24 MHz 直驱 SYSCLK / HCLK 24 MHz，PLL 关闭        |
| 外设总线     | APB1 24 MHz，APB2 12 MHz                              |
| 控制台       | uart2，USART2 PA2 TX / PA3 RX，AF2，115200 8N1        |
| 可选串口     | uart1，USART1 PA9 TX / PA10 RX，AF2                   |
| 板载 LED     | PA1，低电平点亮；默认应用每 500 ms 切换亮灭，周期 1 s |
| 用户按键     | PA0；支持 GPIO 读取，尚不支持 EXTI                    |
| 调试器       | DAPLink，CMSIS-DAP v2，SWD 1 MHz                      |

本板固定使用 `SOC_PY32F403xD`，与 384 KiB Flash / 64 KiB SRAM 的链接脚本和
MDK 设备配置保持一致，不提供 xB/xC 型号切换。SDK 仍支持其它 F403 型号；
移植到其它板卡时，应同时适配芯片宏、链接容量、引脚和烧录配置。

## 快速开始

准备 RT-Thread Env（包含 Python、SCons、Kconfig 和 packages 工具）及对应编译器。

在 Env 终端进入 RT-Thread 源码根目录，打开 BSP 配置菜单：

```shell
cd bsp/py32/py32f403_start_kit
scons --menuconfig
```

PY32F403 系列配置会自动选中 SDK 和 CMSIS-Core，SDK 版本可在以下菜单中查看或调整：

```text
RT-Thread online packages
    peripheral libraries and drivers
        HAL & SDK Drivers
            PUYA HAL & SDK Drivers
                -*- PY32F403 SDK (Device and HAL)
                    Version (latest)
```

保存配置后，下载依赖软件包：

```text
pkgs --update
```

SDK 安装到 `bsp/py32/libraries/py32f403_sdk-latest/`。执行以下命令构建固件：

```powershell
$env:RTT_CC = 'gcc'
$env:RTT_EXEC_PATH = 'C:/toolchains/arm-gnu/bin' # 修改为实际安装路径
scons -j8
```

构建生成 `rtthread.elf` 和 `rtthread.bin`。烧录步骤见下文；UART2 接线为
PA2（MCU TX）接 USB 转串口 RX、PA3（MCU RX）接 TX，并共地，使用 3.3 V 电平和 115200 8N1。


## 开发环境搭建指南

### 工具链

GCC 配置见快速开始。切换为 Clang（已验证 ATfE 22.1）时：

```powershell
$env:RTT_CC = 'llvm-arm'
$env:RTT_EXEC_PATH = 'C:/toolchains/ATfE/bin'
$env:RTT_GNU_ROOT = 'C:/toolchains/arm-gnu'
scons -j8
```

Clang 使用 LLD、ATfE compiler-rt 和 GNU newlib-nano，因此还需要 Arm GNU Toolchain。
`RTT_GNU_ROOT` 指向 GNU 安装根目录；这套组合与其它 LLVM 发行包不一定通用。

MDK 命令行构建（已验证 Arm Compiler 6.19）：

```powershell
$env:RTT_CC = 'keil'
$env:RTT_EXEC_PATH = 'C:/Keil_v5/ARM/ARMCLANG/bin'
scons -j8
```

GCC / Clang 生成 `rtthread.elf`，MDK 生成 `rtthread.axf`，三者均生成 `rtthread.bin`。

### 工程导出

| 命令 | 工具链配置和用途 |
| --- | --- |
| `scons --target=cmake` | GCC，生成 CMake 工程 |
| `scons --target=mdk5` | MDK，生成 Arm Compiler 6 工程 |
| `scons --target=vsc` | GCC，生成 VS Code 配置 |
| `scons --cdb` | 生成 `build/compile_commands.json` |
| `scons --dist` | 生成包含内核、驱动和依赖包的独立 BSP 分发包 |

### VS Code 与 clangd

在 VS Code 中安装 `clangd` 扩展。

1. 构建时添加 `--cdb` 参数来生成 `build/compile_commands.json`：
```shell
scons --cdb -j8
```

2. 使用 VS Code 打开 `py32f403_start_kit` 目录，创建 `.vscode/settings.json`，写入以下配置：
```jsonc
{
  "clangd.arguments": [
    "--compile-commands-dir=build",
    // 可使用 **/ 匹配编译器路径，或填写实际安装路径。
    "--query-driver=**/arm-none-eabi-gcc.exe,**/arm-none-eabi-g++.exe"
  ]
}
```

如果代码跳转未生效，在命令面板（`Ctrl+Shift+P`）中执行 `clangd: Restart language server`。

### Keil MDK

使用 `RTT_CC=keil` 配置后再导出工程（模板为 BSP 根目录的 `template.uvprojx`）：

```powershell
$env:RTT_CC = 'keil'
$env:RTT_EXEC_PATH = 'C:/Keil_v5'
scons --target=mdk5
```

`RTT_EXEC_PATH` 可以指向 Keil 安装根目录，也可以指向 `ARM/ARMCLANG/bin`；
`project.uvprojx` 由 BSP 中的 `template.uvprojx` 模板生成。
Arm Compiler 6 通过厂商 `__main` 与 RT-Thread `$Sub$$main` 接入，不使用 GCC 启动文件。


## 驱动支持情况

驱动支持范围如下：

| 驱动                | 范围                                                  |
| ------------------- | ----------------------------------------------------- |
| 启动、时钟、SysTick | HSE 24 MHz，SYSCLK 24 MHz，系统 tick 1000 Hz          |
| UART1/UART2         | Serial V2，中断接收、轮询或中断发送；无 DMA/硬件流控 |
| GPIO                | 输入、上下拉、推挽、开漏、读写；EXTI 尚不支持         |
| 其他外设            | 尚未接入 BSP 驱动                                    |

厂商 SDK 不随本 BSP 提交，设备头、HAL、system 与启动文件来自 SDK 软件包。

默认 C 工程已通过 GCC、Clang 和 MDK 的命令行构建验证。硬件验证覆盖 GCC 固件的
烧录回读、启动、软件复位、SysTick 和 PA1 输出；Serial V2 串口收发、PA0 按键、
断电冷启动、Clang/MDK 固件运行、C++ 和 MDK IDE 内构建尚未验证。

## 系列与板级适配边界

`SOC_FAMILY_PY32` 标识 PY32 家族；`SOC_SERIES_PY32F403` 在
`libraries/Kconfig` 中选择 Cortex-M4 和 F403 SDK、CMSIS-Core 软件包。
板级配置固定实际芯片，负责时钟、引脚、存储容量和工具链参数。

沿用 STM32、AT32 BSP 的组织方式，板级 `board.h` 选择厂商头文件，
`drivers/drv_common.h` 和 `drv_gpio.h` 经由 `board.h` 使用对应 HAL；
公共驱动构建按 `RT_USING_PIN`、`RT_USING_SERIAL` 选择源文件。
GPIO 端口和时钟按 SDK 提供的外设宏启用，端口表保留字母编号空洞，
例如 F003 的 A/B/F 不会被重新编号为 A/B/C。

系列差异在具体外设中处理：目前 UART 的时钟、寄存器及中断适配仅支持
F403。新增系列需补充 Kconfig、
SDK 包和板级 HAL 配置，验证 GPIO 地址映射及 UART 语义；复杂的系列
配置可按 AT32 的 `config/<series>/` 结构拆分。F403 板级使用的 SysTick 时基、
FPU 编译选项和链接脚本也需要按新芯片调整。

`tools/sdk_dist.py` 接收板级 `rtconfig.py` 提供的 SDK 包名和裁剪目录，
共享分发逻辑不固定某个 PY32 系列。当前完整 BSP 支持范围仍为 F403。
