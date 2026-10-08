/* ============================================================================
 * aht21_handler.c —— AHT21 的 Handler 层（缓存 / 状态）
 *
 * 职责：把 driver 的「这一次通不通」翻译成 app 要的「这份数据能不能用」
 *   · 缓存：上次成功的数据
 *   · 状态：连续失败计数  →  H_OK / H_STALE / H_OFFLINE
 *
 * 本文件不碰 I2C、也不依赖 HAL —— 否则 PC 上跑不了单测（SWUT 要的就是这个）。
 * ==========================================================================*/

#include "aht21_handler.h"
#include "aht21.h"
#include <stdint.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * 【私有数据】—— 只住在这里，.h 里一个字都不出现
 *
 * TODO: 想想要几个变量？
 *   · 上次成功的温度 / 湿度                     （缓存）
 *   · 连续失败了几次                            （状态的账本）
 *   · 「到底成功过没有」—— 这个信息能从上面推出来吗？
 *     还是要单独记一个 flag？（提示：看下面的规则表，第 3 行靠它区分）
 * -------------------------------------------------------------------------*/

static float   g_last_temp  = 0.0f;   /* 缓存：上次成功的温度 */
static float   g_last_humi  = 0.0f;   /* 缓存：上次成功的湿度 */
static uint8_t g_fail_count = 0;      /* 账本：连续失败了几次 */
static uint8_t g_has_data   = 0;      /* 0 = 从没成功过；1 = 手里有可用旧数据 */


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

aht21_handler_status_t aht21_handler_read(float *temp, float *humi)
{
    /* TODO:
     *   ① 参数检查：temp / humi 是 NULL 怎么办？（driver 层有 AHT21_ERR_PARAM 的先例）
     *   ② 调 driver：aht21_read_temp_humi(...)
     *   ③ 按规则表决定「返回什么状态」＋「要不要写出参」
     */
    aht21_err_t err;
    float t = 0.0f,h = 0.0f;    /* ← 先落"临时车位"，别直接往缓存里写 */
    
    /* ① 参数检查：调用方传空指针 → 直接打回，一步都不往下走 */
    if (temp == NULL || humi == NULL)
    {
        return AHT21_H_PARAM;
    }

    /* ② 调 driver —— 结果先落临时车位 */
    err = aht21_read_temp_humi(&t, &h);

    /* ③ 成功：刷新缓存 → 给出参 → 报 OK */
    if (AHT21_OK == err)
    {
        g_last_temp  = t;      /* 缓存"转正" */
        g_last_humi  = h;
        g_has_data   = 1;      /* 从这一刻起，手里有数据了 */
        g_fail_count = 0;      /* ← 这一行就是"连续"二字的全部实现 */

        *temp = t;             /* 新数据交给 app */
        *humi = h;
        return AHT21_H_OK;
    }

    /* ④ 失败分支 —— 第 4 步填 */
    if (g_fail_count < AHT21_H_OFFLINE_THRESHOLD)   /* 到阈值就封顶：再涨没有意义 */
    {
        g_fail_count++;
    }

    if (0 == g_has_data)    /* 压根没有旧数据 */
    {
        return AHT21_H_OFFLINE;
    }
    
    if (g_fail_count >= AHT21_H_OFFLINE_THRESHOLD)   /* 有旧数据,但它也熬到头了*/
    {
        g_has_data   = 0;
        return AHT21_H_OFFLINE;
    }
    /* 剩下：有旧数据、失败没到阈值 → 给旧的 */
    *temp = g_last_temp;
    *humi = g_last_humi;
    return AHT21_H_STALE;
}
