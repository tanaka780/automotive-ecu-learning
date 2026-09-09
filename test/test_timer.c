/* -std=c11だけではnanosleep()が見えないため、POSIX機能テストマクロをヘッダより前に定義する
   （src/timer.cのclock_gettimeと理由は同じ） */
#define _POSIX_C_SOURCE 200809L

#include "unity.h"
#include "timer.h"
#include <time.h>   /* nanosleep() */

void setUp(void) {}
void tearDown(void) {}

/* テスト内で指定ミリ秒だけ待つ（usleepはPOSIX.1-2008で非推奨のため、後継のnanosleepを使う） */
static void sleep_ms(long ms) {
    struct timespec duration = { .tv_sec = 0, .tv_nsec = ms * 1000000L };
    (void)nanosleep(&duration, NULL);
}

/* timer_get_elapsed_msは、時間が経てば同じか大きい値を返す（単調増加）ことを確認する */
static void test_get_elapsed_ms_is_monotonic(void) {
    uint32_t first = timer_get_elapsed_ms();
    sleep_ms(10);   /* 10ms待つ */
    uint32_t second = timer_get_elapsed_ms();

    TEST_ASSERT_TRUE_MESSAGE(second >= first, "経過時間は後の呼び出しほど同じか大きい値になる");
}

/* timer_init直後は、周期がまだ経過していないためtimer_is_dueがfalseを返すことを確認する */
static void test_is_due_false_immediately_after_init(void) {
    Timer timer;
    timer_init(&timer, 1000);   /* 1000ms周期 */

    TEST_ASSERT_FALSE_MESSAGE(timer_is_due(&timer), "init直後は周期(1000ms)がまだ経過していない");
}

/* 周期時間が経過すればtimer_is_dueがtrueを返すことを確認する（テストを高速に保つため短い周期を使う） */
static void test_is_due_true_after_period_elapsed(void) {
    Timer timer;
    timer_init(&timer, 50);   /* 50ms周期 */

    sleep_ms(60);   /* 60ms待つ（50ms周期より確実に長い時間） */

    TEST_ASSERT_TRUE_MESSAGE(timer_is_due(&timer), "周期(50ms)より長く待てばtrueになる");
}

/* timer_is_dueがtrueを返した直後は、基準時刻が更新されているため再度falseに戻ることを確認する */
static void test_is_due_resets_after_firing(void) {
    Timer timer;
    timer_init(&timer, 50);   /* 50ms周期 */

    sleep_ms(60);
    TEST_ASSERT_TRUE_MESSAGE(timer_is_due(&timer), "前提: 1回目はtrueになる");

    TEST_ASSERT_FALSE_MESSAGE(timer_is_due(&timer), "trueを返した直後は基準時刻が更新され、再びfalseになる");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_get_elapsed_ms_is_monotonic);
    RUN_TEST(test_is_due_false_immediately_after_init);
    RUN_TEST(test_is_due_true_after_period_elapsed);
    RUN_TEST(test_is_due_resets_after_firing);

    return UNITY_END();
}
