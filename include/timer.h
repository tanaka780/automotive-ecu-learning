/* インクルードガード */
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

/* 起動からの経過時間を取得する（単調増加クロック CLOCK_MONOTONIC を使用）。
   システム時刻の変更（NTP同期等）の影響を受けない。約49.7日（2^32ミリ秒）で
   桁あふれするが、本プロジェクトの実行時間では問題にならない */
uint32_t timer_get_elapsed_ms(void);

/* ms ミリ秒待つ。実時間のビルドでは実際に待ち、検証用のビルドでは仮想時間を進めるだけで待たない。
   timer_get_elapsed_msと合わせて、src/timer_clock.c（実時間）とsim/timer_clock_virtual.c（仮想時間）
   のどちらか一方をリンクする */
void timer_wait_ms(uint32_t ms);

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
