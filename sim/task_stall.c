#include <stdio.h>
#include <string.h>
#include "task_stall.h"
#include "timer.h"
#include "logger.h"

/* 設定ファイルの1行（"TARGET=FAN_RX_ENGINE_STATUS"等）が収まるサイズ */
#define TASK_STALL_LINE_SIZE 64

/* ログに出すタスクの名前。設定ファイルのTARGETの値と同じにする */
static const char *task_stall_name(TaskStallTarget task) {
    if (task == TASK_STALL_FAN_RX_FAULT_STATUS) {
        return "FAN_RX_FAULT_STATUS";
    }
    if (task == TASK_STALL_FAN_RX_ENGINE_STATUS) {
        return "FAN_RX_ENGINE_STATUS";
    }
    return "NONE";
}

void task_stall_init(TaskStallConfig *cfg) {
    cfg->target   = TASK_STALL_NONE;
    cfg->start_ms = 0U;
    cfg->end_ms   = 0U;
    cfg->base_ms  = timer_get_elapsed_ms();
    cfg->stopped  = false;
}

/* "KEY=VALUE"の1行を解釈する。未知のキー・値欠落・数値に変換できない行・不正なTARGET値は無視する
   （can_fault.cと同じ考え方） */
static void task_stall_apply_line(TaskStallConfig *cfg, const char *line) {
    char key[TASK_STALL_LINE_SIZE];
    char str_value[TASK_STALL_LINE_SIZE];
    int  int_value;

    if (sscanf(line, "%63[^=]=%63s", key, str_value) != 2) {
        return;
    }

    if (strcmp(key, "TARGET") == 0) {
        if (strcmp(str_value, "FAN_RX_FAULT_STATUS") == 0) {
            cfg->target = TASK_STALL_FAN_RX_FAULT_STATUS;
        } else if (strcmp(str_value, "FAN_RX_ENGINE_STATUS") == 0) {
            cfg->target = TASK_STALL_FAN_RX_ENGINE_STATUS;
        } else {
            /* 不正なTARGET値は無視する */
        }
        return;
    }

    if (sscanf(str_value, "%d", &int_value) != 1) {
        return;   /* 数値に変換できない値は無視する */
    }

    if ((strcmp(key, "START_MS") == 0) && (int_value >= 0)) {
        cfg->start_ms = (uint32_t)int_value;
    } else if ((strcmp(key, "END_MS") == 0) && (int_value >= 0)) {
        cfg->end_ms = (uint32_t)int_value;
    } else {
        /* 未知のキー・負の値は無視する */
    }
}

bool task_stall_load(TaskStallConfig *cfg, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return false;   /* ファイルが無ければ止めない。ログも出さない */
    }

    TaskStallConfig loaded = *cfg;   /* base_msはtask_stall_init時点のものを引き継ぐ */
    char line[TASK_STALL_LINE_SIZE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        task_stall_apply_line(&loaded, line);
    }
    /* 読み込みモードのfcloseのため、失敗しても既読データの正しさに影響しない（MISRA 17.7） */
    (void)fclose(fp);

    if (loaded.target == TASK_STALL_NONE) {
        log_print_leveled(LOG_INFO, "STALL", "Loaded task stall file (no task stopped)");
        return false;
    }

    *cfg = loaded;
    char msg[64];   /* "Loaded task stall file (FAN_RX_ENGINE_STATUS)" が収まるサイズ */
    (void)snprintf(msg, sizeof(msg), "Loaded task stall file (%s)", task_stall_name(cfg->target));
    log_print_leveled(LOG_INFO, "STALL", msg);
    return true;
}

bool task_stall_is_stopped(const TaskStallConfig *cfg, TaskStallTarget task, uint32_t elapsed_ms) {
    if ((cfg->target == TASK_STALL_NONE) || (cfg->target != task)) {
        return false;
    }
    return (elapsed_ms >= cfg->start_ms) && (elapsed_ms <= cfg->end_ms);
}

bool task_stall_should_skip(TaskStallConfig *cfg, TaskStallTarget task) {
    if ((cfg->target == TASK_STALL_NONE) || (cfg->target != task)) {
        return false;   /* 止める対象ではないタスクは、時計も読まずにそのまま呼ぶ */
    }
    bool stop = task_stall_is_stopped(cfg, task, timer_get_elapsed_ms() - cfg->base_ms);
    if (stop != cfg->stopped) {
        char msg[48];   /* "FAN_RX_ENGINE_STATUS resumed" が収まるサイズ */
        (void)snprintf(msg, sizeof(msg), "%s %s", task_stall_name(task), stop ? "stopped" : "resumed");
        log_print_leveled(LOG_INFO, "STALL", msg);
        cfg->stopped = stop;
    }
    return stop;
}
