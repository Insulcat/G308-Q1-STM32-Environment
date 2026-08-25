#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 发送给 VOFA+ 的控制状态。
 *
 * mode 为 0 时是调速模式，单位为 rpm；mode 为 1 时是定位模式，单位为度。
 */
typedef struct
{
  float mode;
  float adc_raw;
  float target;
  float actual;
  float pwm_percent;
  float speed_rpm;
  float position_deg;
  float error;
} FanTelemetry_t;

/** 初始化 ADC、编码器和 PWM；全部成功后才会使能电机驱动。 */
bool FanControl_Init(void);

/** 每 10 ms 调用一次，完成采样、测速、模式切换和 PID 运算。 */
void FanControl_Update(FanTelemetry_t *telemetry);

/** 立即关闭 PWM、方向信号和 TB6612 的 STBY。 */
void FanControl_ForceStop(void);

/** 按 VOFA+ JustFloat 格式发送一帧数据。 */
void FanControl_SendTelemetry(const FanTelemetry_t *telemetry);

#ifdef __cplusplus
}
#endif

#endif /* FAN_CONTROL_H */
