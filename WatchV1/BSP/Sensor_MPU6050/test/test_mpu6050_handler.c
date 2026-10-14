/* ============================================================================
 * test_mpu6050_handler.c —— mpu6050_handler 的 SWUT 单元测试（PC 上跑，不碰板子）
 *
 * 靠 mpu6050_stub.c 顶替真驱动。本测试是【白盒】—— 见下面那个 include 的说明。
 *
 * 编译运行（在 test/ 目录下）：
 *     gcc -Wall -I.. test_mpu6050_handler.c mpu6050_stub.c -o test.exe
 *     ./test.exe
 *
 *   ⚠️ 注意命令行里【没有】../mpu6050_handler.c —— 它已经在下头 include 进来了。
 * ==========================================================================*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mpu6050.h"            /* 桩的遥控杆用到 mpu6050_err_t / MPU6050_OK */
#include "mpu6050_handler.h"

/* ── 白盒：直接包含被测的 .c ──────────────────────────────────────────
 *   目的：handler 的 static 变量（g_last / g_fail_count / g_has_data）
 *         只在【编译单元】内可见。把 .c include 进来 ⇒ 测试与它同处一个
 *         编译单元 ⇒ 测试才能直接读写它们，构造"干净起点"（用例④ 需要）。
 *
 *   代价：① 编译命令必须删掉 ../mpu6050_handler.c，否则函数被定义两次
 *         ② 两边的 static 名字共用【同一个名字空间】—— 不能重名
 *            （所以下面的计数变量叫 g_test_pass / g_test_fail，不叫 g_pass）
 * -------------------------------------------------------------------*/
#include "../mpu6050_handler.c"

/* ── 桩的「遥控杆」：定义在 mpu6050_stub.c 里 ──────────────────────── */
extern mpu6050_err_t  stub_read_err;
extern uint32_t       stub_fail_times;
extern mpu6050_data_t stub_data;

/* ── 极简测试设施：不引框架，两个宏就够 ────────────────────────────────
 *    不用 assert()：assert 一失败就把程序打停，后面用例全不跑了。
 *    这里要「失败也继续，最后统一算总账」。
 * -------------------------------------------------------------------------*/
static int g_test_pass = 0;
static int g_test_fail = 0;

#define CHECK(cond, msg)                                                \
    do {                                                                \
        if (cond) { g_test_pass++; printf("  [PASS] %s\n", (msg)); }     \
        else      { g_test_fail++; printf("  [FAIL] %s\n", (msg)); }     \
    } while (0)

/* 每条用例开跑前「复位遥控杆」—— 用例之间必须互不影响 */
static void stub_reset(void)
{
    stub_read_err   = MPU6050_OK;
    stub_fail_times = 0;
}

/* ── 把 handler 拉回「已知状态」────────────────────────────────────────
 *   手段：喂它一次成功（成功会清计数、建缓存）。
 *   ⚠️ 它给出的起点是"有缓存"，不是"干净" —— 需要干净起点的用例（④）
 *      必须直接动 static 变量，那是白盒 include 才给的能力。
 * -------------------------------------------------------------------------*/
static void handler_prime(void)
{
    mpu6050_data_t d = {0};

    stub_reset();                   /* 桩侧：恢复成"一直成功" */
    mpu6050_handler_read(&d);
}

/* ── 直接把 handler 清成"从没成功过"（白盒专用）────────────────────────
 *   只清状态、不喂数据 —— 用例④ 要的就是这个起点。
 * -------------------------------------------------------------------------*/
static void handler_wipe(void)
{
    memset(&g_last, 0, sizeof(g_last));
    g_fail_count = 0;
    g_has_data   = 0;               /* ★ 关键：手里没有任何旧数据 */
}

/* =========================================================================
 * 用例 ① 参数检查 · 传空指针 → H_PARAM
 *
 *   ⚠️ 这一支【不计入失败账本】—— 参数错是"调用"的问题，不是器件的问题
 * =======================================================================*/
static void test_case_param_null(void)
{
    mpu6050_handler_status_t st = mpu6050_handler_read(NULL);

    CHECK(st == MPU6050_H_PARAM, "传 NULL → 应返回 H_PARAM");
}

/* =========================================================================
 * 用例 ② 一直成功 → 一直 H_OK，且出参 = 桩给的那一包
 * =======================================================================*/
