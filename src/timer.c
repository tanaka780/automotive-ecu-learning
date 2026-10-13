#include "timer.h"

/* 周期判定。時刻はtimer_get_elapsed_ms（src/timer_clock.c、検証用ビルドではsim/timer_clock_virtual.c）
   から得るだけで、時計が実時間か仮想時間かは知らない */

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
