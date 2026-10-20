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
#include "SEGGER_RTT.h"
#include "aht21.h"
#include "aht21_handler.h"

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
TaskHandle_t sensorTaskHandle = NULL;   /* sensorTask 句柄（动态创建，失败时为 NULL） */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void StartSensorTask(void *argument);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
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
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  BaseType_t ret = xTaskCreate(StartSensorTask,       /* ① 任务函数                     */
                               "SensorTask",          /* ② 任务名（调试用）              */
                               256,                   /* ③ 栈大小 = 256 word = 1KB      */
                               NULL,                  /* ④ 传给任务函数的参数            */
                               tskIDLE_PRIORITY + 3,  /* ⑤ 优先级 = 空闲任务优先级 + 3 .数字越大越高，< 56  */
                               &sensorTaskHandle);    /* ⑥ 返回任务句柄                 */
  if (ret != pdPASS) {
    SEGGER_RTT_printf(0, "[ERR] sensorTask create failed, ret=%d\r\n", (int)ret);
  }

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    SEGGER_RTT_printf(0, "test\r\n");
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* 传感器任务：先初始化一次，然后每 1 s 取一次（handler 内部有缓存，不碰总线以外的逻辑）。
 * 为什么初始化也放这里：aht21.c 内部用 vTaskDelay，调度器启动后才能调。 */
static void StartSensorTask(void *argument)
{
  (void)argument;

  aht21_err_t init_err = aht21_init();
  if (init_err != AHT21_OK)
  {
    SEGGER_RTT_printf(0, "AHT21 init FAIL = %d\r\n", init_err);
    /* 故意不 return：任务函数一旦返回就停机（port.c 的 prvTaskExitError），
     * 且器件坏掉时 handler 会自动降级成 H_OFFLINE，不需要在这里退出。 */
  }

  for(;;)
  {
    float t = 0.0f;
    float h = 0.0f;
    aht21_handler_status_t st = aht21_handler_read(&t,&h);  

    /* RTT 的 printf 不支持 %f → 用定点整数（×10） */
    SEGGER_RTT_printf(0, "T=%d H=%d st=%d\r\n",
                      (int)(t * 10.0f), (int)(h * 10.0f), (int)st);

    UBaseType_t hw = uxTaskGetStackHighWaterMark(NULL);   /* NULL = 当前任务 */
    SEGGER_RTT_printf(0, "stack_hw=%u word\r\n", (unsigned)hw);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
/* 栈溢出钩子：覆盖 CMSIS 的 __WEAK 版（cmsis_os2.c）。
 * 打印任务名后关中断停机 —— 便于用 Ozone 接上抓现场，别让它继续踩内存。 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName)
{
    (void)xTask;
    SEGGER_RTT_printf(0, "\r\n[FATAL] STACK OVERFLOW: %s\r\n", (char *)pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;) { }        /* 停在这里，等 Ozone 接上 */
}



/* USER CODE END Application */

