#include "q5_control.h"

#include "adc.h"
#include "i2c.h"
#include "main.h"
#include "usart.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#define MPU6050_ADDRESS            (0x68u << 1)
#define MPU6050_REG_SMPLRT_DIV     0x19u
#define MPU6050_REG_CONFIG         0x1Au
#define MPU6050_REG_GYRO_CONFIG    0x1Bu
#define MPU6050_REG_ACCEL_CONFIG   0x1Cu
#define MPU6050_REG_ACCEL_XOUT_H   0x3Bu
#define MPU6050_REG_PWR_MGMT_1     0x6Bu
#define MPU6050_REG_WHO_AM_I       0x75u

#define SERVO_MIN_DEG              20u
#define SERVO_MAX_DEG              160u
#define SERVO_CENTER_DEG           90u
#define MPU_FOLLOW_LIMIT_DEG       45.0f
#define RAD_TO_DEG                 57.2957795f

static volatile uint16_t s_adc_dma[2];
static uint32_t s_adc_filtered[2];
static uint8_t s_status;
static Q5_ControlMode_t s_mode;

static bool s_mpu_filter_initialized;
static float s_mpu_roll_deg;
static float s_mpu_pitch_deg;
static uint32_t s_mpu_last_tick;
static uint32_t s_mpu_retry_tick;
static uint8_t s_mpu_error_count;

static uint8_t clamp_u8(int32_t value, uint8_t minimum, uint8_t maximum)
{
  if (value < (int32_t)minimum)
  {
    return minimum;
  }
  if (value > (int32_t)maximum)
  {
    return maximum;
  }
  return (uint8_t)value;
}

static uint8_t adc_to_servo(uint32_t adc_value)
{
  uint32_t span = SERVO_MAX_DEG - SERVO_MIN_DEG;
  if (adc_value > 4095u)
  {
    adc_value = 4095u;
  }
  return (uint8_t)(SERVO_MIN_DEG + ((adc_value * span) / 4095u));
}

static bool mpu_write(uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDRESS, reg,
                           I2C_MEMADD_SIZE_8BIT, &value, 1u, 20u) == HAL_OK;
}

static bool mpu_init(void)
{
  uint8_t who_am_i = 0u;

  if (HAL_I2C_IsDeviceReady(&hi2c2, MPU6050_ADDRESS, 2u, 20u) != HAL_OK)
  {
    return false;
  }

  if (HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDRESS, MPU6050_REG_WHO_AM_I,
                       I2C_MEMADD_SIZE_8BIT, &who_am_i, 1u, 20u) != HAL_OK)
  {
    return false;
  }

  if (who_am_i != 0x68u)
  {
    return false;
  }

  if (!mpu_write(MPU6050_REG_PWR_MGMT_1, 0x00u) ||
      !mpu_write(MPU6050_REG_SMPLRT_DIV, 0x07u) ||
      !mpu_write(MPU6050_REG_CONFIG, 0x03u) ||
      !mpu_write(MPU6050_REG_GYRO_CONFIG, 0x00u) ||
      !mpu_write(MPU6050_REG_ACCEL_CONFIG, 0x00u))
  {
    return false;
  }

  s_mpu_filter_initialized = false;
  s_mpu_last_tick = HAL_GetTick();
  s_mpu_error_count = 0u;
  return true;
}

static bool mpu_update(float *roll_deg, float *pitch_deg)
{
  uint8_t data[14];
  int16_t ax_raw;
  int16_t ay_raw;
  int16_t az_raw;
  int16_t gx_raw;
  int16_t gy_raw;
  float ax;
  float ay;
  float az;
  float acc_roll;
  float acc_pitch;
  float dt;
  uint32_t now;

  if (HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDRESS, MPU6050_REG_ACCEL_XOUT_H,
                       I2C_MEMADD_SIZE_8BIT, data, sizeof(data), 20u) != HAL_OK)
  {
    return false;
  }

  ax_raw = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
  ay_raw = (int16_t)(((uint16_t)data[2] << 8) | data[3]);
  az_raw = (int16_t)(((uint16_t)data[4] << 8) | data[5]);
  gx_raw = (int16_t)(((uint16_t)data[8] << 8) | data[9]);
  gy_raw = (int16_t)(((uint16_t)data[10] << 8) | data[11]);

  ax = (float)ax_raw;
  ay = (float)ay_raw;
  az = (float)az_raw;
  acc_roll = atan2f(ay, az) * RAD_TO_DEG;
  acc_pitch = atan2f(-ax, sqrtf((ay * ay) + (az * az))) * RAD_TO_DEG;

  now = HAL_GetTick();
  dt = (float)(now - s_mpu_last_tick) / 1000.0f;
  s_mpu_last_tick = now;
  if ((dt < 0.005f) || (dt > 0.100f))
  {
    dt = 0.020f;
  }

  if (!s_mpu_filter_initialized)
  {
    s_mpu_roll_deg = acc_roll;
    s_mpu_pitch_deg = acc_pitch;
    s_mpu_filter_initialized = true;
  }
  else
  {
    s_mpu_roll_deg = 0.98f * (s_mpu_roll_deg + (((float)gx_raw / 131.0f) * dt))
                     + 0.02f * acc_roll;
    s_mpu_pitch_deg = 0.98f * (s_mpu_pitch_deg + (((float)gy_raw / 131.0f) * dt))
                      + 0.02f * acc_pitch;
  }

  *roll_deg = s_mpu_roll_deg;
  *pitch_deg = s_mpu_pitch_deg;
  return true;
}

