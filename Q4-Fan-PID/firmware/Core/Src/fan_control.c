#include "fan_control.h"

#include "adc.h"
#include "main.h"
#include "tim.h"
#include "usart.h"

/* 任务固定每 10 ms 调用一次控制函数。 */
#define CONTROL_PERIOD_S             (0.010f)

/* 电机铭牌为 7 PPR、减速比 1:50，四倍频后暂按 1400 计数/输出轴圈计算。 */
#define ENCODER_COUNTS_PER_REV       (1400.0f)

/* 首次低速测试若“正转时测速为负”，只需把这里改成 -1.0f。 */
#define ENCODER_DIRECTION            (1.0f)

#define ADC_FULL_SCALE               (4095.0f)
#define SPEED_TARGET_MAX_RPM         (250.0f)
#define POSITION_TARGET_RANGE_DEG    (180.0f)

/* 6 V 适配器空载实测 6.45 V，因此调速模式先把最大占空比限制为 85%。 */
#define SPEED_PWM_LIMIT_PERCENT      (85.0f)

/* 定位时可能频繁换向，先用更低的限幅保护电机、驱动板和扇叶。 */
#define POSITION_PWM_LIMIT_PERCENT   (60.0f)
#define PWM_SLEW_PER_STEP_PERCENT    (2.0f)
#define PWM_ZERO_THRESHOLD_PERCENT   (0.5f)

#define BUTTON_DEBOUNCE_TICKS        (3U)
#define POSITION_DEADBAND_DEG        (2.0f)
#define POSITION_STOP_SPEED_RPM      (8.0f)

typedef enum
{
  FAN_MODE_SPEED = 0,
  FAN_MODE_POSITION = 1
} FanMode_t;

typedef struct
{
  float kp;
  float ki;
  float kd;
  float integral;
  float previous_error;
  float derivative;
  bool has_previous;
} PidController_t;

typedef struct
{
  float values[8];
  uint8_t tail[4];
} JustFloatPacket_t;

_Static_assert(sizeof(JustFloatPacket_t) == 36U, "Unexpected JustFloat packet size");

/* ADC DMA 只搬运一个半字，因此内存地址不递增。 */
static volatile uint16_t g_adc_raw = 0U;

static FanMode_t g_mode = FAN_MODE_SPEED;
static PidController_t g_speed_pid = {0.35f, 0.80f, 0.00f, 0.0f, 0.0f, 0.0f, false};
static PidController_t g_position_pid = {0.45f, 0.01f, 0.03f, 0.0f, 0.0f, 0.0f, false};

static uint16_t g_previous_encoder_count = 0U;
static int32_t g_position_counts = 0;
static int32_t g_position_zero_counts = 0;
static float g_filtered_speed_rpm = 0.0f;
static float g_filtered_adc = 0.0f;
static float g_applied_pwm_percent = 0.0f;

static uint8_t g_button_ticks = 0U;
static bool g_button_latched = false;

static float ClampFloat(float value, float minimum, float maximum)
{
  if (value < minimum)
  {
    return minimum;
  }
  if (value > maximum)
  {
    return maximum;
  }
  return value;
}

static float AbsFloat(float value)
{
  return (value < 0.0f) ? -value : value;
}

static void PidReset(PidController_t *pid)
{
  pid->integral = 0.0f;
  pid->previous_error = 0.0f;
  pid->derivative = 0.0f;
  pid->has_previous = false;
}

