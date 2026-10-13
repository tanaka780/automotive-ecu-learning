/* -std=c11だけではclock_gettime/CLOCK_MONOTONIC/nanosleepが見えないため、POSIX機能テストマクロを
   ヘッダより前に定義する（200809L = POSIX.1-2008、clock_gettimeが属するバージョン） */
#define _POSIX_C_SOURCE 200809L

#include "timer.h"
#include <time.h>

/* 実時間の時計と待機。timer.cの周期判定はここで得た時刻だけを使う。
   検証用のビルド（sensor_sim_virtual）では、このファイルの代わりにsim/timer_clock_virtual.cを使う */

/* 起動からの経過時間を取得する（単調増加クロック CLOCK_MONOTONIC を使用） */
uint32_t timer_get_elapsed_ms(void) {
    struct timespec ts;
    /* このプロジェクトの前提（CLOCK_MONOTONICが使える環境）では失敗しないため、
       戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);

    /* 複合式を直接キャストせず、一旦同じ本質型(uint64_t)の変数で受けてからキャストする（MISRA 10.8） */
    uint64_t total_ms = ((uint64_t)ts.tv_sec * 1000U) + ((uint64_t)ts.tv_nsec / 1000000U);
    return (uint32_t)total_ms;
}

/* ms ミリ秒だけ実際に待つ（もとはscheduler.cが直接nanosleepを呼んでいた） */
void timer_wait_ms(uint32_t ms) {
    struct timespec duration = { .tv_sec = 0, .tv_nsec = (long)ms * 1000000L };
    (void)nanosleep(&duration, NULL);
}
