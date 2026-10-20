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

/* 7 位地址里能自由分给从机的一段（0x00 = 广播、0x78~0x7F = 10 位前缀，都不能扫）。
 * 扫描循环的边界和日志文案都从这里取，别各写一份。 */
#define I2C_ADDR7_SCAN_MIN 0x08
#define I2C_ADDR7_SCAN_MAX 0x77

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
/* ── 器件级接口（本层补的原语，失败时总线均已释放）────────────────────
 *   i2c_mem_* = 寄存器式器件（EEPROM / MPU6050）：先发 reg，再读写数据
 *   i2c_read_bytes = 命令式器件（AHT21 之类）：无 reg，直接读 N 字节
 *   i2c_ping  = 只探活：发设备地址看有没有应答
 */
uint8_t i2c_mem_read (uint8_t dev7, uint8_t reg, uint8_t       *buf, uint8_t len);
uint8_t i2c_mem_write(uint8_t dev7, uint8_t reg, const uint8_t *buf, uint8_t len);

uint8_t i2c_read_bytes(uint8_t dev7, uint8_t *buf, uint8_t len);  /* 命令式读，末字节自动 NACK；0=成功 */
uint8_t i2c_ping(uint8_t dev7);                                   /* 探活：0=在线，非 0=无应答 */
uint8_t i2c_scan(uint8_t *found, uint8_t max);   /* 普查 0x08~0x77；返回找到的个数 */
#endif /* __SOFT_I2C_H__ */