static float PidUpdate(PidController_t *pid, float error, float output_min, float output_max)
{
  float raw_derivative = 0.0f;
  float candidate_integral;
  float output;

  if (pid->has_previous)
  {
    raw_derivative = (error - pid->previous_error) / CONTROL_PERIOD_S;
  }

  /* 一阶低通抑制编码器量化噪声，避免微分项放大毛刺。 */
  pid->derivative += 0.20f * (raw_derivative - pid->derivative);
  candidate_integral = ClampFloat(pid->integral + error * CONTROL_PERIOD_S,
                                  -200.0f,
                                  200.0f);

  output = pid->kp * error
         + pid->ki * candidate_integral
         + pid->kd * pid->derivative;

  /* 输出饱和且误差还在同方向增大时，暂停积分，防止积分饱和。 */
  if (!((output > output_max && error > 0.0f)
      || (output < output_min && error < 0.0f)))
  {
    pid->integral = candidate_integral;
  }

  output = pid->kp * error
         + pid->ki * pid->integral
         + pid->kd * pid->derivative;

  pid->previous_error = error;
  pid->has_previous = true;
  return ClampFloat(output, output_min, output_max);
}

static float RampPwm(float requested_percent)
{
  float target = requested_percent;
  float delta;

  /* 正反转切换必须先降到零，不能在较大占空比下直接反接电机。 */
  if ((target > 0.0f && g_applied_pwm_percent < 0.0f)
      || (target < 0.0f && g_applied_pwm_percent > 0.0f))
  {
    target = 0.0f;
  }

  delta = target - g_applied_pwm_percent;
  delta = ClampFloat(delta,
                     -PWM_SLEW_PER_STEP_PERCENT,
                     PWM_SLEW_PER_STEP_PERCENT);
  g_applied_pwm_percent += delta;

  if (AbsFloat(g_applied_pwm_percent) < PWM_ZERO_THRESHOLD_PERCENT)
  {
    g_applied_pwm_percent = 0.0f;
  }
  return g_applied_pwm_percent;
}

static void MotorApplyPwm(float signed_percent)
{
  uint32_t compare;
  float magnitude = AbsFloat(signed_percent);

  /* 改变方向引脚之前先关 PWM，减小换向瞬间的冲击电流。 */
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);

  if (magnitude < PWM_ZERO_THRESHOLD_PERCENT)
  {
    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_RESET);
    return;
  }

  if (signed_percent > 0.0f)
  {
    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_RESET);
  }
  else
  {
    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_SET);
  }

  compare = (uint32_t)(magnitude * (float)__HAL_TIM_GET_AUTORELOAD(&htim3) / 100.0f);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, compare);
}

static bool ModeButtonPressed(void)
{
  bool pressed = (HAL_GPIO_ReadPin(MODE_BUTTON_GPIO_Port, MODE_BUTTON_Pin) == GPIO_PIN_RESET);

  if (pressed)
  {
    if (g_button_ticks < BUTTON_DEBOUNCE_TICKS)
    {
      g_button_ticks++;
    }
    if (g_button_ticks >= BUTTON_DEBOUNCE_TICKS && !g_button_latched)
    {
      g_button_latched = true;
      return true;
    }
  }
  else
  {
    g_button_ticks = 0U;
    g_button_latched = false;
  }
  return false;
}

bool FanControl_Init(void)
{
  FanControl_ForceStop();

  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
  {
    return false;
  }
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&g_adc_raw, 1U) != HAL_OK)
  {
    return false;
  }
  if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) != HAL_OK)
  {
    return false;
  }
  if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
  {
    return false;
  }

  g_previous_encoder_count = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
  g_filtered_adc = (float)g_adc_raw;

  /* 所有外设启动成功后才拉高 STBY，避免复位过程中电机误动作。 */
  HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port, MOTOR_STBY_Pin, GPIO_PIN_SET);
  return true;
}

