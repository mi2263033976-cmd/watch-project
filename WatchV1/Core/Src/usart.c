/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "usart.h"

/* USER CODE BEGIN 0 */
#include "SEGGER_RTT.h"

/* USER CODE END 0 */

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 DMA Init */
    /* USART1_RX Init */
    hdma_usart1_rx.Instance = DMA2_Stream2;
    hdma_usart1_rx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_CIRCULAR;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmarx,hdma_usart1_rx);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

    /* USART1 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmarx);

    /* USART1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
#define UART_RX_BUF_SIZE  256          /* 软件环：任务从这里取（2 的幂，靠 & 回绕） */
#define DMA_RX_SIZE       256          /* 硬件环：DMA 的临时落脚点，每次事件必须清空 */
static uint8_t dma_buf[DMA_RX_SIZE];       /* DMA 直接往里写，不能用局部变量 */

static uint8_t  g_rx_buf[UART_RX_BUF_SIZE];   /* 环形区：唯一写者是 rx_push */
static volatile uint16_t g_rx_wr = 0;         /* 写指针：异步上下文（DMA 事件）独占 */
static uint16_t g_rx_rd = 0;                  /* 读指针：任务独占 */
static uint32_t g_rx_overflow = 0;            /* 溢出累计（任务写、任务读） */
static uint32_t g_evt_lost   = 0;             /* Size==last 的事件数：非零 ⇒ 有整圈被盖 */

/*队列 */
#define RX_EVT_Q_LEN   8                       /* 队列深度 */
static QueueHandle_t g_rx_q = NULL;             /* 队列句柄：任务读、事件写 */

/* 启动接收。返回 HAL 状态 —— 调用者必须检查，否则启动失败只会表现为 total 一直是 0 */
HAL_StatusTypeDef uart_rx_start(void)
{
    if( NULL == g_rx_q)      
    {
        g_rx_q = xQueueCreate(RX_EVT_Q_LEN, sizeof(uint16_t));
        if( NULL == g_rx_q)
        {
            return HAL_ERROR;        /* 防止重复调用把队列建两份 */
        }
    }
    return HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_buf, DMA_RX_SIZE);
}

QueueHandle_t uart_rx_queue(void) { return g_rx_q; }   /* 消费者拿句柄 */

/* 写软件环的唯一入口：换搬运工时只改调用者，不改这里 */
static void rx_push(uint8_t b)
{
    g_rx_buf[g_rx_wr & (UART_RX_BUF_SIZE - 1)] = b;
    g_rx_wr++;
}

/* 溢出修正：读指针落后超过一整圈 ⇒ 追不回的丢掉并记账。
 * 只有任务会写 g_rx_rd，所以本函数只能在任务上下文调用。 */
static void rx_drop_if_overflow(void)
{
    uint16_t avail = (uint16_t)(g_rx_wr - g_rx_rd);
    if (avail > UART_RX_BUF_SIZE) {
        g_rx_overflow += (uint32_t)(avail - UART_RX_BUF_SIZE);
        g_rx_rd = (uint16_t)(g_rx_wr - UART_RX_BUF_SIZE);   /* 停在活着的最老数据上 */
    }
}

static uint16_t g_last_pos = 0;   /* 上一次事件的坐标：只有本回调碰它 */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    if (Size > g_last_pos)                     /* 没跨圈：一段 */
    {
        for (uint16_t i = g_last_pos; i < Size; i++)
        {
            rx_push(dma_buf[i]);
        }
    }
    else if (Size < g_last_pos)                /* 跨圈：两段，第二段终点就是 Size 本身 */
    {
        for (uint16_t i = g_last_pos; i < DMA_RX_SIZE; i++)
        {
            rx_push(dma_buf[i]);
        }
        for (uint16_t i = 0; i < Size; i++)
        {
            rx_push(dma_buf[i]);
        }
    }
    else                                       /* Size == last：零字节，但不是"没事发生" */
    {
        g_evt_lost++;                          /* 可能是整整转了一圈、数据全被盖 ⇒ 留痕 */
    }

    /* 算出数据放入到队列中 */
    uint16_t data = 0;  /* 事件数据：本回调里累加，最后一次性发给任务 */
    if (Size >= g_last_pos) 
    {
        data = (uint16_t)(Size - g_last_pos);                 /* 没跨圈（相等时 = 0） */
    } 
    else 
    {
        data = (uint16_t)(DMA_RX_SIZE - g_last_pos + Size);   /* 跨圈 */
    }
    
    for (uint16_t i = 0; i < data; i++) 
    {
        rx_push(dma_buf[(g_last_pos + i) & (DMA_RX_SIZE - 1)]);   /* 掩码自带回绕 */
    }

    if (data > 0) 
    {
        BaseType_t woken = pdFALSE;
        xQueueSendFromISR(g_rx_q, &data, &woken);
        portYIELD_FROM_ISR(woken);      /* 如果这一下唤醒了更高优先级的任务，立刻切 */
    }

    g_last_pos = Size;
}

uint16_t uart_rx_read(uint8_t *dst, uint16_t max)
{
    rx_drop_if_overflow();                     /* ① 进来先清债 */

    uint16_t avail = (uint16_t)(g_rx_wr - g_rx_rd);
    if (avail > max) avail = max;              /* ② 接口带长度、内部截断 */

    for (uint16_t i = 0; i < avail; i++)
    {
        dst[i] = g_rx_buf[g_rx_rd & (UART_RX_BUF_SIZE - 1)];
        g_rx_rd++;
    }

    rx_drop_if_overflow();                     /* ③ 拷贝这几百 µs 里又溢出了，再修一次 */
    return avail;
}

uint32_t uart_rx_overflow(void) { return g_rx_overflow; }
uint32_t uart_rx_evt_lost(void) { return g_evt_lost; }

/* USER CODE END 1 */
