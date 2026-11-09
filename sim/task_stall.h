/* インクルードガード */
#ifndef TASK_STALL_H
#define TASK_STALL_H

#include <stdint.h>
#include <stdbool.h>

/* タスクの停止を起こす設定ファイルのデフォルトのファイル名（Phase32） */
#define TASK_STALL_FILENAME "task_stall.txt"

/* 止めるタスク。can_fault.txtのTARGET（CANのメッセージ）と取り違えないよう、タスクの名前で呼ぶ。
   NONEは止めない */
typedef enum {
    TASK_STALL_NONE = 0,
    TASK_STALL_FAN_RX_FAULT_STATUS,    /* ファン制御ECU役の警告灯データの受信（200ms周期） */
    TASK_STALL_FAN_RX_ENGINE_STATUS    /* ファン制御ECU役のゲージデータの受信（1000ms周期） */
} TaskStallTarget;

/* 指定した期間だけ、タスクを呼ばない設定（Phase32）。can_fault.cが送信をラップするのと同じく、
   main.cのタスクの変換関数がこの判定を見て、止める期間ならECU役の関数を呼ばずに戻る。
   ECU役のコード（fan_ecu.c）とSchedulerは変えない */
typedef struct {
    TaskStallTarget target;    /* 止めるタスク */
    uint32_t        start_ms;  /* プログラム起動からの経過時間がこれ以上で止める */
    uint32_t        end_ms;    /* プログラム起動からの経過時間がこれを超えたら、また呼ぶ */
    uint32_t        base_ms;   /* 基準時刻（task_stall_init時点のtimer_get_elapsed_ms）。can_fault.cと同じ */
    bool            stopped;   /* 前回の判定で止めていたか（止めた・また呼んだ瞬間だけログを出すため） */
} TaskStallConfig;

/* cfgを初期値（止めない）にする。base_msをこの呼び出し時点のtimer_get_elapsed_msで記録する */
void task_stall_init(TaskStallConfig *cfg);

/* 指定したファイルをKEY=VALUE形式（TARGET=FAN_RX_FAULT_STATUS/FAN_RX_ENGINE_STATUS, START_MS=, END_MS=）で
   読み込む。TARGETが正しく指定されていればcfgに反映してtrueを返す。ファイルが無い場合は何も表示せず
   （uds_tester.cと同じ。ファイルを置かない実行の出力を変えないため）、cfgを初期値のままfalseを返す。
   START_MSとEND_MSの大小関係はチェックしない（can_fault.cと同じ） */
bool task_stall_load(TaskStallConfig *cfg, const char *filename);

/* elapsed_ms（プログラム起動からの経過時間）の時点でtaskを止めるかを判定する
   （cfg->target==task && start_ms<=elapsed_ms<=end_ms）。時計を読まない判定だけの関数にして、
   テストから任意の経過時間を指定できるようにする */
bool task_stall_is_stopped(const TaskStallConfig *cfg, TaskStallTarget task, uint32_t elapsed_ms);

/* 今の時刻でtaskを止めるならtrueを返す。止め始めた・また呼び始めた瞬間に[STALL]のログを出す */
bool task_stall_should_skip(TaskStallConfig *cfg, TaskStallTarget task);

#endif /* TASK_STALL_H */
