/* mpu6050_handler.h —— MPU6050 的 Handler 层对外契约
 * 分层：main.c(app) → 【本层】 → mpu6050.c(器件) → soft_i2c(时序)
 * 2026-10-11 建（W5）
 *
 * 职责：把 driver 的「这一次通不通」翻译成 app 要的「这份数据能不能用」
 */
#ifndef __MPU6050_HANDLER_H__
#define __MPU6050_HANDLER_H__

#include "mpu6050.h"   /* 本层出参用 mpu6050_data_t，必须拿到它的完整定义 */

/* ── ① 可靠性策略：handler 自己的旋钮（与器件无关）────────────────────
 *    放 .h 而不埋进 .c，是因为它们是"这一层对外可调的参数"，不是实现细节。 */
#define MPU6050_H_OFFLINE_THRESHOLD   5   /* 连续失败到几次算「失联」
                                             —— 【你对可靠性的要求】，与器件无关
                                                （同板两器件该一致 ⇒ 与 AHT21 同值）*/
#define MPU6050_H_MAX_TRIES           3   /* 一次调用内最多【尝试】几次（含首次）*/
#define MPU6050_H_RETRY_DELAY_MS      1   /* 两次尝试之间等多久（毫秒）          */


/* ② 状态类型 —— 回答"这段时间靠不靠得住"
 *    （driver 的 mpu6050_err_t 说的是"这一次通不通"—— 两者不是一个东西）
 *
 *      OK      ：在线，这次读到了   → 数据新鲜，能用
 *      STALE   ：在线，这次没读到   → 给上次缓存的，标"可能过期"
 *      OFFLINE ：连续失败到阈值     → 旧数据也撤，明确"读不到"
 *      PARAM   ：调用方传了空指针   → 不是器件的问题，是【调用】的问题
 *
 *    ⚠️ 四态全是"数据的可用性"，没有一个是 MPU6050 特有的 ⇒ 整套可复用 */
typedef enum {
    MPU6050_H_OK = 0,
    MPU6050_H_STALE,
    MPU6050_H_OFFLINE,
    MPU6050_H_PARAM
} mpu6050_handler_status_t;

/* ② 对外函数 —— app 一次调用拿到「一整包 6 轴 + 温度」＋状态
 *
 *    出参用 struct 而不是多个标量：7 个值是【同一时刻、同一个姿态】的完整
 *    描述，拆开单独用没有意义（只有 ax 没有 gz，那不是一个姿态）。
 *    —— 判据是"这些值是不是一个不可分的整体"，不是"值有多少个"。
 */
mpu6050_handler_status_t mpu6050_handler_read(mpu6050_data_t *out);

/* 注入"毫秒延时"的实现 —— handler 自己不依赖 HAL，由调用方提供
 *   不注入 = 默认空实现（不延时）*/
void mpu6050_handler_set_delay(void (*fn)(uint32_t ms));


#endif /* __MPU6050_HANDLER_H__ */
