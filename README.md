# STM32F103C8T6 现代开发环境工程

G308电控组考核第一题：现代开发环境配置。

## 开发环境

- MCU：STM32F103C8T6
- 开发工具：Visual Studio Code
- 配置工具：STM32CubeMX
- 固件库：STM32Cube FW_F1 V1.8.7
- 构建系统：CMake + Ninja
- 编译器：GNU Arm Embedded GCC
- 代码框架：STM32 HAL

## 工程说明

本工程使用STM32CubeMX完成引脚和时钟配置，并生成CMake工程，随后在Visual Studio Code中完成配置和编译。

当前工程已通过Debug构建，成功生成可执行文件。