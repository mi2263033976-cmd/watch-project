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
 * 用例 ①：连续失败 → OFFLINE
 *
 *   设定：桩一直报 NACK（喂足够多次，保证 handler 内部重试也拿不到 OK）
 *   动作：连调 handler 5 次
 *   期望：第 5 次之后，状态 = AHT21_H_OFFLINE
 * =======================================================================*/

#define OFFLINE_AFTER 5   /* ⚠️ 这个 5 属于 handler 的规则，将来该由 aht21_handler.h 暴露 */

static void test_case_1_offline_after_failures(void)
{
    float t = 0.0f, h = 0.0f;
    aht21_handler_status_t st = AHT21_H_OK;
    int i;

    stub_reset();                       /* 复位 */
    stub_read_err   = AHT21_ERR_NACK;   /* 设定：报 NACK */
    stub_fail_times = 100;              /* 设定：一直报，别中途"恢复" */

    for (i = 0; i < OFFLINE_AFTER; i++) /* 动作 */
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
    /* TODO: handler 侧 —— 调一次，让它回到"在线 + 手里有数据" */
}

/* =========================================================================
 * 用例 ②：一直读到 → 一直 H_OK
 *
 *   设定：桩一直成功（25.6 / 60.0）—— 复位之后本来就是
 *   期望：每次都是 H_OK，且数据就是桩给的那份
 * =======================================================================*/
static void test_case_always_ok(void)
{
    float t = 0.0f, h = 0.0f;
    aht21_handler_status_t st = AHT21_H_OK;
    int i;

    handler_prime();

    /* TODO ①：还需要动哪根遥控杆吗？（复位后就是一直成功）
     * TODO ②：连读 3 次 —— 只 CHECK 最后一次够不够？怎么写才不留死角？
     * TODO ③：顺手验数据：t / h 应等于桩给的 25.6 / 60.0
     */
}

/* =========================================================================
 * 用例 ③：先建立缓存 → 再短暂失败 → STALE，且给出【上次的】数据
 * =======================================================================*/
static void test_case_stale_keeps_cached_data(void)
{
    float t = 0.0f, h = 0.0f;
    aht21_handler_status_t st = AHT21_H_OK;
    int i;

    handler_prime();       /* ← 这一步就已经建好缓存了（它内部成功过一次） */

    /* TODO ①：把桩掰成【失败】：动哪两根遥控杆？喂几次？
     * TODO ②：连调 3 次，记最后一次的状态
     * TODO ③：CHECK 状态 == AHT21_H_STALE
     * TODO ④：再 CHECK 数据 —— t / h 仍应是 25.6 / 60.0
     *          （这一条才是"缓存真的缓了"的证据，别漏）
     */
}

/* =========================================================================
 * 用例 ④⑤：OFFLINE → 器件恢复 → 回到 OK → 再失败 → STALE
 *
 *   这两步天生连着（中间不能停），所以写在同一个函数里。
 *   ⑤ 的意义：证明 g_fail_count 真的被清零了 —— 否则一失败就该立刻 OFFLINE。
 * =======================================================================*/
static void test_case_recovery_and_reset(void)
{
    float t = 0.0f, h = 0.0f;
    aht21_handler_status_t st = AHT21_H_OK;
    int i;

    /* TODO ①：造 OFFLINE（照用例① 的写法：报 NACK + 喂够次数 + 连调 5 次）
     * TODO ②：让桩"恢复"—— 遥控杆里哪个变量管"还剩几次失败"？
     *          把它摆成 0 会怎样？（这就是"器件治好了"的演法）
     * TODO ③：调一次 → CHECK(st == AHT21_H_OK, "器件恢复 → 状态回到 OK")
     * TODO ④：再让桩失败 3 次 → 调 3 次
     *          → CHECK(st == AHT21_H_STALE, "计数已清零：再失败 3 次只是 STALE")
     */
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
