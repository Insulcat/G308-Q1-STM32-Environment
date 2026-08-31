#include "q5_receiver.h"

#include "cmsis_os.h"
#include "tim.h"
#include "usart.h"

#include <stddef.h>

#define PACKET_HEADER_1        0xAAu
#define PACKET_HEADER_2        0x55u
#define PACKET_LENGTH          8u

#define SERVO_MIN_DEG          20u
#define SERVO_MAX_DEG          160u
#define SERVO_CENTER_DEG       90u
#define SERVO_MIN_PULSE_US     1000u
#define SERVO_MAX_PULSE_US     2000u
#define SERVO_STEP_DEG         3u
#define LINK_TIMEOUT_MS        500u

extern osMessageQId RxByteQueueHandle;

static uint8_t s_uart_rx_byte;
static uint8_t s_packet[PACKET_LENGTH];
static uint8_t s_packet_index;
static uint8_t s_current_pan = SERVO_CENTER_DEG;
static uint8_t s_current_tilt = SERVO_CENTER_DEG;

static uint8_t clamp_angle(uint8_t angle)
{
  if (angle < SERVO_MIN_DEG)
  {
    return SERVO_MIN_DEG;
  }
  if (angle > SERVO_MAX_DEG)
  {
    return SERVO_MAX_DEG;
  }
  return angle;
}

static uint16_t angle_to_pulse(uint8_t angle)
{
  uint32_t scaled;

  angle = clamp_angle(angle);
  scaled = ((uint32_t)(angle - SERVO_MIN_DEG) *
            (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) /
           (SERVO_MAX_DEG - SERVO_MIN_DEG);
  return (uint16_t)(SERVO_MIN_PULSE_US + scaled);
}

static uint8_t approach_angle(uint8_t current, uint8_t target)
{
  if (current < target)
  {
    uint16_t next = (uint16_t)current + SERVO_STEP_DEG;
    return (next > target) ? target : (uint8_t)next;
  }
  if (current > target)
  {
    return ((uint16_t)target + SERVO_STEP_DEG > current)
             ? target
             : (uint8_t)(current - SERVO_STEP_DEG);
  }
  return current;
}

void Q5_ReceiverInit(Q5_ServoCommand_t *command)
{
  if (command == NULL)
  {
    return;
  }

  command->mode = 0u;
  command->pan_deg = SERVO_CENTER_DEG;
  command->tilt_deg = SERVO_CENTER_DEG;
  command->status = 0u;
  command->sequence = 0u;
  command->last_rx_tick = 0u;

  s_current_pan = SERVO_CENTER_DEG;
  s_current_tilt = SERVO_CENTER_DEG;
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, angle_to_pulse(s_current_pan));
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, angle_to_pulse(s_current_tilt));
  (void)HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  (void)HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

  s_packet_index = 0u;
  (void)HAL_UART_Receive_IT(&huart1, &s_uart_rx_byte, 1u);
}

bool Q5_ReceiverParseByte(uint8_t byte, Q5_ServoCommand_t *command)
{
  uint8_t checksum = 0u;
  uint8_t index;

  if (command == NULL)
  {
    return false;
  }

  if (s_packet_index == 0u)
  {
    if (byte == PACKET_HEADER_1)
    {
      s_packet[0] = byte;
      s_packet_index = 1u;
    }
    return false;
  }

  if (s_packet_index == 1u)
  {
    if (byte == PACKET_HEADER_2)
    {
      s_packet[1] = byte;
      s_packet_index = 2u;
    }
    else if (byte != PACKET_HEADER_1)
    {
      s_packet_index = 0u;
    }
    return false;
  }

  s_packet[s_packet_index++] = byte;
  if (s_packet_index < PACKET_LENGTH)
  {
    return false;
  }
  s_packet_index = 0u;

  for (index = 0u; index < (PACKET_LENGTH - 1u); index++)
  {
    checksum ^= s_packet[index];
  }

  if ((checksum != s_packet[7]) ||
      (s_packet[2] > 1u) ||
      (s_packet[3] > 180u) ||
      (s_packet[4] > 180u))
  {
    return false;
  }

  command->mode = s_packet[2];
  command->pan_deg = clamp_angle(s_packet[3]);
  command->tilt_deg = clamp_angle(s_packet[4]);
  command->status = s_packet[5];
  command->sequence = s_packet[6];
  command->last_rx_tick = HAL_GetTick();
  return true;
}

void Q5_ServoApply(const Q5_ServoCommand_t *command)
{
  uint8_t target_pan = SERVO_CENTER_DEG;
  uint8_t target_tilt = SERVO_CENTER_DEG;
  uint32_t now = HAL_GetTick();

  if ((command != NULL) &&
      (command->last_rx_tick != 0u) &&
      ((now - command->last_rx_tick) <= LINK_TIMEOUT_MS))
  {
    target_pan = clamp_angle(command->pan_deg);
    target_tilt = clamp_angle(command->tilt_deg);
  }

  s_current_pan = approach_angle(s_current_pan, target_pan);
  s_current_tilt = approach_angle(s_current_tilt, target_tilt);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, angle_to_pulse(s_current_pan));
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, angle_to_pulse(s_current_tilt));
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    if (RxByteQueueHandle != NULL)
    {
      (void)osMessagePut(RxByteQueueHandle, s_uart_rx_byte, 0u);
    }
    (void)HAL_UART_Receive_IT(&huart1, &s_uart_rx_byte, 1u);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    (void)HAL_UART_Receive_IT(&huart1, &s_uart_rx_byte, 1u);
  }
}