void FanControl_Update(FanTelemetry_t *telemetry)
{
  uint16_t encoder_now = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
  int16_t encoder_delta = (int16_t)(encoder_now - g_previous_encoder_count);
  float raw_speed_rpm;
  float position_deg;
  float target;
  float actual;
  float error;
  float requested_pwm;

  g_previous_encoder_count = encoder_now;
  g_position_counts += (int32_t)encoder_delta;

  raw_speed_rpm = (float)encoder_delta * ENCODER_DIRECTION * 60.0f
                / (ENCODER_COUNTS_PER_REV * CONTROL_PERIOD_S);
  g_filtered_speed_rpm += 0.25f * (raw_speed_rpm - g_filtered_speed_rpm);
  g_filtered_adc += 0.10f * ((float)g_adc_raw - g_filtered_adc);

  if (ModeButtonPressed())
  {
    g_mode = (g_mode == FAN_MODE_SPEED) ? FAN_MODE_POSITION : FAN_MODE_SPEED;
    PidReset(&g_speed_pid);
    PidReset(&g_position_pid);

    /* 切入定位模式时把当前位置定义为 0 度，避免突然倒转很多圈。 */
    if (g_mode == FAN_MODE_POSITION)
    {
      g_position_zero_counts = g_position_counts;
    }
  }

  position_deg = (float)(g_position_counts - g_position_zero_counts)
               * ENCODER_DIRECTION
               * 360.0f / ENCODER_COUNTS_PER_REV;

  if (g_mode == FAN_MODE_SPEED)
  {
    target = g_filtered_adc * SPEED_TARGET_MAX_RPM / ADC_FULL_SCALE;
    if (g_filtered_adc < 80.0f)
    {
      target = 0.0f;
    }

    actual = g_filtered_speed_rpm;
    error = target - actual;

    /* 风扇调速只允许正转；目标为零时直接滑行停止并清空积分。 */
    if (target <= 0.0f)
    {
      PidReset(&g_speed_pid);
      requested_pwm = 0.0f;
    }
    else
    {
      requested_pwm = PidUpdate(&g_speed_pid,
                                error,
                                0.0f,
                                SPEED_PWM_LIMIT_PERCENT);
    }
  }
  else
  {
    target = g_filtered_adc * (2.0f * POSITION_TARGET_RANGE_DEG) / ADC_FULL_SCALE
           - POSITION_TARGET_RANGE_DEG;
    actual = position_deg;
    error = target - actual;

    if (AbsFloat(error) <= POSITION_DEADBAND_DEG
        && AbsFloat(g_filtered_speed_rpm) <= POSITION_STOP_SPEED_RPM)
    {
      PidReset(&g_position_pid);
      requested_pwm = 0.0f;
    }
    else
    {
      requested_pwm = PidUpdate(&g_position_pid,
                                error,
                                -POSITION_PWM_LIMIT_PERCENT,
                                POSITION_PWM_LIMIT_PERCENT);

    }
  }

  MotorApplyPwm(RampPwm(requested_pwm));

  if (telemetry != NULL)
  {
    telemetry->mode = (float)g_mode;
    telemetry->adc_raw = g_filtered_adc;
    telemetry->target = target;
    telemetry->actual = actual;
    telemetry->pwm_percent = g_applied_pwm_percent;
    telemetry->speed_rpm = g_filtered_speed_rpm;
    telemetry->position_deg = position_deg;
    telemetry->error = error;
  }
}

void FanControl_ForceStop(void)
{
  g_applied_pwm_percent = 0.0f;
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
  HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port, MOTOR_STBY_Pin, GPIO_PIN_RESET);
}

void FanControl_SendTelemetry(const FanTelemetry_t *telemetry)
{
  JustFloatPacket_t packet;

  if (telemetry == NULL)
  {
    return;
  }

  /* JustFloat 直接发送小端 float，帧尾固定为 00 00 80 7F。 */
  packet.values[0] = telemetry->mode;
  packet.values[1] = telemetry->adc_raw;
  packet.values[2] = telemetry->target;
  packet.values[3] = telemetry->actual;
  packet.values[4] = telemetry->pwm_percent;
  packet.values[5] = telemetry->speed_rpm;
  packet.values[6] = telemetry->position_deg;
  packet.values[7] = telemetry->error;
  packet.tail[0] = 0x00U;
  packet.tail[1] = 0x00U;
  packet.tail[2] = 0x80U;
  packet.tail[3] = 0x7FU;
  (void)HAL_UART_Transmit(&huart1,
                          (uint8_t *)&packet,
                          (uint16_t)sizeof(packet),
                          20U);
}