static void test_case_always_ok(void)
{
    mpu6050_data_t d = {0};
    mpu6050_handler_status_t st = MPU6050_H_OK;
    int i;
    int bad = 0;

    handler_prime();

    for (i = 0; i < 3; i++)
    {
        st = mpu6050_handler_read(&d);
        if (st != MPU6050_H_OK)
        {
            bad++;
        }
    }

    CHECK(bad == 0, "连读 3 次都应为 OK");

    /* 比"一整包"：C 里 struct 没有 == （内部可能有 padding，无意义）
     *   这里用 memcmp 逐字节比 —— 前提是 mpu6050_data_t 【无 padding、无指针】，
     *   当前成立（7×int16_t + 7×float，天然对齐、无空隙）。
     *   ⚠️ 将来若加了 uint8_t / bool 之类，就会引入 padding ⇒ 改用逐字段比。 */
    CHECK(memcmp(&d, &stub_data, sizeof(d)) == 0, "数据应是桩给的那一包");
}

/* =========================================================================
 * 用例 ③ 先成功建缓存 → 再短暂失败 → H_STALE，且给的是【上次那一包】
 * =======================================================================*/
static void test_case_stale_keeps_cached(void)
{
    mpu6050_data_t d = {0};
    mpu6050_handler_status_t st = MPU6050_H_OK;
    int i;

    handler_prime();                /* 这一步就已经建好缓存了 */

    stub_read_err   = MPU6050_ERR_NACK;
    stub_fail_times = 100;          /* 一直报，别中途"恢复" */

    for (i = 0; i < 3; i++)         /* 3 < 阈值 5 → 还没到撤数据的时候 */
    {
        st = mpu6050_handler_read(&d);
    }

    CHECK(st == MPU6050_H_STALE, "失败 3 次 → 应为 STALE");
    CHECK(memcmp(&d, &stub_data, sizeof(d)) == 0,
          "STALE 时给出的应是上次那份数据");
}

/* =========================================================================
 * 用例 ④ 从没成功过就失败 → H_OFFLINE（无旧可给）
 *
 *   这是 OFFLINE 的【另一个出口】—— 不是"失败到阈值"，是"压根没有旧数据"。
 *   要造这个起点，只能直接动 handler 的 static ⇒ handler_wipe()。
 * =======================================================================*/
static void test_case_offline_no_data(void)
{
    mpu6050_data_t d = {0};
    mpu6050_handler_status_t st;

    handler_wipe();                 /* ★ 干净起点：从没成功过 */
    stub_reset();                   /* 桩侧也跟着复位，防止上一用例的残留设定 */

    stub_read_err   = MPU6050_ERR_NACK;
    stub_fail_times = 100;

    st = mpu6050_handler_read(&d);  /* 第一次失败就该走"无旧可给"这条出口 */

    CHECK(st == MPU6050_H_OFFLINE, "没有旧数据时第一次失败 → 应为 OFFLINE");
}

/* =========================================================================
 * 用例 ⑤ 连续失败到阈值 → H_OFFLINE，且【旧数据被撤】
 *
 *   两条判据：
 *     ① 第 5 次失败后 → OFFLINE
 *     ② "撤数据"真的生效 —— 再失败一次仍是 OFFLINE（若没撤干净会掉回 STALE）
 * =======================================================================*/
static void test_case_offline_after_failures(void)
{
    mpu6050_data_t d = {0};
    mpu6050_handler_status_t st = MPU6050_H_OK;
    int i;

    handler_prime();                /* 先让 handler 手里有数据 */

    stub_read_err   = MPU6050_ERR_NACK;
    stub_fail_times = 100;

    for (i = 0; i < MPU6050_H_OFFLINE_THRESHOLD; i++)
    {
        st = mpu6050_handler_read(&d);
    }
    CHECK(st == MPU6050_H_OFFLINE, "连续 5 次 NACK 后 → 状态应为 OFFLINE");

    /* ② 再失败一次：证明 g_has_data 真的被清了
     *    （如果那行没生效，这里会因为没有清干净而返回 STALE） */
    st = mpu6050_handler_read(&d);
    CHECK(st == MPU6050_H_OFFLINE, "旧数据已撤：再失败一次仍应为 OFFLINE");
}

/* =========================================================================
 * 用例 ⑥ 重试能救回来 —— 桩"失败 2 次后成功"，一次 handler 调用就够
 *
 *   设定：stub_fail_times = 2（只失败 2 次）
 *   动作：调【一次】handler_read（它内部最多试 MPU6050_H_MAX_TRIES 次）
 *   期望：H_OK，且给的是【新】数据
 *
 *   ⚠️ 这就是"重试"最直接的价值：一次偶发抖动，
 *      不该被当成一次失败报给上层。
 * =======================================================================*/
