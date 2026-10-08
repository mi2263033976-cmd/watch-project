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

/* ── 时序层对外接口：上层只认这些"动作"，不碰 GPIO ───────────────────
 *   典型用法：START → 写地址 → 等 ACK → 写/读字节 → … → STOP
 */
void i2c_gpio_init(void);              /* 一次性：PB7/PB8 配开漏+上拉，总线置空闲 */
void i2c_start(void);                  /* START：SCL 高时把 SDA 由高→低          */
void i2c_stop(void);                   /* STOP ：SCL 高时把 SDA 由低→高          */
void i2c_write_byte(uint8_t data);     /* 主机发 1 字节（MSB 先出）              */
uint8_t i2c_wait_ack(void);            /* 读从机应答：0 = ACK，非 0 = NACK       */
uint8_t i2c_read_byte(void);           /* 主机读 1 字节（MSB 先入）              */
void i2c_send_ack(void);               /* 主机应答"还要"（拉低 SDA）            */
void i2c_send_nack(void);              /* 主机应答"不要了"（释放 SDA）          */

#endif /* __SOFT_I2C_H__ */
