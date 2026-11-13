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
#include "mpu6050.h"
#include "mpu6050_handler.h"
#include "soft_i2c.h"
#include "usart.h"
#include "frame.h"
#include "string.h"
#include "queue.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* ═══ 设备表（W9 第 4 步）════════════════════════════════════════════════
 * 目的：加一个器件 = 表里加一行 + 写一个"读一次"函数；任务代码 diff = 0。
 * 约束：表项的 read() 必须【不阻塞】——否则会拖慢同一张表里的其他器件。
 * ═══════════════════════════════════════════════════════════════════ */
typedef enum {
    DEV_OK  = 0,     /* 这次读到了   */
    DEV_ERR,         /* 这次没读到   */
} dev_result_t;

/* "读一次"。判据：里面不许 vTaskDelay / HAL_Delay / 长时间等锁。 */
typedef dev_result_t (*dev_read_fn)(void *ctx);

typedef struct {
    const char *name;       /* 调试用：日志里认得出是谁          */
    uint32_t    period_ms;  /* 采样周期                          */
    uint32_t    next_due;   /* 下次到期的【绝对时刻】(tick)       */
    uint16_t    fail_cnt;   /* 这一项连续出错次数（0 = 正常） */
    dev_read_fn read;       /* 读一次（非阻塞）                   */
    void       *ctx;        /* 该器件自己的出参缓冲/状态           */
} dev_item_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* W9 撞车实验脚手架已于 2026-11-13 清理：
 *   实验开关 W9_RUN_MPU_TASK / 探针地址 MPU_PROBE_ADDR7 / WHO_AM_I 期望值 /
 *   耗时门限 MPU_SLOW_US —— 随 Mpu6050Task 一起删除，证据已留档在
 *   teaching/progress.md（T=1499 → 243）与 W9任务-学习笔记.md。 */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */


