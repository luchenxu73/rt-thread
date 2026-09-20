# PY32F403 Start Kit BSP

本 BSP 支持 `py32f403_start_kit`，使用 PY32F403V1DT6。

| 项目         | 默认配置                                              |
| ------------ | ----------------------------------------------------- |
| MCU          | PY32F403V1DT6，Cortex-M4F                             |
| Flash / SRAM | 384 KiB / 64 KiB                                      |
| 系统时钟     | HSE 24 MHz 直驱 SYSCLK / HCLK 24 MHz，PLL 关闭        |
| 外设总线     | APB1 24 MHz，APB2 12 MHz                              |
| 控制台       | uart2，USART2 PA2 TX / PA3 RX，AF2，115200 8N1        |
| 可选串口     | uart1，USART1 PA9 TX / PA10 RX，AF2                   |
| 板载 LED     | PA1，低电平点亮；默认应用每 500 ms 切换亮灭，周期 1 s |
| 用户按键     | PA0；支持 GPIO 读取，尚不支持 EXTI                    |
| 调试器       | DAPLink，CMSIS-DAP v2，SWD 1 MHz                      |

## 快速开始

在 Env 终端进入 RT-Thread 源码根目录，打开 BSP 配置菜单：

```shell
cd bsp/py32/py32f403_start_kit
scons --menuconfig
```

软件包位于以下菜单：

```text
RT-Thread online packages
    peripheral libraries and drivers
        HAL & SDK Drivers
            [*] PY32F403 SDK (Device and HAL)
                Version (latest)
```

完成配置后更新packages:

```text
pkgs --update
```

SDK 将被安装到 `bsp/py32/libraries/py32f403_sdk-latest/`下。执行以下命令即可完成构建：

```shell
scons -j8
```

## 开发环境搭建指南

本章节提供一些典型的开发环境搭建指南

### Toolchain

当前工程支持以下三种工具链：
- Arm GNU Toolchain
- clang (ATfE)
- armclang (keil)

### 工程导出

当前支持的target包括：

- cmake
- mdk
- vscode

### vscode + clangd 开发环境

**预置条件：**
vscode安装拓展 `clangd`


1. 构建时添加 `--cdb` 参数来生成对应的 `build_commands.json`:
```shell
scons --cdb -j8
```

2. 使用vscode打开py32f403_start_kit目录下，并创建 .vscode/settings.json 文件，写入对应配置：
```json
{
  "clangd.arguments": [
    "--compile-commands-dir=build",
    // 此处填写自己的编译器路径，可以**/来匹配任意路径，也可以使用绝对路径，例如 D:/dev_tools/arm-none-eabi-gcc.exe
    "--query-driver=**/arm-none-eabi-gcc.exe,**/arm-none-eabi-g++.exe"
  ]
}
```

此时工程可以具备正常的代码跳转能力。

> 备注
> 如果代码跳转未生效，在vscode中 `ctrl` + `p`， 并选择 \> clangd: Restart language server

### keil(mdk) 开发环境



## 首次板上验证

1. 烧录与复位，确认 UART2 输出 RT-Thread banner 和 `msh` 提示符。
2. 执行 `help`、`list thread`、`list device`、`free`，确认命令收发、调度及堆状态。
3. 确认 PA1 LED 每 500 ms 切换亮灭；按下与松开 PA0 用户按键，验证 GPIO 输入。

2026-09-22 使用 GCC 15.2.Rel1 固件完成烧录验证，64,196 B 固件逐字节回读一致。
UART2 在 115200 8N1 下输出启动信息，以上四条 msh 命令均正常；读得
`SystemCoreClock` 为 24 MHz，系统 tick 持续递增，PA1 输出约每 500 ms 翻转。
`free` 报告堆总量 59,896 B、已用 5,172 B、可用 54,724 B。
本次验证包含软件复位；PA0 按键、UART1、断电冷启动及 Clang/MDK 固件的实物运行尚未验证。

烧录前先复位并暂停芯片，再执行扇区擦除和编程。本次直接接管原程序运行状态时，
Flash 算法在擦除阶段触发 HardFault；复位暂停并配置 DBGMCU 调试冻结位后烧录成功。
固件包附带的 OpenOCD 0.12.0 本次将容量自动识别为 16 KiB，因此改用上述 pyOCD 流程。

## 驱动支持情况

当前驱动范围如下；本次实物验证覆盖启动、时钟、SysTick、UART2 控制台和 PA1 输出。

| 驱动                | 范围                                                  |
| ------------------- | ----------------------------------------------------- |
| 启动、时钟、SysTick | HSE 24 MHz，SYSCLK 24 MHz，系统 tick 1000 Hz          |
| UART1/UART2         | Serial V1，中断接收、轮询发送；无 DMA/TX IRQ/硬件流控 |
| GPIO                | 输入、上下拉、推挽、开漏、读写；EXTI 尚不支持         |
| 其他外设            | 计划按板卡需求增加                                    |

厂商 SDK 不随本 BSP 提交，设备头、HAL、system 与启动文件来自 SDK 软件包。
