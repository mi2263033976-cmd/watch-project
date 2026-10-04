/**
  ******************************************************************************
  * @file    soft_i2c.h
  * @brief   软件模拟 I2C（PB7 = SDA / PB8 = SCL，开漏 + 上拉）
  *
  *          ⚠️ 本文件**不受 CubeMX 管理** —— 由 Core/Inc/i2c.h 迁出。
  *             原因：i2c.c / i2c.h 是 CubeMX 拥有的文件名，一旦在 .ioc 里
  *             把 I2C1 设为 Disable，生成器会连文件一起删掉（2026-10-03 已发生一次）。
  ******************************************************************************
  */
#ifndef __SOFT_I2C_H__
#define __SOFT_I2C_H__

#include "main.h"   /* SDA_GPIO_Port / SDA_Pin / SCL_GPIO_Port / SCL_Pin / HAL_GPIO_* */

#define Time_us 5
#define sda_read HAL_GPIO_ReadPin(SDA_GPIO_Port,SDA_Pin)

void i2c_gpio_init(void);
void i2c_start(void);
void i2c_stop(void);
void i2c_write_byte(uint8_t bit);
uint8_t i2c_wait_ack(void);
uint8_t i2c_read_byte(void);
void i2c_send_ack(void);
void i2c_send_nack(void);

#endif /* __SOFT_I2C_H__ */
