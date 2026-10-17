/* ============================================================================
 * mpu6050.c —— MPU6050 器件层（Driver）
 *
 * 分层：main.c(app) → mpu6050_handler → 【本层】 → soft_i2c(时序)
 *
 * 职责：只管"器件协议"这一层
 *   · init：复位 → 就绪等待 → 唤醒 → 设量程 → 回读校验 → 验真身
 *   · read：一次连读 14 字节 → 拼 7 个 int16_t → 换算 → 填出参
 *
 * ⚠️ 本层【不做】的事：
 *   · 不重试（那是 handler 的可靠性策略）
 *   · 不判断"数据合不合理"（0 也可能是合法数据；那属于应用层知识）
 *
 * 错误码（`mpu6050_err_t`，定义见 .h）：
 *   OK      成功
 *   ERR_NACK 地址/数据没被 ACK（总线不通、器件不在、当前不响应）
 *   ERR_ID   读到的 WHO_AM_I 不是期望值（不是这颗器件）
 *   ERR_BUSY 复位轮询超时（器件在位，但迟迟没就绪）
 *   ERR_PARAM 调用方传了空指针
 * ==========================================================================*/

#include "mpu6050.h"
#include <stdint.h>
#include "SEGGER_RTT.h"
#include "main.h"        /* HAL_Delay（复位轮询与就绪等待要用） */
#include "soft_i2c.h"

/* ── ① 器件地址 ─────────────────────────────────────────────────── */
#define MPU6050_ADDR7              0x68   /* 7 位通信地址（AD0 接地；接高为 0x69） */

/* ── ② 寄存器地址（RM §4；十六进制升序）─────────────────────────── */
#define MPU6050_REG_GYRO_CONFIG    0x1B   /* 陀螺仪配置：FS_SEL[4:3] 选量程        */
#define MPU6050_REG_ACCEL_CONFIG   0x1C   /* 加速度计配置：AFS_SEL[4:3] 选量程     */
#define MPU6050_REG_ACCEL_XOUT_H   0x3B   /* ★ 数据包起点：从此连读 14 字节         */
#define MPU6050_REG_PWR_MGMT_1     0x6B   /* 电源管理 1：复位 / 睡眠 / 温度 / 时钟源 */
#define MPU6050_REG_SMPRT_DIV      0x19   /* 采样率分频器                         */
#define MPU6050_REG_CONFIG         0x1A   /* 滤波配置                            */
#define MPU6050_REG_WHO_AM_I       0x75   /* 只读：自报家门                        */

/* ── ③ 寄存器取值（要写进去的值 / 要对比的期望值）──────────────── */
#define MPU6050_GYRO_CONFIG_GEAR   0x18   /* FS_SEL=3 → ±2000 °/s ←配 GYRO_SENS    */
#define MPU6050_ACCEL_CONFIG_GEAR  0x00   /* AFS_SEL=0 → ±2g      ←配 ACCEL_SENS   */
#define MPU6050_SMPRT_DIV_GEAR     0x09   /* SMPRT_DIV = 9                         */
#define MPU6050_CONFIG_GEAR        0x04   /* EXT_SYNC_SET=0(不设置内部采样)，DLPF = 4 */
#define MPU6050_PWR_MGMT_1_RESET   0x80   /* 置 bit7 DEVICE_RESET：软复位器件       */
#define MPU6050_PWR_MGMT_1_WAKE    0x00   /* 清 bit6 SLEEP：唤醒（顺带开温度）      */
#define MPU6050_WHO_AM_I_VALUE     0x68   /* WHO_AM_I 期望值（与 AD0 无关）         */

/* ── ④ 时序参数 ─────────────────────────────────────────────────── */
#define MPU6050_RESET_TIMEOUT_MS   100    /* 轮询 DEVICE_RESET 清 0 的上限（每轮 1ms）*/
#define MPU6050_STARTUP_DELAY_MS   50     /* ★复位/上电后就绪等待（手册 20ms，留余量）*/

/* ── ⑤ 换算常数（★ 与 ③ 的量程宏成对：改档必须两处一起改）───────── */
#define MPU6050_ACCEL_SENS         16384.0f /* LSB/g     —— 配 AFS_SEL=0（±2g）      */
#define MPU6050_GYRO_SENS          16.4f    /* LSB/(°/s) —— 配 FS_SEL=3（±2000°/s） */
#define MPU6050_TEMP_SENS          340.0f   /* LSB/°C                                */
#define MPU6050_TEMP_OFFSET        36.53f   /* °C 偏移（= 35 + 521/340）             */

