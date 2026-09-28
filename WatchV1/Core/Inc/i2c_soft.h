#ifndef __I2C_SOFT_H__
#define __I2C_SOFT_H__

#include "stm32f4xx_hal.h"

/* ===== 块 0.1：引脚 =====
 * 直接沿用 WatchV1 里 I2C1 的两个脚（PB7=SDA / PB8=SCL），接线不用动，
 * 只是把它们的角色从"硬件 I2C 的 AF 引脚"改成"普通开漏 GPIO"。
 * 换脚只改下面 4 个宏即可。 */
#define I2C_SOFT_SDA_PORT   GPIOB
#define I2C_SOFT_SDA_PIN    GPIO_PIN_7
#define I2C_SOFT_SCL_PORT   GPIOB
#define I2C_SOFT_SCL_PIN    GPIO_PIN_8

/* 半周期：50us 高 + 50us 低 = 100us -> 10kHz */
#define I2C_SOFT_HALF_US    50u

/* ---- 底层：只有电平动作，不含任何协议 ---- */
void I2C_Soft_Init(void);
void I2C_Soft_DelayUs(uint32_t us);

void SCL_H(void);
void SCL_L(void);
void SDA_H(void);
void SDA_L(void);

/* ---- 块 1 的两个实验：在 main 的 while(1) 里调其中一个 ---- */
void TaskA_SCL_SquareWave(void);
void TaskB_SDA_TogglePer8Pulse(void);

#endif /* __I2C_SOFT_H__ */
