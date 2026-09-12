/* -std=c11だけではnanosleep()が見えないため、POSIX機能テストマクロをヘッダより前に定義する
   （src/timer.cのclock_gettimeと理由は同じ） */
#define _POSIX_C_SOURCE 200809L

#include "unity.h"
#include "scheduler.h"
#include <time.h>   /* nanosleep() */

void setUp(void) {}
void tearDown(void) {}

/* テスト内で指定ミリ秒だけ待つ（usleepはPOSIX.1-2008で非推奨のため、後継のnanosleepを使う） */
static void sleep_ms(long ms) {
    struct timespec duration = { .tv_sec = 0, .tv_nsec = ms * 1000000L };
    (void)nanosleep(&duration, NULL);
}

/* テスト用タスク関数：呼ばれるたびにcontext(int*)が指すカウンタを1増やす */
static void count_up_task(void *context) {
    int *count = (int *)context;
    (*count)++;
}

/* 登録直後の1回目のscheduler_run_dueは、周期を待たずに即座にタスクを実行することを確認する（offset=0相当） */
static void test_task_runs_immediately_on_first_call(void) {
    Scheduler sched;
    scheduler_init(&sched);
    int count = 0;
    TEST_ASSERT_TRUE(scheduler_add_task(&sched, count_up_task, &count, 1000));

    scheduler_run_due(&sched);

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, count, "初回呼び出しは周期を待たず即座に実行される");
}

/* 初回実行の直後は、周期(period_ms)が経過するまで2回目が実行されないことを確認する */
static void test_task_waits_for_period_after_first_run(void) {
    Scheduler sched;
    scheduler_init(&sched);
    int count = 0;
    TEST_ASSERT_TRUE(scheduler_add_task(&sched, count_up_task, &count, 50));   /* 50ms周期 */

    scheduler_run_due(&sched);   /* 1回目: 即座に実行される */
    TEST_ASSERT_EQUAL_INT(1, count);

    sleep_ms(60);   /* 50ms周期より確実に長い時間待つ */
    scheduler_run_due(&sched);   /* 2回目: 周期が経過しているため実行される */

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, count, "周期(50ms)が経過すれば2回目が実行される");
}

/* 上限(SCHEDULER_MAX_TASKS)を超えるタスク登録はfalseを返し、登録されないことを確認する */
static void test_add_task_fails_when_table_is_full(void) {
    Scheduler sched;
    scheduler_init(&sched);
    int count = 0;

    for (uint8_t i = 0U; i < SCHEDULER_MAX_TASKS; i++) {
        TEST_ASSERT_TRUE_MESSAGE(scheduler_add_task(&sched, count_up_task, &count, 1000),
                                  "上限内の登録は成功する");
    }

    TEST_ASSERT_FALSE_MESSAGE(scheduler_add_task(&sched, count_up_task, &count, 1000),
                               "上限(SCHEDULER_MAX_TASKS)を超える登録はfalseを返す");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_task_runs_immediately_on_first_call);
    RUN_TEST(test_task_waits_for_period_after_first_run);
    RUN_TEST(test_add_task_fails_when_table_is_full);

    return UNITY_END();
}
