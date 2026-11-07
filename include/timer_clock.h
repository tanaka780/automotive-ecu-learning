/* インクルードガード */
#ifndef TIMER_CLOCK_H
#define TIMER_CLOCK_H

#include <stdint.h>

/* 時計と待機（いちばん下のドライバ。Phase31でtimer.hから分けた）。
   中身は、src/timer_clock.c（実時間）とsim/timer_clock_virtual.c（仮想時間）のどちらか一方をリンクする。
   周期判定（timer.h）はこのヘッダを読む側で、ドライバのヘッダはドライバの側が持つ */

/* 起動からの経過時間を取得する（単調増加クロック CLOCK_MONOTONIC を使用）。
   システム時刻の変更（NTP同期等）の影響を受けない。約49.7日（2^32ミリ秒）で
   桁あふれするが、本プロジェクトの実行時間では問題にならない */
uint32_t timer_get_elapsed_ms(void);

/* ms ミリ秒待つ。実時間のビルドでは実際に待ち、検証用のビルドでは仮想時間を進めるだけで待たない */
void timer_wait_ms(uint32_t ms);

#endif /* TIMER_CLOCK_H */
