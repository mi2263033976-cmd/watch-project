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
#include "usart.h"
#include "frame.h"
#include "string.h"
#include "queue.h"
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
TaskHandle_t rxTaskHandle = NULL;       /* RxTask 句柄（动态创建，失败时为 NULL） */


/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void StartSensorTask(void *argument);
static void StartRxTask(void *argument);
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
  /* 队列在调度器启动前建好：DMA 中断可能早于任何任务首次运行，
   * 句柄为 NULL 时往队列发事件 = 访问空指针。 */
  if (uart_rx_init() != HAL_OK) {
    SEGGER_RTT_printf(0, "[ERR] uart_rx_init failed (heap?)\r\n");
  }
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

  BaseType_t ret2 = xTaskCreate(StartRxTask,       
                               "RxTask",          
                               256,                   
                               NULL,                  
                               tskIDLE_PRIORITY + 4,  
                               &rxTaskHandle);    
   if (ret2 != pdPASS) {
    SEGGER_RTT_printf(0, "[ERR] RxTask create failed, ret2=%d\r\n", (int)ret2);
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
  (void)argument;

  /* CubeMX 要求至少保留一个任务，本任务只占位，不承担业务。
   * ⚠️ 两条硬约束：
   *   1) 任务函数永不 return（返回会跳 prvTaskExitError 停机）⇒ 必须 for(;;)
   *   2) 不要在这里碰 uart_rx_*：环的读指针只能有一个消费者（StartRxTask） */
  for(;;)
  {
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

static void StartRxTask(void *argument)
{
  (void)argument;
  static uint32_t total = 0;
  static uint8_t out[32];        /* 一帧数据区最大 32 */
  uint32_t last_dump = 0;       /* 记“上次打总账的时刻”（tick）*/

  /* 检查返回值：启动失败时 total 会一直是 0，光看接线找不到原因 */
  HAL_StatusTypeDef rx_st = uart_rx_start();
  if (rx_st != HAL_OK) {
    SEGGER_RTT_printf(0, "[ERR] uart_rx_start st=%d\r\n", (int)rx_st);
  }

  QueueHandle_t q = uart_rx_queue();
  while (NULL == q)                 /* 队列没建好就等 */
  {
    vTaskDelay(pdMS_TO_TICKS(10));
    q = uart_rx_queue();
  }

  for(;;)
  {
    uint16_t evt = 0;       /*evt 只当"叫醒服务"*/
    (void)xQueueReceive(q, &evt, pdMS_TO_TICKS(5));   /* 结果不参与判断 */
         
      uint32_t prog = 0, okn = 0, guard = 0; //丢掉的字节/成功交付的帧数/死循环保险
      
      for(;;)
      {
        uint8_t len = 0;
        frame_err_t r = frame_poll(xTaskGetTickCount() * portTICK_PERIOD_MS, out, &len);
        
        /* 解析一步：等 / 丢 1 字节 / 交整帧 */
        if (r == FRAME_OK)    //收到一帧
        { 
          okn ++; total += len; 
          SEGGER_RTT_printf(0, "  FRAME len=%u d0=%02X dlast=%02X\r\n",
                      len, out[0], len ? out[len-1] : 0);
        } 
        else if (r == FRAME_PROGRESS)   {prog ++;}     //有进展，再来一次
        else break;                                    //FRAME_WAIT：没进展，等下次唤醒
        
        /* 保险：防止环里有垃圾数据，poll 永远进不去 FRAME_WAIT，死循环。 */
        if(++guard > 2000)
        { SEGGER_RTT_printf(0, "[W] poll guard!\r\n"); break; }
      }
      
      /* 每次唤醒最多一行，避免 RTT 堵塞 */
      if(prog || okn)     
      SEGGER_RTT_printf(0, "wake evt=%u prog=%u ok=%u total=%u\r\n",
                          (unsigned)evt, (unsigned)prog, (unsigned)okn, (unsigned)total);
      
      uint32_t now = xTaskGetTickCount();            /* 每秒一行总账 */
      if ((now - last_dump) >= pdMS_TO_TICKS(1000)) 
      { last_dump = now; frame_print_counters(); }    

    
  }
}

/* USER CODE END Application */

