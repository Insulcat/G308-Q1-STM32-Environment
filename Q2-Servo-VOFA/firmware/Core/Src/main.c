/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SERVO_MIN_ANGLE_DEG  0.0f
#define SERVO_MAX_ANGLE_DEG  180.0f
#define SERVO_MIN_PULSE_US   1000U
#define SERVO_MAX_PULSE_US   2000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static float servo_current_angle_deg = 90.0f;
static uint16_t servo_pulse_us = 1500U;
static uint8_t uart_rx_byte;
static char uart_rx_buffer[16];
static volatile uint8_t uart_rx_index = 0U;
static volatile uint8_t uart_command_ready = 0U;
static uint32_t vofa_last_send_tick = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Servo_SetAngle(float angle_deg);
static void VOFA_SendData(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
{
  Error_Handler();
}

Servo_SetAngle(90.0f);
if (HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1U) != HAL_OK)
{
  Error_Handler();
}
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (uart_command_ready != 0U)
{
  char *number_text = uart_rx_buffer;
  char *end_ptr;

  if ((number_text[0] == 'A') || (number_text[0] == 'a'))
  {
    number_text++;
  }

  float target_angle = strtof(number_text, &end_ptr);

  if (end_ptr != number_text)
  {
    Servo_SetAngle(target_angle);
  }

  uart_command_ready = 0U;
}
if ((HAL_GetTick() - vofa_last_send_tick) >= 50U)
{
  vofa_last_send_tick = HAL_GetTick();
  VOFA_SendData();
}
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
static void Servo_SetAngle(float angle_deg)
{
  if (angle_deg < SERVO_MIN_ANGLE_DEG)
  {
    angle_deg = SERVO_MIN_ANGLE_DEG;
  }
  else if (angle_deg > SERVO_MAX_ANGLE_DEG)
  {
    angle_deg = SERVO_MAX_ANGLE_DEG;
  }

  
  servo_current_angle_deg = angle_deg;

  servo_pulse_us =
      SERVO_MIN_PULSE_US +
      (uint16_t)((angle_deg / 180.0f) *
      (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) + 0.5f);

  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, servo_pulse_us);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    if (uart_command_ready == 0U)
    {
      if (uart_rx_byte == '\n')
      {
        uart_rx_buffer[uart_rx_index] = '\0';
        uart_rx_index = 0U;
        uart_command_ready = 1U;
      }
      else if (uart_rx_byte != '\r')
      {
        if (uart_rx_index < (sizeof(uart_rx_buffer) - 1U))
        {
          uart_rx_buffer[uart_rx_index++] = (char)uart_rx_byte;
        }
        else
        {
          uart_rx_index = 0U;
        }
      }
    }

    HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1U);
  }
}

static void VOFA_SendData(void)
{
  uint8_t frame[12];
  float duty_percent =
      ((float)servo_pulse_us / 20000.0f) * 100.0f;

  memcpy(&frame[0], &servo_current_angle_deg, sizeof(float));
  memcpy(&frame[4], &duty_percent, sizeof(float));

  frame[8]  = 0x00;
  frame[9]  = 0x00;
  frame[10] = 0x80;
  frame[11] = 0x7F;

  (void)HAL_UART_Transmit(&huart1, frame, sizeof(frame), 20U);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
