# G308 / ROBOCON 电控组考核

- 姓名：王永祺
- 学号：2025111454
- 专业：电气工程及其自动化
- 主控：STM32F103C8T6

本仓库保存 G308 电控组考核第一题、第二题和第三题的工程及提交材料。

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
| 开发过程简要说明 | [`docs/`](./Q2-Servo-VOFA/docs/) | 接线、协议、测试和故障排查说明 |
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
