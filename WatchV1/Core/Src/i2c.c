/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.c
  * @brief   This file provides code for the configuration
  *          of the I2C instances.
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
#include "i2c.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

I2C_HandleTypeDef hi2c1;

/* I2C1 init function */
void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

void HAL_I2C_MspInit(I2C_HandleTypeDef* i2cHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(i2cHandle->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspInit 0 */

  /* USER CODE END I2C1_MspInit 0 */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C1 GPIO Configuration
    PB7     ------> I2C1_SDA
    PB8     ------> I2C1_SCL
    */
    GPIO_InitStruct.Pin = SDA_Pin|SCL_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* I2C1 clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();
  /* USER CODE BEGIN I2C1_MspInit 1 */

  /* USER CODE END I2C1_MspInit 1 */
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{

  if(i2cHandle->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspDeInit 0 */

  /* USER CODE END I2C1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C1_CLK_DISABLE();

    /**I2C1 GPIO Configuration
    PB7     ------> I2C1_SDA
    PB8     ------> I2C1_SCL
    */
    HAL_GPIO_DeInit(SDA_GPIO_Port, SDA_Pin);

    HAL_GPIO_DeInit(SCL_GPIO_Port, SCL_Pin);

  /* USER CODE BEGIN I2C1_MspDeInit 1 */

  /* USER CODE END I2C1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

static void SDA_H(void) {HAL_GPIO_WritePin(SDA_GPIO_Port,SDA_Pin,GPIO_PIN_SET);}
static void SDA_L(void) {HAL_GPIO_WritePin(SDA_GPIO_Port,SDA_Pin,GPIO_PIN_RESET);}
static void SCL_H(void) {HAL_GPIO_WritePin(SCL_GPIO_Port,SCL_Pin,GPIO_PIN_SET);}
static void SCL_L(void) {HAL_GPIO_WritePin(SCL_GPIO_Port,SCL_Pin,GPIO_PIN_RESET);}

 /* 不用 HAL_Delay()：它是毫秒级、且依赖 SysTick 中断，抖时序太粗。
 * 100MHz 下 1us = 100 个计数，计数由 CPU 主频驱动，与中断无关。 */
static void I2C_DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000u);

    while ((DWT->CYCCNT - start) < ticks) { }
}

static void DWT_CycleCounter_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   /* 打开跟踪单元 */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;             /* 开周期计数器 */
}

void i2c_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Mode  = GPIO_MODE_OUTPUT_OD;   /* 开漏：只能主动拉低，"高"由上拉给 */
    gpio.Pull  = GPIO_PULLUP;           /* 先借内部上拉；块 1.4 再换外部 4.7k 对比 */
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = SCL_Pin;
    HAL_GPIO_Init(SCL_GPIO_Port, &gpio);

    gpio.Pin = SDA_Pin;
    HAL_GPIO_Init(SDA_GPIO_Port, &gpio);

    DWT_CycleCounter_Init();

    SCL_H();
    SDA_H();                            /* 空闲态：两条线都释放（= 高） */
}

void i2c_start(void)
{
  SDA_H();
  I2C_DelayUs(Time_us);
  SCL_H();
  I2C_DelayUs(Time_us);
  SDA_L();
  I2C_DelayUs(Time_us);
  SCL_L();
  I2C_DelayUs(Time_us);
}

void i2c_stop(void)
{
SCL_L();
SDA_L();   /* 趁 SCL 低先压下去 */
I2C_DelayUs(Time_us);
SCL_H();   /* 抬钟 */
I2C_DelayUs(Time_us);
SDA_H();   /* ★ SCL 高时释放 = STOP */
I2C_DelayUs(Time_us);
}
//发送一个字节
void i2c_write_byte(uint8_t data)
{
  uint8_t i;
  for(i=0;i<8;i++)
  {
    SCL_L();
    if(data & 0x80)   SDA_H();
    else              SDA_L();
    I2C_DelayUs(Time_us);
    SCL_H();
    I2C_DelayUs(Time_us);
    data <<= 1;
  }
  SCL_L();
  I2C_DelayUs(Time_us);
}
//判断ACK,0继续，1停止
uint8_t i2c_wait_ack(void)
{
  uint8_t ack;

  SDA_H();
  I2C_DelayUs(Time_us);
  SCL_H();
  I2C_DelayUs(Time_us);
  ack = sda_read;
  SCL_L();
  I2C_DelayUs(Time_us);
  return ack;
}

uint8_t i2c_read_byte(void)
{
  uint8_t byte = 0 ;
  SDA_H();
  I2C_DelayUs(Time_us);
  for(uint8_t i=0;i<8;i++)
  {
    SCL_H();
    I2C_DelayUs(Time_us);
    byte = (uint8_t)((byte << 1) | sda_read);
    SCL_L();
    I2C_DelayUs(Time_us);
  }
  return byte;
}

void i2c_send_ack()
{
  SCL_L();
  SDA_L();
  I2C_DelayUs(Time_us);
  SCL_H();
  I2C_DelayUs(Time_us);
  SCL_L();
  I2C_DelayUs(Time_us);
}
void i2c_send_nack()
{
  SCL_L();
  SDA_H();
  I2C_DelayUs(Time_us);
  SCL_H();
  I2C_DelayUs(Time_us);
  SCL_L();
  I2C_DelayUs(Time_us);
}
/* USER CODE END 1 */
