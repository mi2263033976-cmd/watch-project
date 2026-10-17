/* ============================================================================
 * mpu6050_handler.c —— MPU6050 的 Handler 层（缓存 / 状态）
 * 参考形状：BSP/Sensor_Humidity_Temp/aht21_handler.c
 *
 * 职责：把 driver 的「这一次通不通」翻译成 app 要的「这份数据能不能用」
 *   · 缓存：上次成功的【一整包】6 轴 + 温度
 *   · 状态：连续失败计数  →  H_OK / H_STALE / H_OFFLINE
 *
 * 本文件不碰 I2C、也不依赖 HAL —— 否则 PC 上跑不了单测（SWUT 要的就是这个）。
 * ==========================================================================*/

#include "mpu6050_handler.h"
#include "mpu6050.h"
#include <stdint.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * 【私有数据】—— 只住在这里，.h 里一个字都不出现
 *   ⚠️ 缓存只需【一个】变量：mpu6050_data_t 本身已把 raw 和 conv 包在一起，
 *      再拆开 = 给"同时产生、同时失效"的东西添上"赋值漏一半"的机会。
 * -------------------------------------------------------------------------*/
static mpu6050_data_t   g_last       = {0};   /* 缓存：上次成功的那一整包 */
static uint8_t          g_fail_count = 0;     /* 账本：连续失败了几次 */
static uint8_t          g_has_data   = 0;     /* 0 = 从没成功过；1 = 手里有可用旧数据 */

/* ── 【依赖注入】"怎么等"不归 handler 管 —— 由调用方接进来 ────────────
 *   默认空实现：没注入时"等 0 毫秒"（等价于不延时）。
 *   ⇒ 这样 handler 依然【零 HAL 依赖】，PC 单测照跑。 */
static void delay_noop(uint32_t ms) { (void)ms; }
static void (*g_delay_ms)(uint32_t ms) = delay_noop;
/*中断数据*/
static volatile uint8_t g_new_data = 0 ;

void mpu6050_handler_set_delay(void (*fn)(uint32_t ms))
{
    g_delay_ms = (fn != NULL) ? fn : delay_noop;
}

/* ---------------------------------------------------------------------------
 * 【规则表】—— 这就是函数体里那串 if/else 的出处
 *
 *   读到了                        → H_OK      （刷新缓存、失败计数清零）
 *   没读到 + 有旧数据 + 失败 < 5   → H_STALE   （给上次的数据）
 *   没读到 + 没旧数据（从没成功过）→ H_OFFLINE （无旧可给）
 *   没读到 + 失败 ≥ 5              → H_OFFLINE （旧数据也撤）
 *
 *   ⚠️ 最后两行返回一样、理由不同 —— 但它们可以是两个出口。
 * -------------------------------------------------------------------------*/

