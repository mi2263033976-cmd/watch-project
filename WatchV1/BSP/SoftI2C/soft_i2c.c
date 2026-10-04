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
