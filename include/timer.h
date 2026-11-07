/* インクルードガード */
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "timer_clock.h"   /* timer_get_elapsed_ms・timer_wait_ms（Phase31で分けた） */

/* 周期判定用のタイマー。最後に周期が来たと判定した時刻と、周期の長さを持つ */
typedef struct {
    uint32_t period_ms;
    uint32_t last_due_ms;
} Timer;

/* period_msの周期でtimerを初期化する。基準時刻は呼び出した瞬間の経過時間 */
void timer_init(Timer *timer, uint32_t period_ms);

/* 前回周期が来たと判定してから period_ms 以上経過していればtrueを返し、
   基準時刻を今の経過時間に更新する。経過していなければfalseを返す（基準時刻は変えない） */
bool timer_is_due(Timer *timer);

#endif /* TIMER_H */
