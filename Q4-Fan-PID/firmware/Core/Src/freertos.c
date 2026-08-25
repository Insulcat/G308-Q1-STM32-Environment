/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "fan_control.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/* 两个任务通过互斥量共享这份最新状态，避免发送到一半时数据被更新。 */
static FanTelemetry_t g_latest_telemetry = {0};
/* USER CODE END Variables */
osThreadId ControlTaskHandle;
osThreadId TelemetryTaskHandle;
osMutexId DataMutexHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartControlTask(void const * argument);
void StartTelemetryTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* definition and creation of DataMutex */
  osMutexDef(DataMutex);
  DataMutexHandle = osMutexCreate(osMutex(DataMutex));

  /* USER CODE BEGIN RTOS_MUTEX */
  /* 互斥量创建失败时不启动控制系统。 */
  if (DataMutexHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of ControlTask */
  osThreadDef(ControlTask, StartControlTask, osPriorityAboveNormal, 0, 384);
  ControlTaskHandle = osThreadCreate(osThread(ControlTask), NULL);

  /* definition and creation of TelemetryTask */
  osThreadDef(TelemetryTask, StartTelemetryTask, osPriorityNormal, 0, 256);
  TelemetryTaskHandle = osThreadCreate(osThread(TelemetryTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  if (ControlTaskHandle == NULL || TelemetryTaskHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartControlTask */
/**
  * @brief  Function implementing the ControlTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartControlTask */
void StartControlTask(void const * argument)
{
  /* USER CODE BEGIN StartControlTask */
  FanTelemetry_t new_telemetry;

  (void)argument;

  if (!FanControl_Init())
  {
    /* 任一外设启动失败都保持 STBY 为低，防止电机误转。 */
    FanControl_ForceStop();
    for (;;)
    {
      osDelay(1000U);
    }
  }

  for(;;)
  {
    FanControl_Update(&new_telemetry);

    if (osMutexWait(DataMutexHandle, osWaitForever) == osOK)
    {
      g_latest_telemetry = new_telemetry;
      (void)osMutexRelease(DataMutexHandle);
    }

    /* 当前 FreeRTOS 配置保留普通延时接口，控制任务按 10 ms 周期运行。 */
    osDelay(10U);
  }
  /* USER CODE END StartControlTask */
}

/* USER CODE BEGIN Header_StartTelemetryTask */
/**
* @brief Function implementing the TelemetryTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTelemetryTask */
void StartTelemetryTask(void const * argument)
{
  /* USER CODE BEGIN StartTelemetryTask */
  FanTelemetry_t telemetry_copy = {0};

  (void)argument;
  osDelay(100U);

  for(;;)
  {
    if (osMutexWait(DataMutexHandle, osWaitForever) == osOK)
    {
      telemetry_copy = g_latest_telemetry;
      (void)osMutexRelease(DataMutexHandle);
    }

    /* 20 Hz 刷新足够观察 PID 曲线，也不会让串口长期占用 CPU。 */
    FanControl_SendTelemetry(&telemetry_copy);
    osDelay(50U);
  }
  /* USER CODE END StartTelemetryTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

