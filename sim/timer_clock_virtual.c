#include "timer.h"

/* 検証用の仮想時計（PC上の検証専用）。src/timer_clock.cの代わりにリンクし、sensor_sim_virtualを作る。
   時刻はtimer_wait_msで進めた分だけ進み、実際には待たない。処理にかかった実時間は時刻に入らないため、
   タスクは常に周期ちょうどに実行され、同じ入力なら出力が毎回同じになる */

/* 仮想時刻の開始値 [ms]。0から始めると、CAN受信側の「まだ何も受け取っていない」初期値
   （last_seen_timestamp_ms = 0）と最初のフレームの送信時刻が同じになり、最初の受信を新しいフレームと
   判定できない。実時間のCLOCK_MONOTONIC（OS起動からの経過時間）も0にはならないため、0以外から始める */
#define VIRTUAL_START_MS 1000U

static uint32_t g_virtual_ms = VIRTUAL_START_MS;

uint32_t timer_get_elapsed_ms(void) {
    return g_virtual_ms;
}

void timer_wait_ms(uint32_t ms) {
    g_virtual_ms += ms;
}