mpu6050_handler_status_t mpu6050_handler_read(mpu6050_data_t *out)
{
    mpu6050_err_t err;
    mpu6050_data_t tmp = {0};    /* ← 先落"临时车位"，别直接往缓存里写 */

    /* ① 参数检查：调用方传空指针 → 直接打回，一步都不往下走
     *    （这是"调用"的问题，不是器件的问题 ⇒ 不计入失败账本） */
    if (out == NULL)
    {
        return MPU6050_H_PARAM;
    }

    /* ② 调 driver —— 带重试；结果先落"临时车位"，别直接往缓存里写
     *
     *   重试策略：一次调用内最多【尝试】MPU6050_H_MAX_TRIES 次。
     *   ⚠️ 只重试"再试一次可能改变结果"的错：
     *        NACK —— 最常见成因是"这一刻从机没准备好/总线没回到空闲"，等一等再来可能就成
     *        BUSY —— 时序擦边，再来一次可能就过（注：mpu6050_read 不返回它，init 才返回）
     *      而 ID / PARAM 属于"再试一万次也一样"（世界不会变）⇒ 立刻跳出，别浪费时间。
     *
     *   ⚠️ 末次尝试之后不再延时 —— 反正不会再有下一次，等它没有意义。
     */
    for (uint8_t try = 0; try < MPU6050_H_MAX_TRIES; try++)
    {
        err = mpu6050_read(&tmp);

        /* 成功，或这个错"重试也不会变" → 结束 */
        if ((MPU6050_OK == err) || (MPU6050_ERR_ID == err) || (MPU6050_ERR_PARAM == err))
        {
            break;
        }
        /* 走到这里只剩"值得重试"的错 —— 所以这个守卫当前【恒真】。
         *   留着它，是为了逼住将来新增的错误码：
         *     落进这一支 ⇒ 会空转重试；落进上面的 break ⇒ 会立刻放弃。
         *   两种都得先想清楚，而不是让它悄悄掉进默认行为。 */
        else if ((MPU6050_ERR_NACK == err) || (MPU6050_ERR_BUSY == err))
        {
            if (try < (MPU6050_H_MAX_TRIES - 1))   /* 还有下次，才等 */
            {
                g_delay_ms(MPU6050_H_RETRY_DELAY_MS);
            }
        }
    }

    /* ③ 成功：刷新缓存 → 给出参 → 报 OK */
    if (MPU6050_OK == err)
    {
        g_last = tmp;          /* 缓存"转正"（整包赋值，一行顶十四行） */
        g_has_data   = 1;      /* 从这一刻起，手里有数据了 */
        g_fail_count = 0;      /* ← 这一行就是"连续"二字的全部实现 */

        *out = tmp;            /* 新数据交给 app（同样整包） */
        return MPU6050_H_OK;
    }

    /* ④ 失败分支 —— 先记账，再决定"给旧的"还是"干脆不给" */
    if (g_fail_count < MPU6050_H_OFFLINE_THRESHOLD)   /* 到阈值就封顶：再涨没有意义 */
    {
        g_fail_count++;
    }

    if (0 == g_has_data)    /* 压根没有旧数据 */
    {
        return MPU6050_H_OFFLINE;
    }

    if (g_fail_count >= MPU6050_H_OFFLINE_THRESHOLD)   /* 有旧数据，但它也熬到头了 */
    {
        g_has_data = 0;      /* ← 撤旧数据：下次失败就走上面那条 OFFLINE */
        return MPU6050_H_OFFLINE;
    }

    /* 剩下：有旧数据、失败没到阈值 → 给旧的，并如实标注"可能过期"
     *   ⚠️ 只有走到这里才碰 *out —— 前面几条失败出口一个字段都不写 */
    *out = g_last;

    return MPU6050_H_STALE;
}

void mpu6050_handler_notify(void)
{
    g_new_data = 1;
}

/* read_if_new —— "有新数据才读"（中断驱动的取货入口）
 *
 *   分工：notify() 只置标志（ISR 调）；本函数负责取货 + 清标志（主循环调）。
 *
 *   ⚠️ 为什么【无论成败都清】标志：
 *     · 中断驱动下，"下一次机会" = 下一次中断（100Hz ⇒ 10ms 后），
 *       而不是"下一圈主循环"（微秒级）—— 在主循环里立刻重试 = 忙等
 *     · 失败还把标志留着 ⇒ 主循环空转狂调 read（每次自带 3 次重试 + 1ms 延时）
 *       ⇒ CPU 被吃干、日志被刷爆
 *     · 丢一份数据不要紧：下一份 10ms 就到
 *
 *   ⚠️ 已知代价（尚未补）：器件被拔掉后中断不再来，标志停在 0，
 *      本函数只会返回 NO_DATA ⇒ app 看不到 OFFLINE。
 *      要判"失联"得另加超时（W6 记的账）。
 */
mpu6050_handler_status_t mpu6050_handler_read_if_new(mpu6050_data_t *out)
{
    mpu6050_handler_status_t st;

    if (0 == g_new_data)              /* 没新数据：一步都不往下走 */
    {
        return MPU6050_H_NO_DATA;
    }

    st = mpu6050_handler_read(out);   /* 老函数一个字不动 */

    g_new_data = 0;

    return st;
}
