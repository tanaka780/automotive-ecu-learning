#include <stdio.h>
#include <stdlib.h>   /* rand() を使うために必要 */
#include "ignition.h"
#include "logger.h"

/* イグニッション状態を初期値（OFF、前回状態もOFF）に設定する */
void ignition_init(Ignition *ignition) {
    ignition->current  = IGNITION_OFF;
    ignition->previous = IGNITION_OFF;
}

/* 実際のイグニッションはドライバー操作で変わるが、このプロジェクトでは学習用にランダム値で代替する */
void ignition_update(Ignition *ignition) {
    ignition->previous = ignition->current;
    ignition->current  = (rand() % 2 == 0) ? IGNITION_OFF : IGNITION_ON;
}

/* イグニッション状態を表示名に変換する（ignition.c専用の表示ヘルパー） */
static const char *ignition_state_to_str(IgnitionState state) {
    switch (state) {
        case IGNITION_OFF: return "OFF";
        case IGNITION_ON:  return "ON";
        default:           return "UNKNOWN";
    }
}

/* 前回と今回の状態を比較し、遷移した瞬間だけイベントとして表示する */
void ignition_check(const Ignition *ignition) {
    if (ignition->current != ignition->previous) {
        char line[32];   /* "OFF -> ON" が収まるサイズ */
        snprintf(line, sizeof(line), "%s -> %s",
                 ignition_state_to_str(ignition->previous),
                 ignition_state_to_str(ignition->current));
        log_print_leveled(LOG_INFO, "IGN", line);
    }
}

/* 現在のイグニッション状態を1行でコンソールに出力する */
void ignition_print(const Ignition *ignition) {
    log_print_leveled(LOG_INFO, "IGN", ignition_state_to_str(ignition->current));
}
