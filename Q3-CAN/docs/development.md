# 第三题 CAN 通信开发文档

## 1. 任务与完成情况

使用两块 STM32F103C8T6 分别作为 CAN 主机和从机。主机按键按下时发送 `0x11`；从机收到后翻转 LED 并回复 `0x22`；主机收到回复后通过串口助手打印。以上功能已完成实机验证。

## 2. 工程结构

- `firmware/master/`：主机工程，包含按键检测、CAN 收发及 USART1 文本输出。
- `firmware/slave/`：从机工程，包含 CAN 收发及 LED 翻转。
- 两个工程均保留 `.ioc`、`CMakeLists.txt`、启动文件、链接脚本、HAL 驱动和应用源码。
- `Inc/can.h` 声明 `CAN_App_Init()`、`CAN_SendByte()`、`CAN_GetReceivedByte()`。
- `Src/can.c` 实现过滤器配置、CAN 启动、单字节标准帧发送、接收缓存及 FIFO0 接收中断回调。

构建目录、IDE 缓存和本机临时文件未纳入提交。

## 3. 关键配置

| 项目 | 主机 | 从机 |
|---|---|---|
| MCU | STM32F103C8T6 | STM32F103C8T6 |
| 系统时钟 | 72 MHz | 72 MHz |
| CAN 引脚 | PA11 RX、PA12 TX | PA11 RX、PA12 TX |
| CAN 波特率 | 500 kbit/s | 500 kbit/s |
| CAN 位时序 | Prescaler 4、BS1 13TQ、BS2 4TQ、SJW 1TQ | 同左 |
| 标准帧 ID | `0x123` | `0x123` |
| 应用数据 | 发送 `0x11`，接收 `0x22` | 接收 `0x11`，发送 `0x22` |
| 本地外设 | PB0 低电平按键；USART1 TX PA9 | PC13 LED |

主机 USART1 使用 `115200 8N1`。VOFA+ 选择 `RawData` 显示文本；`JustFloat` 是二进制浮点协议，不适用于本题的文本输出。

## 4. 接线

每块 STM32 的 PA11/PA12 不能直接连接 CANH/CANL，必须先连接 CAN 收发器（本次实机使用对应 CAN 模块）。两侧 CANH 对 CANH、CANL 对 CANL，并连接公共 GND。总线两端按模块情况配置 120 Ω 终端电阻。

主机串口：PA9（USART1_TX）接 CP2102 RX，GND 接 GND；本题只需单向打印。主机按键接 PB0，低电平有效。从机使用 PC13 LED。

上电前必须核对：两块板和收发器的额定电压、VCC/GND 接线、CANH/CANL 极性、串口 TX→RX、所有设备共地，以及供电正负极。确认无误后再上电。

## 5. 程序流程

1. 两机初始化时钟、GPIO 和 CAN，配置接收过滤器，启动 CAN 并打开 FIFO0 消息挂起中断。
2. 主机初始化 USART1，打印 `CAN master ready`。
3. 主机检测 PB0 按下，延时 20 ms 消抖；一次按下只发送一次 `0x11`，并打印 `TX: 0x11`。
4. 接收中断只读取并缓存合法的标准数据帧，主循环处理缓存，避免在中断中执行耗时操作。
5. 从机收到 `0x11` 后翻转 PC13 LED，并发送 `0x22`。
6. 主机收到 `0x22` 后打印 `RX: 0x22`。

## 6. 实机验证

- 两块 STM32F103C8T6 主从机 CAN 通信成功。
- 每按一次主机按键，从机 LED 翻转一次。
- VOFA+（RawData，115200）显示：

```text
CAN master ready
TX: 0x11
RX: 0x22
```

- CP2102 回环测试正常，主机 PA9 串口输出正常。
- 演示文件：`video/IMG_2700.MP4`，约 37 秒、50.1 MB。

## 7. 构建说明

工程由 STM32CubeMX 生成 CMake 配置。已安装 ARM GNU Toolchain、CMake 和 Ninja 时，可分别进入主机或从机目录执行：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

也可用 STM32CubeMX 打开各自 `.ioc` 文件检查引脚和外设配置。重新生成代码前应先保留 `USER CODE BEGIN/END` 区域内的用户代码。

## 8. 说明

实机验证完成后已经断电。若再次搭建，务必先完成第 4 节的电压、接线、共地和极性检查，再连接电源。