/* mpu6050_init —— 开机配置器件（复位 → 就绪等待 → 唤醒 → 设量程 → 回读 → 验真身）
 *
 *   ⚠️ 只在开机调【一次】（放 main 的 USER CODE 2 区，别放进主循环）：
 *      它内部会让器件【软复位】，每轮都调 = 每秒把器件打回睡眠态。
 *   ⚠️ 本函数内部依赖"复位后 20ms 就绪时间"，见 ② 与 ③ 之间的 HAL_Delay。
 */
mpu6050_err_t mpu6050_init(void)
{
    uint8_t  id;
    uint8_t  val = MPU6050_PWR_MGMT_1_RESET;
    uint16_t i;

    /* ① 复位：只置 DEVICE_RESET（其余位保持 0）
     *    ⚠️ 必须在所有配置之前 —— 复位会把刚设过的寄存器清回默认值 */
    if (i2c_mem_write(MPU6050_ADDR7, MPU6050_REG_PWR_MGMT_1, &val, 1) != 0)
        return MPU6050_ERR_NACK;

    /* ② 等复位完成：该位"做完自动清 0"；封顶，防"器件不在时死等" */
    for (i = 0; i < MPU6050_RESET_TIMEOUT_MS; i++)
    {
        if (i2c_mem_read(MPU6050_ADDR7, MPU6050_REG_PWR_MGMT_1, &val, 1) != 0)
            return MPU6050_ERR_NACK;
        if (0 == (val & MPU6050_PWR_MGMT_1_RESET))  break;
        HAL_Delay(1);
    }
    if (i >= MPU6050_RESET_TIMEOUT_MS)  return MPU6050_ERR_BUSY;

    /* ★ 关键：DEVICE_RESET 位清 0 ≠ 器件已就绪！
     *   清 0 只表示"复位逻辑走完"，内部时钟还没稳；此时写的配置会被全部丢弃。
     *   手册：START-UP TIME FOR REGISTER READ/WRITE = 20 ms（PS §6.3 p.14）*/
    HAL_Delay(MPU6050_STARTUP_DELAY_MS);

    /* ③ 唤醒：清 SLEEP（顺带 TEMP_DIS=0 开温度、CLKSEL 落到内部 8MHz） */
    val = MPU6050_PWR_MGMT_1_WAKE;
    if (i2c_mem_write(MPU6050_ADDR7, MPU6050_REG_PWR_MGMT_1, &val, 1) != 0)
        return MPU6050_ERR_NACK;

    /*滤波的配置：DLPF_CFG = 4，SMPRT_DIV = 9，采样率 = 100Hz */
    val = MPU6050_CONFIG_GEAR;
    if (i2c_mem_write(MPU6050_ADDR7, MPU6050_REG_CONFIG, &val, 1) != 0)
        return MPU6050_ERR_NACK;
    val = MPU6050_SMPRT_DIV_GEAR;
    if (i2c_mem_write(MPU6050_ADDR7, MPU6050_REG_SMPRT_DIV, &val, 1) != 0)
        return MPU6050_ERR_NACK;

    /* 配置陀螺仪和加速度计 */
    val = MPU6050_GYRO_CONFIG_GEAR;
    if (i2c_mem_write(MPU6050_ADDR7, MPU6050_REG_GYRO_CONFIG, &val, 1) != 0)
        return MPU6050_ERR_NACK;
    val = MPU6050_ACCEL_CONFIG_GEAR;
    if (i2c_mem_write(MPU6050_ADDR7, MPU6050_REG_ACCEL_CONFIG, &val, 1) != 0)
        return MPU6050_ERR_NACK;

    /* ── 回读校验：必须**在写之后** —— 读的是"写进去没有"，不是"原来是什么" ── */
    //加速度计和陀螺仪
    if (i2c_mem_read(MPU6050_ADDR7, MPU6050_REG_GYRO_CONFIG, &val, 1) != 0)
        return MPU6050_ERR_NACK;
    SEGGER_RTT_printf(0, "GYRO_CONFIG  = 0x%02X (wrote  0x%02X)\r\n",
                      val, MPU6050_GYRO_CONFIG_GEAR);
    if (i2c_mem_read(MPU6050_ADDR7, MPU6050_REG_ACCEL_CONFIG, &val, 1) != 0)
        return MPU6050_ERR_NACK;
    SEGGER_RTT_printf(0, "ACCEL_CONFIG = 0x%02X (wrote  0x%02X)\r\n",
                      val, MPU6050_ACCEL_CONFIG_GEAR);
    //滤波和采样
    if (i2c_mem_read(MPU6050_ADDR7, MPU6050_REG_CONFIG, &val, 1) != 0)
        return MPU6050_ERR_NACK;
    SEGGER_RTT_printf(0, "GYRO_CONFIG  = 0x%02X (wrote  0x%02X)\r\n",
                      val, MPU6050_CONFIG_GEAR);
    if (i2c_mem_read(MPU6050_ADDR7, MPU6050_REG_SMPRT_DIV, &val, 1) != 0)
        return MPU6050_ERR_NACK;
    SEGGER_RTT_printf(0, "ACCEL_CONFIG = 0x%02X (wrote  0x%02X)\r\n",
                      val, MPU6050_SMPRT_DIV_GEAR);
                 
    /* ④ 验真身 */
    if (i2c_mem_read(MPU6050_ADDR7, MPU6050_REG_WHO_AM_I, &id, 1) != 0)
        return MPU6050_ERR_NACK;

    SEGGER_RTT_printf(0, "MPU6050 ID = 0x%02X\r\n", id);

    if (MPU6050_WHO_AM_I_VALUE != id)  return MPU6050_ERR_ID;
    return MPU6050_OK;
}

