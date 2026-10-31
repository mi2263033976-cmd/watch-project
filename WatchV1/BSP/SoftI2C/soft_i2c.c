/**
  ******************************************************************************
  * @file    soft_i2c.c
  * @brief   软件模拟 I2C 时序层（PB7 = SDA / PB8 = SCL）
  *
  *          ⚠️ 本文件**不受 CubeMX 管理** —— 由 Core/Src/i2c.c 的 USER CODE 区迁出。
  *             迁出原因见 soft_i2c.h 头部说明。
  ******************************************************************************
  */
#include "soft_i2c.h"
#include <stdint.h>

/* ── 位操作助手 ──────────────────────────────────────────────────────
 *   开漏输出下：「写 1」= 释放引脚（高由上拉电阻给），「写 0」= 主动拉低
 */
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

/* 一次性初始化：PB7/PB8 配成开漏 + 上拉，启动 DWT 计数器，总线置空闲（两线都释放） */
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

/* START：SCL 保持高时，SDA 由高→低跳变（只有"SCL 高期间的跳变"才算事件） */
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

/* STOP：SCL 保持高时，SDA 由低→高跳变（之前先趁 SCL 低把 SDA 压好） */
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
/* 主机发一个字节：MSB 先出；SCL 低时换数据，SCL 高时从机采样 */
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
/* 读从机应答：主机先释放 SDA（开漏写 1），再抬 SCL 采样 → 0 = ACK，非 0 = NACK */
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

/* 主机读一个字节：MSB 先入；读完停在 SCL 低，ACK/NACK 由调用者发 */
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

/* ACK：拉低 SDA = 告诉从机"继续发" */
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
/* NACK：释放 SDA = 告诉从机"停"，读最后一个字节时用 */
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

/* 私有助手：发一字节并查应答。
 * ⚠️ 契约：NACK 时【本函数内部已经 i2c_stop()】，调用方不要再补 stop。
 *    好处 = 失败必定释放总线；代价 = 调用点看不到这个副作用，改前先读这里。 */
static uint8_t i2c_mem_checked(uint8_t byte)
{
  i2c_write_byte(byte);
  if(0 != i2c_wait_ack())
	{
	  i2c_stop();
	  return 1;
	}
  	else
	{
		return 0;
	}
}

uint8_t i2c_mem_write(uint8_t dev7, uint8_t reg, const uint8_t *buf, uint8_t len)
{  
  if (len == 0) return 1;            //防止len=0;

  i2c_start();
  if (i2c_mem_checked(dev7 << 1 | 0) != 0) return 1;
  if (i2c_mem_checked(reg) != 0) return 1;


  for (uint8_t i = 0 ; i < len; i++)
  {
    if (i2c_mem_checked(buf[i]) != 0) return 1;
  }
  i2c_stop();
  return 0;

}
uint8_t i2c_mem_read (uint8_t dev7, uint8_t reg, uint8_t *buf, uint8_t len)
{
  if (len == 0) return 1;            //防止len=0;

  i2c_start();
  if (i2c_mem_checked(dev7 << 1 | 0) != 0) return 1;
  if (i2c_mem_checked(reg) != 0) return 1;

  i2c_start();
  if (i2c_mem_checked(dev7 << 1 | 1) != 0) return 1;

  for (uint8_t i = 0; i < len; i++)
	{
	  buf[i] = i2c_read_byte();
	  if (i < len - 1)  i2c_send_ack();     /* 还有 → 主机回 ACK */
	  else          i2c_send_nack();    /* 最后一个 → 主机喊停 */
	}
	i2c_stop();
  return 0;

}

/* 命令式器件：START → 读地址 → 连读 len 字节（末字节 NACK）→ STOP。
 * 失败时 STOP 已由 i2c_mem_checked 发出；返回值 0=成功。 */
uint8_t i2c_read_bytes(uint8_t dev7, uint8_t *buf, uint8_t len)  /* 0=成功 */
{
  if(len == 0) return 1;

  i2c_start();
  if (i2c_mem_checked(dev7 << 1 | 1) != 0) return 1;

  for (uint8_t i = 0; i < len; i++)
	{
	  buf[i] = i2c_read_byte();
	  if (i < len - 1)  i2c_send_ack();     /* 还有 → 主机回 ACK */
	  else          i2c_send_nack();    /* 最后一个 → 主机喊停 */
	}
	i2c_stop();
  return 0;

}

/* 探活：START → 设备地址 → 查 ACK。0=在线。
 * 无应答时 STOP 已由 i2c_mem_checked 发出（见上面的契约）。 */
uint8_t i2c_ping(uint8_t dev7)
{
  i2c_start();
  if (i2c_mem_checked(dev7 << 1 | 0) != 0) return 1;
  i2c_stop();
  return 0;
}

/* 普查：把 I2C_ADDR7_SCAN_MIN~I2C_ADDR7_SCAN_MAX 里所有有应答的地址写进 found[]，返回个数。
 * 不扫 0x00（通用广播）和 0x78~0x7F（10 位地址前缀）；边界见 soft_i2c.h。 */
uint8_t i2c_scan(uint8_t *found, uint8_t max)
{
  uint8_t num = 0;
  for(uint8_t addr = I2C_ADDR7_SCAN_MIN; addr <= I2C_ADDR7_SCAN_MAX; addr++)
  {
    if(i2c_ping(addr) == 0)
    {
      if (num < max) found[num] = addr;   /* 缓冲满了也继续数，便于发现"挂太多" */
      num++;
    }
  }
  return num;
}
