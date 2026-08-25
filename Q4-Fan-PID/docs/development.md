# 第四题 FreeRTOS 电机 PID 控制开发文档

## 1. 任务与完成情况

本题使用 STM32F103C8T6、JGA12-N20 带编码器减速电机、TB6612FNG、电位器和 FreeRTOS，实现以下功能：

1. 让直流减速电机转动。
2. 通过 ADC 读取电位器，将采样值映射为目标转速或目标角度。
3. 使用 PID 完成定速和定位控制，并通过 VOFA+ 显示目标值与实际值曲线。
4. 使用 FreeRTOS 分离实时控制和串口遥测任务。

上述功能已经完成实机验证。速度模式能够跟随分段目标并停车；位置模式能够自动定位并停止，最终仍有约 2° 的误差。

## 2. 工程结构

- `firmware/`：完整 STM32CubeMX/CMake 工程，保留 `.ioc`、启动文件、链接脚本、HAL 驱动、FreeRTOS 和应用源码。
- `firmware/Core/Src/fan_control.c`：编码器测速、位置计算、PID 运算、电机方向与 PWM 输出。
- `firmware/Core/Src/freertos.c`：控制任务、遥测任务、模式切换和 JustFloat 数据发送。
- `docs/vofa-speed.png`：速度模式 VOFA+ 全局截图。
- `docs/vofa-position.png`：位置模式 VOFA+ 全局截图。
- `video/speed-control.mp4`：速度控制实操视频。
- `video/position-control.mp4`：位置控制实操视频。

构建目录、IDE 缓存和本机临时文件未纳入提交。源代码关键逻辑处保留了中文注释。

## 3. 硬件与关键配置

| 功能 | 配置 |
|---|---|
| MCU | STM32F103C8T6，系统时钟 72 MHz |
| 电机 | JGA12-N20，6 V，约 300 r/min，减速比 1:50，霍尔编码器 7 PPR |
| 编码器 | TIM2 Encoder Mode：PA0/CH1 接 C1，PA1/CH2 接 C2 |
| 电位器 | PA2 / ADC1_IN2，12 位连续转换，DMA 循环模式 |
| 电机 PWM | PA6 / TIM3_CH1，PSC=0，ARR=3599，约 20 kHz |
| 电机方向与使能 | PB0=AIn1，PB1=AIn2，PB10=STBY |
| 模式输入 | PB11，上拉输入；短接 GND 切换速度/位置模式 |
| 串口 | USART1：PA9 TX、PA10 RX，115200、8N1 |
| 电机驱动 | TB6612FNG A 通道，VM 接 6 V 电机电源，VCC 接 3.3 V 逻辑电源 |

编码器按电机轴 7 PPR、四倍频和 1:50 减速估算，每个输出轴整圈约 `7 × 4 × 50 = 1400` 个计数。程序据此计算转速和角度。

## 4. FreeRTOS 设计

| 对象 | 配置 | 作用 |
|---|---|---|
| `ControlTask` | AboveNormal，384 words | 周期读取 ADC 和编码器，执行 PID 并更新 PWM |
| `TelemetryTask` | Normal，256 words | 通过 USART1 向 VOFA+ 发送状态数据 |
| `DataMutex` | Mutex | 保护任务之间共享的控制与遥测数据 |
| Heap | heap_4，8192 bytes | 支持任务和互斥量的动态分配 |

FreeRTOS Tick 为 1 kHz。控制任务优先级高于遥测任务，串口发送不会阻塞实时控制周期。

## 5. PID 与控制流程

最终使用的参数如下：

| 模式 | Kp | Ki | Kd |
|---|---:|---:|---:|
| 速度 PID | 0.35 | 0.80 | 0.00 |
| 位置 PID | 0.45 | 0.01 | 0.03 |

控制流程：

1. ADC DMA 连续读取电位器；程序把 `0~4095` 映射为速度或角度目标。
2. TIM2 编码器计数计算实际转速和累计角度。
3. PID 根据 `目标值 - 实际值` 计算输出，限制积分与 PWM，避免输出越界。
4. 输出符号决定 AIN1/AIN2 方向，绝对值转换为 TIM3 PWM 占空比。
5. 目标为零或位置进入停止条件后，程序关闭驱动输出，使电机停下。
6. 遥测任务把控制过程发送给 VOFA+，以阶跃目标观察响应、超调和稳态误差。

本次调试保留了可复现的速度阶跃与位置阶跃演示。速度控制能稳定跟随；位置控制存在约 2° 的残余误差，后续可继续小幅调整位置环 `Ki` 和停止阈值。

## 6. VOFA+ 数据定义

VOFA+ 使用 `JustFloat` 引擎，串口参数为 115200、8N1。每帧发送 8 个浮点数：

| 通道 | 含义 |
|---|---|
| I0 | 模式：0=速度，1=位置 |
| I1 | ADC 原始值（0~4095） |
| I2 | 目标转速或目标角度 |
| I3 | 实际转速或实际角度 |
| I4 | PWM 占空比（%） |
| I5 | 实际转速（r/min） |
| I6 | 累计角度（°） |
| I7 | 控制误差 |

绘图时只显示 I2 和 I3，即可直观看到目标曲线与实际曲线。VOFA+ 下方文本区出现乱码，是 JustFloat 二进制数据被当作 UTF-8 文本显示，并非通信故障。

## 7. 实机验证与视频内容

### 速度模式

- I0 为 0。
- 视频中把 I2 从 0 调到约 30，再调到约 60，I3 随目标变化并稳定跟随。
- 目标回到 0 后电机停止。
- 文件：[`../video/speed-control.mp4`](../video/speed-control.mp4)。
- 全局截图：[`vofa-speed.png`](./vofa-speed.png)。

### 位置模式

- I0 为 1。
- 调整电位器后，目标角度发生阶跃变化，电机转动到目标附近并自动停止。
- 曲线可观察到跟随、轻微超调和最终约 2° 的残余误差。
- 文件：[`../video/position-control.mp4`](../video/position-control.mp4)。
- 全局截图：[`vofa-position.png`](./vofa-position.png)。

## 8. 接线与安全说明

编码器线序以本次电机标签为准：黑线为编码器 3.3 V，蓝线为编码器 GND，绿线为 C1，黄线为 C2；红、白两线为电机 M2/M1。电机动力线连接 TB6612FNG 的 AO1/AO2，不能连接 STM32 GPIO。

电机电源与 3.3 V 逻辑电源分开供电，但 STM32、TB6612FNG、编码器和串口模块必须共地。实机使用的 6 V 适配器空载实测约 6.45 V。

每次上电前必须确认：

- 电机、编码器、TB6612FNG 和 STM32 的工作电压匹配；
- VM、VCC、GND、AO1/AO2、AIN1/AIN2、PWMA 和 STBY 接线正确；
- 所有模块共地，电源正负极无反接，裸线无短路；
- 电机已经可靠固定；安装扇叶时还需固定防护罩并保持人员远离旋转区域；
- 改线、插拔模块或安装扇叶前先完全断电。

## 9. 构建说明

工程由 STM32CubeMX 生成 CMake 配置。安装 ARM GNU Toolchain、CMake 和 Ninja 后，可在 `firmware/` 目录执行：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

生成的 ELF、HEX 或 BIN 可用 STM32CubeProgrammer 通过 ST-LINK 烧录。重新用 CubeMX 生成代码时，应保留 `USER CODE BEGIN/END` 区域内的用户代码。
