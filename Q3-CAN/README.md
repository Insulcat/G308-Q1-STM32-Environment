# 第三题：CAN 通信

本目录对应《电控组考核题 V2.0》软件部分第三题，包含主从机项目文件、开发文档和实操视频。

## 提交材料

| 新版要求 | 位置 | 内容 |
|---|---|---|
| 项目文件 | [`firmware/master/`](./firmware/master/)、[`firmware/slave/`](./firmware/slave/) | STM32F103C8T6 主机和从机的 STM32CubeMX/CMake 工程 |
| 开发文档 | [`docs/development.md`](./docs/development.md) | 设计、接线、通信流程、配置和验证记录 |
| 实操视频 | [`video/IMG_2700.MP4`](./video/IMG_2700.MP4) | 主机按键、CAN 收发、从机 LED 翻转和 VOFA+ 串口显示演示 |

## 实现结果

- 主机检测 PB0 按键按下，经 CAN 标准帧 ID `0x123` 发送单字节 `0x11`。
- 从机收到 `0x11` 后翻转 PC13 LED，并向主机返回单字节 `0x22`。
- 主机经 USART1/CP2102 向 VOFA+ 输出 `CAN master ready`、`TX: 0x11`、`RX: 0x22`。
- 两个工程均将通用 CAN 初始化、发送、接收和中断回调封装在 `Src/can.c` 与 `Inc/can.h`。

详细配置、接线与复现步骤见[开发文档](./docs/development.md)。