/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
TaskHandle_t aht21TaskHandle = NULL;   /* Aht21Task 句柄（动态创建，失败时为 NULL） */
TaskHandle_t rxTaskHandle = NULL;       /* RxTask 句柄（动态创建，失败时为 NULL） */
TaskHandle_t sensorTaskHandle = NULL;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128,
  .priority = (osPriority_t) osPriorityIdle,   /* = FreeRTOS 优先级 1（tskIDLE_PRIORITY+1）：
                                                 占位任务只跑 osDelay，给最低，别跟 Aht21(3)/Rx(4) 抢。
                                                 ⚠️ CubeMX 重生成会冲掉本行 —— 见下方 USER CODE 警示。 */
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void StartAht21Task(void *argument);
static void StartRxTask(void *argument);
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
  SEGGER_RTT_printf(0, "bus_lock=%u\r\n", (unsigned)i2c_bus_lock_init());
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
  /* ⚠️ defaultTask 的优先级写在 defaultTask_attributes（生成区），不在 USER CODE 段
   *    ⇒ CubeMX 重生成会把 osPriorityIdle 冲回 osPriorityNormal。
   *    重生成后务必检查：.priority 必须仍是 osPriorityIdle（= FreeRTOS 1），否则占位任务会高过 RxTask/Aht21Task。 */

  BaseType_t ret = xTaskCreate(StartAht21Task,       /* ① 任务函数                     */
                               "Aht21Task",          /* ② 任务名（调试用）              */
                               256,                   /* ③ 栈大小 = 256 word = 1KB      */
                               NULL,                  /* ④ 传给任务函数的参数            */
                               tskIDLE_PRIORITY + 3,  /* ⑤ 优先级 = 空闲任务优先级 + 3 .数字越大越高，< 56  */
                               &aht21TaskHandle);    /* ⑥ 返回任务句柄                 */
  if (ret != pdPASS) {
    SEGGER_RTT_printf(0, "[ERR] Aht21Task create failed, ret=%d\r\n", (int)ret);
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

  BaseType_t ret4 = xTaskCreate(StartSensorTask,       
                               "SensorTask",          
                               256,                   
                               NULL,                  
                               tskIDLE_PRIORITY + 4,  
                               &sensorTaskHandle);    
   if (ret4 != pdPASS) {
    SEGGER_RTT_printf(0, "[ERR] SensorTask create failed, ret4=%d\r\n", (int)ret4);
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

/* ── 器件上下文：每个器件一块自己的出参缓冲 ─────────────────────────── */
static mpu6050_data_t s_mpu_ctx;

/* ── 适配：把"裸驱动的返回值"翻译成表能看懂的 DEV_OK / DEV_ERR ─────── */
static dev_result_t MpuAdapterRead(void *ctx)
{
    mpu6050_data_t *d = (mpu6050_data_t *)ctx;
    return (MPU6050_OK == mpu6050_read(d)) ? DEV_OK : DEV_ERR;
}

/* ── 设备表：加器件就加一行 ────────────────────────────────────────── */
static dev_item_t s_dev_table[] = {
    /* name        period_ms   next_due   fail_cnt      read             ctx        */
    {  "mpu6050",   20,         0,         0,          MpuAdapterRead,  &s_mpu_ctx  },
};

//s_dev_table 的设备项数，由表总字节数除以单个表项字节数自动得出
#define DEV_COUNT   ((uint8_t)(sizeof s_dev_table / sizeof s_dev_table[0]))

/* 查一项的健康状况：找到返回 1，没这个器件返回 0。
 * 判据：按【名字】查，不按表下标 —— 下标会随加行/删行变化，调用方写死下标就脆。
 * 只读，且 fail_cnt 是 16 位对齐 ⇒ 单条指令原子，不需要加锁。
 * ⚠️ 当前【无调用方】：留给验收④（屏/LVGL 显示"传感器离线"）用的接口，别当死代码删。 */
uint8_t SensorFailCount(const char *name, uint16_t *fail_out)
{
  for (uint8_t i = 0; i < DEV_COUNT; i++)
  {
    if(0 == strcmp(s_dev_table[i].name, name))
    {
      if(NULL != fail_out)  {*fail_out = s_dev_table[i].fail_cnt;}
      return 1;
    }
  }
  return 0;
}

/* ── 表驱动采样任务：3 句话，3 个坑 ─────────────────────────────────── */
static void StartSensorTask(void *argument)
{
  (void)argument;

  for(;;)
  {
    uint32_t now = xTaskGetTickCount();
    uint32_t wait = portMAX_DELAY;
    uint8_t i;
    for (i = 0; i < DEV_COUNT; i++)
    {
      dev_item_t *it = &s_dev_table[i];

      if((int32_t)(now - it->next_due) >= 0)    //到期了
      {
        if(NULL != it->read)    
        {
          if (DEV_OK != it->read(it->ctx)) { if(it->fail_cnt < UINT16_MAX) it->fail_cnt++; }   /* 记到本人头上 */
          else                             { it->fail_cnt = 0; } /* 成功就清零 */
        }
        it->next_due += it->period_ms;             //累加一个周期 
      }
      uint32_t d = it->next_due - now;          //离下次到期还有多久 
      if ((int32_t)d < (int32_t)wait) { wait = d; }
    }

    if((int32_t)wait <= 0)  wait = 1;       //已过期：至少睡 1 tick
    vTaskDelay(wait);
  }
}

/* 传感器任务：先初始化一次，然后每 1 s 取一次（handler 内部有缓存，不碰总线以外的逻辑）。
 * 为什么初始化也放这里：aht21.c 内部用 vTaskDelay，调度器启动后才能调。 */
static void StartAht21Task(void *argument)
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
          // SEGGER_RTT_printf(0, "  FRAME len=%u d0=%02X dlast=%02X\r\n",
          //             len, out[0], len ? out[len-1] : 0);
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

