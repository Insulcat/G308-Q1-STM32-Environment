# 第四题：FreeRTOS 电机 PID 控制

本目录对应《电控组考核题 V2.0》软件部分第四题，包含 STM32 工程、开发说明、VOFA+ 全局截图和实操视频。

## 提交材料

| 新版要求 | 位置 | 内容 |
|---|---|---|
| 项目文件 | [`firmware/`](./firmware/) | STM32F103C8T6 的 STM32CubeMX/CMake/HAL/FreeRTOS 完整源码工程 |
| 开发过程简要说明 | [`docs/development.md`](./docs/development.md) | 系统设计、外设配置、PID 参数、接线、验证和安全说明 |
| 实操视频 | [`video/speed-control.mp4`](./video/speed-control.mp4)、[`video/position-control.mp4`](./video/position-control.mp4) | 电位器设定速度与角度、PID 跟随及 VOFA+ 曲线演示 |
| VOFA+ 截图 | [`docs/vofa-speed.png`](./docs/vofa-speed.png)、[`docs/vofa-position.png`](./docs/vofa-position.png) | 速度模式和位置模式的完整全局截图 |

## 实现结果

- ADC1_IN2（PA2）读取可调电位器，ADC 值覆盖约 `0~4095`。
- FreeRTOS 下分别运行控制任务和遥测任务，并用互斥量保护共享数据。
- TIM2 编码器模式读取电机霍尔编码器，TIM3_CH1 输出 20 kHz PWM 驱动 TB6612FNG。
- 速度模式使用 PID 完成转速跟随，实测目标值从 0 调到 30、60 后，实际值能够跟随并在目标回到 0 时停车。
- 位置模式使用 PID 完成角度定位，电机到达目标附近后自动停止；当前实机仍有约 2° 的稳态误差。
- USART1 以 JustFloat 格式向 VOFA+ 连续发送目标值、实际值、PWM、转速、位置和误差。

详细配置、接线、数据通道和复现步骤见[开发文档](./docs/development.md)。