static void update_mode_button(void)
{
  static GPIO_PinState candidate = GPIO_PIN_SET;
  static GPIO_PinState stable = GPIO_PIN_SET;
  static uint8_t stable_count;
  GPIO_PinState sample = HAL_GPIO_ReadPin(MODE_KEY_GPIO_Port, MODE_KEY_Pin);

  if (sample == candidate)
  {
    if (stable_count < 3u)
    {
      stable_count++;
    }
  }
  else
  {
    candidate = sample;
    stable_count = 0u;
  }

  if ((stable_count >= 2u) && (stable != candidate))
  {
    stable = candidate;
    if (stable == GPIO_PIN_RESET)
    {
      if (s_mode == Q5_MODE_POTENTIOMETER)
      {
        if ((s_status & Q5_STATUS_MPU_READY) != 0u)
        {
          s_mode = Q5_MODE_MPU6050;
        }
      }
      else
      {
        s_mode = Q5_MODE_POTENTIOMETER;
      }
    }
  }
}

void Q5_ControlInit(Q5_ControlState_t *state)
{
  if (state == NULL)
  {
    return;
  }

  s_status = 0u;
  s_mode = Q5_MODE_POTENTIOMETER;
  s_adc_dma[0] = 2048u;
  s_adc_dma[1] = 2048u;
  s_adc_filtered[0] = 2048u;
  s_adc_filtered[1] = 2048u;

  if ((HAL_ADCEx_Calibration_Start(&hadc1) == HAL_OK) &&
      (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)(void *)s_adc_dma, 2u) == HAL_OK))
  {
    s_status |= Q5_STATUS_ADC_READY;
  }

  if (mpu_init())
  {
    s_status |= Q5_STATUS_MPU_READY;
  }
  s_mpu_retry_tick = HAL_GetTick();

  state->mode = (uint8_t)s_mode;
  state->pan_deg = SERVO_CENTER_DEG;
  state->tilt_deg = SERVO_CENTER_DEG;
  state->status = s_status;
}

void Q5_ControlUpdate(Q5_ControlState_t *state)
{
  uint8_t pot_pan;
  uint8_t pot_tilt;
  float roll_deg;
  float pitch_deg;
  uint32_t now;

  if (state == NULL)
  {
    return;
  }

  update_mode_button();

  s_adc_filtered[0] = ((s_adc_filtered[0] * 7u) + s_adc_dma[0]) / 8u;
  s_adc_filtered[1] = ((s_adc_filtered[1] * 7u) + s_adc_dma[1]) / 8u;
  pot_pan = adc_to_servo(s_adc_filtered[0]);
  pot_tilt = adc_to_servo(s_adc_filtered[1]);

  now = HAL_GetTick();
  if (((s_status & Q5_STATUS_MPU_READY) == 0u) &&
      ((now - s_mpu_retry_tick) >= 1000u))
  {
    s_mpu_retry_tick = now;
    if (mpu_init())
    {
      s_status |= Q5_STATUS_MPU_READY;
    }
  }

  if ((s_status & Q5_STATUS_MPU_READY) != 0u)
  {
    if (mpu_update(&roll_deg, &pitch_deg))
    {
      s_mpu_error_count = 0u;
      if (s_mode == Q5_MODE_MPU6050)
      {
        if (roll_deg > MPU_FOLLOW_LIMIT_DEG)
        {
          roll_deg = MPU_FOLLOW_LIMIT_DEG;
        }
        else if (roll_deg < -MPU_FOLLOW_LIMIT_DEG)
        {
          roll_deg = -MPU_FOLLOW_LIMIT_DEG;
        }
        if (pitch_deg > MPU_FOLLOW_LIMIT_DEG)
        {
          pitch_deg = MPU_FOLLOW_LIMIT_DEG;
        }
        else if (pitch_deg < -MPU_FOLLOW_LIMIT_DEG)
        {
          pitch_deg = -MPU_FOLLOW_LIMIT_DEG;
        }
        state->pan_deg = clamp_u8((int32_t)(SERVO_CENTER_DEG + roll_deg),
                                  SERVO_MIN_DEG, SERVO_MAX_DEG);
        state->tilt_deg = clamp_u8((int32_t)(SERVO_CENTER_DEG + pitch_deg),
                                   SERVO_MIN_DEG, SERVO_MAX_DEG);
      }
    }
    else
    {
      if (++s_mpu_error_count >= 5u)
      {
        s_status &= (uint8_t)~Q5_STATUS_MPU_READY;
        s_mode = Q5_MODE_POTENTIOMETER;
      }
    }
  }

  if (s_mode == Q5_MODE_POTENTIOMETER)
  {
    state->pan_deg = pot_pan;
    state->tilt_deg = pot_tilt;
  }

  state->mode = (uint8_t)s_mode;
  state->status = s_status;
}

void Q5_ControlSend(const Q5_ControlState_t *state)
{
  static uint8_t sequence;
  uint8_t packet[8];
  uint8_t checksum = 0u;
  uint8_t index;

  if (state == NULL)
  {
    return;
  }

  packet[0] = 0xAAu;
  packet[1] = 0x55u;
  packet[2] = state->mode;
  packet[3] = state->pan_deg;
  packet[4] = state->tilt_deg;
  packet[5] = state->status;
  packet[6] = sequence++;

  for (index = 0u; index < 7u; index++)
  {
    checksum ^= packet[index];
  }
  packet[7] = checksum;

  (void)HAL_UART_Transmit(&huart1, packet, sizeof(packet), 20u);
}
