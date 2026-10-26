/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
/* USER CODE END Includes */

extern UART_HandleTypeDef huart1;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_USART1_UART_Init(void);

/* USER CODE BEGIN Prototypes */
HAL_StatusTypeDef uart_rx_init(void);    /* 只建队列；调度器启动前调用（RTOS_QUEUES 区） */
HAL_StatusTypeDef uart_rx_start(void);   /* 启动 DMA 接收；返回 HAL 状态，调用者必须检查 */
uint16_t uart_rx_read(uint8_t *dst, uint16_t max);
uint32_t uart_rx_overflow(void);         /* 软件环累计丢了多少字节 */
uint32_t uart_rx_evt_lost(void);         /* 零增量事件次数：未知量，非零 = ISR 被饿死过 */
uint32_t uart_rx_evt_qfull(void);        /* 队列满、事件没发出去的次数（数据仍在环里） */
QueueHandle_t uart_rx_queue(void);       /* 消费者拿队列句柄 */
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

