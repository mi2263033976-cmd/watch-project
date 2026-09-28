#include "i2c_soft.h"

/* =====================================================================
 * 块 1 · GPIO 抖位（节 26 第 1 层）
 * 本文件【只有电平动作，没有协议】：START / STOP / 字节 / ACK 全是块 2 的事。
 *
 * ⚠️ 与 CubeMX 的关系（块 0.2）：
 *    MX_I2C1_Init() -> HAL_I2C_MspInit() 已把 PB7/PB8 配成 AF4(I2C1)。
 *    本文件把它们改回"开漏 GPIO"，所以调用顺序必须让 I2C_Soft_Init() 最后生效：
 *        MX_GPIO_Init(); ...; MX_I2C1_Init();  I2C_Soft_Init();   <- 放在最后
 *    更干净：CubeMX 里把 I2C1 禁用后重新生成（W2 建议这么干）。
 * ===================================================================== */

/* ---------- 块 0.4：微秒延时（DWT CYCCNT）----------
 * 不用 HAL_Delay()：它是毫秒级、且依赖 SysTick 中断，抖时序太粗。
 * 100MHz 下 1us = 100 个计数，计数由 CPU 主频驱动，与中断无关。 */
void I2C_Soft_DelayUs(uint32_t us)
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

/* ---------- 电平动作 ----------
 * 开漏输出下："写 1" = 释放（高阻，由外部/内部上拉拉高）；"写 0" = 主动拉低。
 * 所以 I2C 里 0 是主动的，1 是被动的。 */
void SCL_H(void) { HAL_GPIO_WritePin(I2C_SOFT_SCL_PORT, I2C_SOFT_SCL_PIN, GPIO_PIN_SET);   }
void SCL_L(void) { HAL_GPIO_WritePin(I2C_SOFT_SCL_PORT, I2C_SOFT_SCL_PIN, GPIO_PIN_RESET); }
void SDA_H(void) { HAL_GPIO_WritePin(I2C_SOFT_SDA_PORT, I2C_SOFT_SDA_PIN, GPIO_PIN_SET);   }
void SDA_L(void) { HAL_GPIO_WritePin(I2C_SOFT_SDA_PORT, I2C_SOFT_SDA_PIN, GPIO_PIN_RESET); }

void I2C_Soft_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Mode  = GPIO_MODE_OUTPUT_OD;   /* 开漏：只能主动拉低，"高"由上拉给 */
    gpio.Pull  = GPIO_PULLUP;           /* 先借内部上拉；块 1.4 再换外部 4.7k 对比 */
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = I2C_SOFT_SCL_PIN;
    HAL_GPIO_Init(I2C_SOFT_SCL_PORT, &gpio);

    gpio.Pin = I2C_SOFT_SDA_PIN;
    HAL_GPIO_Init(I2C_SOFT_SDA_PORT, &gpio);

    DWT_CycleCounter_Init();

    SCL_H();
    SDA_H();                            /* 空闲态：两条线都释放（= 高） */
}

/* ================= 任务 A：SCL 出 10kHz 方波 =================
 * 验收：逻辑分析仪量周期 ≈100us；顺便看上升沿是不是一条斜线。 */
void TaskA_SCL_SquareWave(void)
{
    for (;;)
    {
        SCL_H();
        I2C_Soft_DelayUs(I2C_SOFT_HALF_US);   /* 第 1 拍：高 —— 数据有效期 */
        SCL_L();
        I2C_Soft_DelayUs(I2C_SOFT_HALF_US);   /* 第 2 拍：低 —— 换数据窗口 */
    }
}

/* ================= 任务 B：每 8 个 SCL 拍翻一次 SDA =================
 * 验收：波形上"一个 SDA 电平 = 8 个 SCL 周期"，且 SDA 的每次跳变
 *       都发生在 SCL 低电平期间（铁律：要变，趁 SCL 低）。 */
void TaskB_SDA_TogglePer8Pulse(void)
{
    uint8_t pulseCnt = 0;
    uint8_t sdaLevel = 0;

    for (;;)
    {
        /* 第 1 拍：SCL 低 —— 唯一的"换数据窗口" */
        SCL_L();
        if (sdaLevel) { SDA_H(); } else { SDA_L(); }
        I2C_Soft_DelayUs(I2C_SOFT_HALF_US);

        /* 第 2 拍：SCL 高 —— 数据有效，SDA 不准动 */
        SCL_H();
        I2C_Soft_DelayUs(I2C_SOFT_HALF_US);

        /* 满 8 拍 = 一位（一个字节），换值 */
        if (++pulseCnt >= 8u)
        {
            pulseCnt = 0u;
            sdaLevel = (uint8_t)(!sdaLevel);
        }
    }
}