/* mpu6050_read —— 读一次 6 轴 + 温度
 *   一次连读 14 字节（手册保证同一采样时刻）；出参 out 由调用者提供。
 *   ⚠️ 失败时 out 的字段**原样不动** —— 不留下"半新半旧"的包。
 */
mpu6050_err_t mpu6050_read(mpu6050_data_t *out)
{
    /* 14 字节 = 6 轴 × 2 + 温度 × 2
     *   器件侧从 0x3B 起【连号】，一次 burst read 拿回同一采样时刻的一整包
     *   （手册保证：用户可见寄存器组在总线空闲时整组锁存） */
    uint8_t buf[14];

    if (NULL == out)  return MPU6050_ERR_PARAM;

    /* 从"包的起点"开始读 —— 不是单个寄存器，是 0x3B~0x48 连读 14 个 */
    if (i2c_mem_read(MPU6050_ADDR7, MPU6050_REG_ACCEL_XOUT_H, buf, 14) != 0)
        return MPU6050_ERR_NACK;

    /* ── 拼接：两字节一组 → int16_t（高字节在前；符号位靠 (int16_t) 还原）── */
    out->raw.ax = (int16_t)((uint16_t)buf[0]  << 8 | buf[1]);
    out->raw.ay = (int16_t)((uint16_t)buf[2]  << 8 | buf[3]);
    out->raw.az = (int16_t)((uint16_t)buf[4]  << 8 | buf[5]);

    out->raw.temp = (int16_t)((uint16_t)buf[6]  << 8 | buf[7]);   /* ★ 温度在第 4 个 */

    out->raw.gx = (int16_t)((uint16_t)buf[8]  << 8 | buf[9]);
    out->raw.gy = (int16_t)((uint16_t)buf[10] << 8 | buf[11]);
    out->raw.gz = (int16_t)((uint16_t)buf[12] << 8 | buf[13]);

    /* ── 换算：raw ÷ 灵敏度（一律单精度；f 后缀不可省）─────────────── */
    out->conv.ax_g = (float)out->raw.ax / MPU6050_ACCEL_SENS;
    out->conv.ay_g = (float)out->raw.ay / MPU6050_ACCEL_SENS;
    out->conv.az_g = (float)out->raw.az / MPU6050_ACCEL_SENS;

    out->conv.temp_c = (float)out->raw.temp / MPU6050_TEMP_SENS + MPU6050_TEMP_OFFSET;

    out->conv.gx_dps = (float)out->raw.gx / MPU6050_GYRO_SENS;
    out->conv.gy_dps = (float)out->raw.gy / MPU6050_GYRO_SENS;
    out->conv.gz_dps = (float)out->raw.gz / MPU6050_GYRO_SENS;

    return MPU6050_OK;
}
