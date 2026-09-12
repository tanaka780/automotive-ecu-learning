/* インクルードガード */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "timer.h"

/* Schedulerに登録するタスク関数の型。scheduler_add_task時に渡したcontextがそのまま渡される */
typedef void (*SchedulerTaskFunc)(void *context);

/* 登録できるタスクの最大数。現状は1個のみ使用するが、将来のCAN受信/送信タスク等の追加に備えて余裕を持たせる */
#define SCHEDULER_MAX_TASKS 4U

/* タスク1個分：呼び出す関数・渡すcontext・周期判定用のTimer・初回実行済みかどうかを保持する */
typedef struct {
    SchedulerTaskFunc func;
    void              *context;
    Timer             timer;
    bool              has_run;
} SchedulerTask;

/* 登録済みタスクの配列と件数を保持する */
typedef struct {
    SchedulerTask tasks[SCHEDULER_MAX_TASKS];
    uint8_t       task_count;
} Scheduler;

/* task_countを0にリセットする */
void scheduler_init(Scheduler *sched);

/* 周期period_msのタスクを登録する。優先度は無く、登録順に判定・実行される。
   上限(SCHEDULER_MAX_TASKS)に達している場合はfalseを返し登録しない */
bool scheduler_add_task(Scheduler *sched, SchedulerTaskFunc func, void *context, uint32_t period_ms);

/* 登録済みタスクのいずれかの周期が来るまで待ち、その時点で周期が来ている全タスクを登録順に実行する。
   各タスクは初回呼び出し時（まだ1度も実行していない間）は周期を待たず即座に実行される（offset=0相当）。
   2回目以降はperiod_msごとの周期判定になる */
void scheduler_run_due(Scheduler *sched);

#endif /* SCHEDULER_H */
