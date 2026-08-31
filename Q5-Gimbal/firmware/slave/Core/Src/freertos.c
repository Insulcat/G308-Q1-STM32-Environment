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
#include "q5_receiver.h"
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
static Q5_ServoCommand_t s_servoCommand;
/* USER CODE END Variables */
osThreadId RxTaskHandle;
osThreadId ServoTaskHandle;
osMessageQId RxByteQueueHandle;
osMutexId ServoMutexHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartRxTask(void const * argument);
void StartServoTask(void const * argument);

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
  /* definition and creation of ServoMutex */
  osMutexDef(ServoMutex);
  ServoMutexHandle = osMutexCreate(osMutex(ServoMutex));

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of RxByteQueue */
  osMessageQDef(RxByteQueue, 32, uint8_t);
  RxByteQueueHandle = osMessageCreate(osMessageQ(RxByteQueue), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of RxTask */
  osThreadDef(RxTask, StartRxTask, osPriorityAboveNormal, 0, 256);
  RxTaskHandle = osThreadCreate(osThread(RxTask), NULL);

  /* definition and creation of ServoTask */
  osThreadDef(ServoTask, StartServoTask, osPriorityNormal, 0, 256);
  ServoTaskHandle = osThreadCreate(osThread(ServoTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartRxTask */
/**
  * @brief  Function implementing the RxTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartRxTask */
void StartRxTask(void const * argument)
{
  /* USER CODE BEGIN StartRxTask */
  Q5_ServoCommand_t localCommand;
  osEvent event;

  (void)argument;
  Q5_ReceiverInit(&localCommand);
  if (osMutexWait(ServoMutexHandle, osWaitForever) == osOK)
  {
    s_servoCommand = localCommand;
    osMutexRelease(ServoMutexHandle);
  }

  /* Infinite loop */
  for(;;)
  {
    event = osMessageGet(RxByteQueueHandle, osWaitForever);
    if ((event.status == osEventMessage) &&
        Q5_ReceiverParseByte((uint8_t)event.value.v, &localCommand))
    {
      if (osMutexWait(ServoMutexHandle, osWaitForever) == osOK)
      {
        s_servoCommand = localCommand;
        osMutexRelease(ServoMutexHandle);
      }
    }
  }
  /* USER CODE END StartRxTask */
}

/* USER CODE BEGIN Header_StartServoTask */
/**
* @brief Function implementing the ServoTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartServoTask */
void StartServoTask(void const * argument)
{
  /* USER CODE BEGIN StartServoTask */
  Q5_ServoCommand_t localCommand = {0};

  (void)argument;
  osDelay(20);
  /* Infinite loop */
  for(;;)
  {
    if (osMutexWait(ServoMutexHandle, osWaitForever) == osOK)
    {
      localCommand = s_servoCommand;
      osMutexRelease(ServoMutexHandle);
    }
    Q5_ServoApply(&localCommand);
    osDelay(20);
  }
  /* USER CODE END StartServoTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