static void test_case_retry_recovers(void)
{
    mpu6050_data_t d = {0};
    mpu6050_handler_status_t st;

    handler_wipe();                 /* 干净起点：免得看到旧数据搞混判断 */
    stub_reset();

    stub_read_err   = MPU6050_ERR_NACK;
    stub_fail_times = 2;            /* ← 前 2 次失败，第 3 次就成功 */

    st = mpu6050_handler_read(&d);

    CHECK(st == MPU6050_H_OK, "重试救回 → 应为 H_OK");
    CHECK(memcmp(&d, &stub_data, sizeof(d)) == 0, "救回时给的应是新数据");
}

/* =========================================================================
 * 用例 ⑦ 重试之间的【延时真的被调了】，且次数 = 尝试次数 − 1
 *
 *   设定：注入一个"只计数、真不睡"的延时函数；桩一直失败
 *   动作：调一次 handler_read（内部试 3 次）
 *   期望：延时被调 【2】次 —— 不是 3 次
 *         试1失败 → 还有下次 → 等(1)
 *         试2失败 → 还有下次 → 等(2)
 *         试3失败 → 没有下次 → 不等    ← try < MAX_TRIES-1 守的就是这里
 *
 *   ⚠️ 这条用例正是"把延时做成可注入"换来的能力：
 *      不注入的话，"等没等、等几次"根本没法断言。
 * =======================================================================*/
static int g_delay_calls = 0;
static void counting_delay(uint32_t ms) { (void)ms; g_delay_calls++; }

static void test_case_retry_delay_count(void)
{
    mpu6050_data_t d = {0};
    mpu6050_handler_status_t st;

    mpu6050_handler_set_delay(counting_delay);   /* 注入"计数器"替掉默认 noop */

    handler_wipe();
    stub_reset();
    stub_read_err   = MPU6050_ERR_NACK;
    stub_fail_times = 100;                       /* 一直失败 */

    g_delay_calls = 0;                           /* ★ 先清零，否则数的是累计 */
    st = mpu6050_handler_read(&d);

    CHECK(st != MPU6050_H_OK, "全失败 → 不该报 OK");
    CHECK(g_delay_calls == 2, "试 3 次 → 只该等 2 次（末次不等）");

    mpu6050_handler_set_delay(NULL);             /* ★ 恢复默认，别污染后面的用例 */
}

/* =========================================================================
 * 用例 ⑧ 不值得重试的错 → 一次都不等（立刻放弃）
 *
 *   设定：桩返回 MPU6050_ERR_ID —— "重试一万次也一样"的那种错
 *   期望：延时 0 次（立刻跳出循环，不浪费时间）
 * =======================================================================*/
static void test_case_no_retry_for_hopeless(void)
{
    mpu6050_data_t d = {0};
    mpu6050_handler_status_t st;

    mpu6050_handler_set_delay(counting_delay);

    handler_wipe();
    stub_reset();
    stub_read_err   = MPU6050_ERR_ID;            /* ← 换成"重试也没用"的错 */
    stub_fail_times = 100;

    g_delay_calls = 0;
    st = mpu6050_handler_read(&d);

    CHECK(st != MPU6050_H_OK, "ID 错 → 不该报 OK");
    CHECK(g_delay_calls == 0, "不值得重试的错 → 一次都不该等");

    mpu6050_handler_set_delay(NULL);
}

/* =========================================================================*/

int main(void)
{
    printf("=== mpu6050_handler SWUT ===\n");

    test_case_param_null();               /* ① */
    test_case_always_ok();                /* ② */
    test_case_stale_keeps_cached();       /* ③ */
    test_case_offline_no_data();          /* ④ 从没成功过 → OFFLINE */
    test_case_offline_after_failures();   /* ⑤ 失败到阈值 → OFFLINE + 撤数据 */
    test_case_retry_recovers();           /* ⑥ 重试救回 → OK */
    test_case_retry_delay_count();        /* ⑦ 延时次数 = 尝试次数 − 1 */
    test_case_no_retry_for_hopeless();    /* ⑧ 不值得重试的错 → 不等 */

    printf("\n%d passed / %d failed\n", g_test_pass, g_test_fail);
    return (g_test_fail == 0) ? 0 : 1;
}
