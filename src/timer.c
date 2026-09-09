/* -std=c11だけではclock_gettime/CLOCK_MONOTONICが見えないため、POSIX機能テストマクロを
   ヘッダより前に定義する（200809L = POSIX.1-2008、clock_gettimeが属するバージョン） */
#define _POSIX_C_SOURCE 200809L

#include "timer.h"
#include <time.h>

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

/* period_msの周期でtimerを初期化する。基準時刻は呼び出した瞬間の経過時間 */
void timer_init(Timer *timer, uint32_t period_ms) {
    timer->period_ms   = period_ms;
    timer->last_due_ms = timer_get_elapsed_ms();
}

/* 前回周期が来たと判定してから period_ms 以上経過していればtrueを返し、基準時刻を更新する */
bool timer_is_due(Timer *timer) {
    uint32_t now_ms  = timer_get_elapsed_ms();
    /* uint32_t同士の引き算は、途中で桁あふれをまたいでも正しい経過時間になる（符号なし整数の性質） */
    uint32_t elapsed = now_ms - timer->last_due_ms;

    if (elapsed >= timer->period_ms) {
        timer->last_due_ms = now_ms;
        return true;
    }
    return false;
}
