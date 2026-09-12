/* -std=c11だけではnanosleep()が見えないため、POSIX機能テストマクロをヘッダより前に定義する
   （src/timer.cのclock_gettimeと理由は同じ） */
#define _POSIX_C_SOURCE 200809L

#include "scheduler.h"
#include <time.h>   /* nanosleep() */

/* タスクの周期判定をポーリングする間隔 [ms]。以前main.cが直接持っていたポーリング間隔と
   同じ値をここに集約する */
#define SCHEDULER_POLL_INTERVAL_MS 10U

/* task_countを0にリセットする */
void scheduler_init(Scheduler *sched) {
    sched->task_count = 0U;
}

/* 周期period_msのタスクを登録する */
bool scheduler_add_task(Scheduler *sched, SchedulerTaskFunc func, void *context, uint32_t period_ms) {
    if (sched->task_count >= SCHEDULER_MAX_TASKS) {
        return false;
    }

    SchedulerTask *task = &sched->tasks[sched->task_count];
    task->func    = func;
    task->context = context;
    timer_init(&task->timer, period_ms);
    task->has_run = false;
    sched->task_count++;
    return true;
}

/* 登録済みタスクのいずれかの周期が来るまで待ち、その時点で周期が来ている全タスクを実行する */
void scheduler_run_due(Scheduler *sched) {
    struct timespec poll_interval = { .tv_sec = 0, .tv_nsec = (long)SCHEDULER_POLL_INTERVAL_MS * 1000000L };

    bool executed = false;
    while (!executed) {
        for (uint8_t i = 0U; i < sched->task_count; i++) {
            SchedulerTask *task = &sched->tasks[i];
            bool due;

            if (!task->has_run) {
                /* 初回は周期を待たず即座に実行する（offset=0相当）。基準時刻は実行時点に更新し、
                   2回目以降はperiod_msごとの周期判定にする */
                due           = true;
                task->has_run = true;
                timer_init(&task->timer, task->timer.period_ms);
            } else {
                due = timer_is_due(&task->timer);
            }

            if (due) {
                task->func(task->context);
                executed = true;
            }
        }

        if (!executed) {
            (void)nanosleep(&poll_interval, NULL);
        }
    }
}
