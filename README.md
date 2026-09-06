# G308 / ROBOCON 电控组考核

- 姓名：王永祺
- 学号：2025111454
- 专业：电气工程及其自动化
- 主控：STM32F103C8T6

本仓库保存 G308 电控组考核第一题至第五题的工程及提交材料。

> 提交前总核验结果、逐题材料对应关系、编译记录和已知说明见
> [`SUBMISSION_CHECKLIST.md`](./SUBMISSION_CHECKLIST.md)。

## 项目结构

### 第一题：STM32 现代开发环境

仓库根目录中的 `Core/`、`Drivers/`、`cmake/`、`F103_LED_Blink.ioc` 等文件属于第一题工程。

主要开发环境：

- Visual Studio Code
- STM32CubeMX
- STM32Cube FW_F1 V1.8.7
- CMake + Ninja
- GNU Arm Embedded GCC
- STM32 HAL

第一题完成了 STM32F103C8T6 工程生成、现代 CMake 构建环境配置及 LED 程序验证。

### 第二题：VOFA+ 上位机与舵机控制

第二题全部材料位于 [`Q2-Servo-VOFA/`](./Q2-Servo-VOFA/)：

| 提交要求 | 仓库位置 | 内容 |
|---|---|---|
| 项目文件 | [`firmware/`](./Q2-Servo-VOFA/firmware/) | STM32CubeMX/CMake 完整工程 |
| 开发文档 | [`docs/`](./Q2-Servo-VOFA/docs/) | 接线、协议、测试和故障排查说明 |
| 实操视频 | [`video/`](./Q2-Servo-VOFA/video/) | VOFA+ 与 SG90 实机演示 |

第二题实现了：

- VOFA+ 通过 CP2102 和 USART1 下发目标角度。
- TIM2_CH1（PA0）输出 50 Hz PWM 控制 SG90 舵机。
- STM32 通过 JustFloat 回传程序控制角度和 PWM 占空比。
- `A45`、`A90`、`A135` 测试均能使舵机到达对应位置并稳定保持。

详细命令、接线和测试结果请查看[第二题说明](./Q2-Servo-VOFA/README.md)。

### 第三题：CAN 通信

第三题全部材料位于 [`Q3-CAN/`](./Q3-CAN/)：

| 提交要求 | 仓库位置 | 内容 |
|---|---|---|
| 项目文件 | [`firmware/`](./Q3-CAN/firmware/) | STM32F103C8T6 主机和从机完整工程 |
| 开发文档 | [`docs/`](./Q3-CAN/docs/) | 设计、配置、接线、通信流程和验证记录 |
| 实操视频 | [`video/`](./Q3-CAN/video/) | CAN 收发、从机 LED 翻转和 VOFA+ 串口输出演示 |

第三题实现了：主机按键发送 `0x11`；从机收到后回复 `0x22` 并翻转 LED；主机通过 USART1/CP2102 在 VOFA+ RawData 中打印收发结果。

详细配置、接线和测试结果请查看[第三题说明](./Q3-CAN/README.md)。

### 第四题：FreeRTOS 电机 PID 控制

第四题全部材料位于 [`Q4-Fan-PID/`](./Q4-Fan-PID/)：

| 提交要求 | 仓库位置 | 内容 |
|---|---|---|
| 项目文件 | [`firmware/`](./Q4-Fan-PID/firmware/) | STM32CubeMX/CMake/HAL/FreeRTOS 完整源码工程 |
| 开发过程简要说明 | [`docs/development.md`](./Q4-Fan-PID/docs/development.md) | 系统设计、PID 参数、接线、数据通道和验证记录 |
| 实操视频 | [`video/`](./Q4-Fan-PID/video/) | 速度控制与位置控制实机演示 |
| VOFA+ 截图 | [`docs/`](./Q4-Fan-PID/docs/) | 速度和位置模式的完整曲线截图 |

第四题实现了 ADC 电位器设定、编码器反馈、TB6612FNG 电机驱动、速度/位置 PID、FreeRTOS 双任务和 VOFA+ JustFloat 实时绘图。

详细配置、接线和测试结果请查看[第四题说明](./Q4-Fan-PID/README.md)。

### 第五题：FreeRTOS 二自由度舵机云台

第五题全部材料位于 [`Q5-Gimbal/`](./Q5-Gimbal/)：

| 提交要求 | 仓库位置 | 内容 |
|---|---|---|
| 程序设计说明 | [`docs/`](./Q5-Gimbal/docs/) | 工程架构、通信协议、接线、问题解决过程和测试结果 |
| AI 对话截图 | [`docs/AI对话截图/`](./Q5-Gimbal/docs/AI对话截图/) | 6 张关键对话截图 |
| 项目文件 | [`firmware/`](./Q5-Gimbal/firmware/) | STM32F103C8T6 主控端和舵机执行端完整工程 |
| 演示视频 | [`video/`](./Q5-Gimbal/video/) | 电位器控制、MPU6050 姿态跟随和实时模式切换演示 |

第五题实现了双电位器控制、MPU6050 姿态跟随、PA2 实时模式切换、HC-05 主从无线通信、双路 SG90 控制和通信超时保护，两端均使用 FreeRTOS。

详细架构、问题与解决过程请查看[第五题说明](./Q5-Gimbal/README.md)。


## 硬件部分

### 硬件第一题：STM32 最小系统板

[图纸与说明](./Hardware/Q1-STM32-Minimum-System/README.md)：包含原理图 PDF/PNG、PCB 六页 PDF 及顶层、底层突出显示的彩色 PNG。此目录为硬件题材料，与根目录的软件第一题区分。
