/* ============================================================================
 * test_aht21_handler.c —— aht21_handler 的 SWUT 单元测试（PC 上跑，不碰板子）
 *
 * 靠 aht21_stub.c 顶替真驱动 —— aht21_handler.c 一个字都不用改。
 *
 * 编译运行（在 test/ 目录下）：
 *     gcc -I.. test_aht21_handler.c aht21_stub.c ../aht21_handler.c -o test.exe
 *     ./test.exe
 * ==========================================================================*/

#include <stdio.h>
#include <stdint.h>
#include "aht21.h"            /* 桩的遥控杆用到 aht21_err_t / AHT21_OK */
#include "aht21_handler.h"

/* ── 桩的「遥控杆」：定义在 aht21_stub.c 里，这里借过来用 ──────────────
 *    （正式项目里会给桩配一个 aht21_stub.h 来声明它们，这里先从简）
 * -------------------------------------------------------------------------*/
extern aht21_err_t stub_read_err;
extern uint32_t    stub_fail_times;
extern float       stub_temp;
extern float       stub_humi;

/* ── 极简测试设施：不引框架，两个宏就够 ──────────────────────────────
 *    为什么不用 assert()？—— assert 一失败就把程序打停，
 *    后面的用例全不跑了。这里要「失败也继续，最后统一算总账」。
 * -------------------------------------------------------------------------*/
static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg)                                         \
    do {                                                         \
        if (cond) { g_pass++; printf("  [PASS] %s\n", (msg)); }   \
        else      { g_fail++; printf("  [FAIL] %s\n", (msg)); }   \
    } while (0)

/* 每条用例开跑前「复位」遥控杆 —— 用例之间必须互不影响 */
static void stub_reset(void)
{
    stub_read_err   = AHT21_OK;
    stub_fail_times = 0;
    stub_temp       = 25.6f;
    stub_humi       = 60.0f;
}

/* =========================================================================
 * 用例 · 连续失败 → OFFLINE
 *
 *   设定：桩一直报 NACK（喂足够多次，保证 handler 内部重试也拿不到 OK）
 *   动作：连调 handler 5 次
 *   期望：第 5 次之后，状态 = AHT21_H_OFFLINE
 * =======================================================================*/

static void test_case_1_offline_after_failures(void)
{
    float t = 0.0f, h = 0.0f;
    aht21_handler_status_t st = AHT21_H_OK;
    int i;

    stub_reset();                       /* 复位 */
    stub_read_err   = AHT21_ERR_NACK;   /* 设定：报 NACK */
    stub_fail_times = 100;              /* 设定：一直报，别中途"恢复" */

    for (i = 0; i < AHT21_H_OFFLINE_THRESHOLD; i++) /* 动作 */
    {
        st = aht21_handler_read(&t, &h);
    }

    CHECK(st == AHT21_H_OFFLINE,        /* 期望 */
          "连续 5 次 NACK 后 → 状态应为 OFFLINE");
}

/* ── 把 handler 拉回「已知状态」────────────────────────────────────────
 *   handler 没有 reset 接口 —— 唯一能复位它的动作，就是【喂它一次成功】
 *   （成功会清计数、建缓存）。每个用例开头都先来一次，起点才一致。
 * -------------------------------------------------------------------------*/
static void handler_prime(void)
{
    float t = 0.0f, h = 0.0f;

    stub_reset();          /* 桩侧：恢复成"一直成功" */

    aht21_handler_read(&t, &h); 
}

/* =========================================================================
 * 用例 · 一直读到 → 一直 H_OK
 *
 *   设定：桩一直成功（25.6 / 60.0）—— 复位之后本来就是
 *   期望：每次都是 H_OK，且数据就是桩给的那份
 * =======================================================================*/
static void test_case_always_ok(void)
{
    float t = 0.0f, h = 0.0f;
    aht21_handler_status_t st = AHT21_H_OK;
    int i;
    int bad = 0;
    handler_prime();

    for(i = 0; i < 3; i++)
    {
        st = aht21_handler_read(&t, &h);
        if (st != AHT21_H_OK)
        {bad ++;}
    }
    CHECK(bad == 0, "连读 3 次都应为 OK");
    CHECK(t == 25.6f && h == 60.0f, "数据应是桩给的 25.6 / 60.0");

}

/* =========================================================================
 * 用例 · 先建立缓存 → 再短暂失败 → STALE，且给出【上次的】数据
 * =======================================================================*/
static void test_case_stale_keeps_cached_data(void)
{
    float t = 0.0f, h = 0.0f;
    aht21_handler_status_t st = AHT21_H_OK;
    int i;

    handler_prime();       /* ← 这一步就已经建好缓存了（它内部成功过一次） */

    stub_read_err   = AHT21_ERR_NACK;   
    stub_fail_times = 100;              
    for(i = 0; i < 3; i++)
    {
        st = aht21_handler_read(&t, &h);
    }
    CHECK(st == AHT21_H_STALE, "失败 3 次 → 应为 STALE");
    CHECK(t == 25.6f && h == 60.0f, "STALE 时给出的应是上次那份数据(25.6 / 60.0)");
}

/* =========================================================================
 * 用例 · OFFLINE → 器件恢复 → 回到 OK → 再失败 → STALE
 *
 *   这两步天生连着（中间不能停），所以写在同一个函数里。
 *   ⑤ 的意义：证明 g_fail_count 真的被清零了 —— 否则一失败就该立刻 OFFLINE。
 * =======================================================================*/
static void test_case_recovery_and_reset(void)
{
    float t = 0.0f, h = 0.0f;
    aht21_handler_status_t st = AHT21_H_OK;
    int i;

    handler_prime();

    stub_read_err   = AHT21_ERR_NACK;   
    stub_fail_times = 100;      

    for(i = 0; i < AHT21_H_OFFLINE_THRESHOLD; i++)
    {
        st = aht21_handler_read(&t, &h);
    }

    stub_fail_times = 0;
    st = aht21_handler_read(&t, &h);
    CHECK(st == AHT21_H_OK, "器件恢复 → 状态应回到 OK");

    stub_read_err   = AHT21_ERR_NACK;   
    stub_fail_times = 100;      
    for(i = 0; i < 3; i++)
    {
        st = aht21_handler_read(&t, &h);
    }
    CHECK(st == AHT21_H_STALE, "计数已清零：恢复后再失败 3 次 → 只该是 STALE");
}

/* =========================================================================*/

int main(void)
{
    printf("=== aht21_handler SWUT ===\n");

    test_case_always_ok();                 /* ① 一直成功 */
    test_case_stale_keeps_cached_data();   /* ② STALE + 缓存 */
    test_case_1_offline_after_failures();  /* ③ 连续失败 → OFFLINE（已写好）*/
    test_case_recovery_and_reset();        /* ④⑤ 恢复 → OK，计数清零 */

    printf("\n%d passed / %d failed\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
