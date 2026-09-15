#include <stdio.h>
#include <string.h>
#include "can_fault.h"
#include "timer.h"
#include "logger.h"

/* CAN Fault設定ファイルの1行（"TARGET=ENGINE_STATUS"等）が収まるサイズ */
#define CAN_FAULT_LINE_SIZE 64

/* ファイルを読みながら組み立てる途中結果。mode_seen/mode_dropでMODE行の有無と内容を、
   target_seenでTARGET行の有無を保持する（TARGET未指定ならDrop対象を特定できないため無効扱いにする） */
typedef struct {
    bool           mode_seen;
    bool           mode_drop;
    bool           target_seen;
    CanFaultConfig cfg;
} CanFaultValues;

void can_fault_init(CanFaultConfig *cfg) {
    cfg->active   = false;
    cfg->target   = CAN_MSG_ENGINE_STATUS;
    cfg->start_ms = 0U;
    cfg->end_ms   = 0U;
    cfg->base_ms  = timer_get_elapsed_ms();
}

/* "KEY=VALUE"の1行を解釈する。未知のキー・値欠落・数値に変換できない行・不正なTARGET値は
   無視する（fixture.c/config.cと同じ考え方）。fixture.c/config.cにも同名のstatic関数があり、
   識別子が別ファイルで重複しないよう関数名にモジュール名を付ける（MISRA 5.9） */
static void can_fault_apply_line(CanFaultValues *fv, const char *line) {
    char key[CAN_FAULT_LINE_SIZE];
    char str_value[CAN_FAULT_LINE_SIZE];
    int  int_value;

    if (sscanf(line, "%63[^=]=%63s", key, str_value) != 2) {
        return;
    }

    if (strcmp(key, "MODE") == 0) {
        fv->mode_seen = true;
        fv->mode_drop = (strcmp(str_value, "DROP") == 0);
        return;
    }

    if (strcmp(key, "TARGET") == 0) {
        if (strcmp(str_value, "ENGINE_STATUS") == 0) {
            fv->target_seen = true;
            fv->cfg.target  = CAN_MSG_ENGINE_STATUS;
        } else if (strcmp(str_value, "FAULT_STATUS") == 0) {
            fv->target_seen = true;
            fv->cfg.target  = CAN_MSG_FAULT_STATUS;
        } else {
            /* 不正なTARGET値は無視する */
        }
        return;
    }

    if (sscanf(str_value, "%d", &int_value) != 1) {
        return;   /* 数値に変換できない値は無視する */
    }

    if (strcmp(key, "START_MS") == 0) {
        if (int_value >= 0) {
            fv->cfg.start_ms = (uint32_t)int_value;
        }
    } else if (strcmp(key, "END_MS") == 0) {
        if (int_value >= 0) {
            fv->cfg.end_ms = (uint32_t)int_value;
        }
    } else {
        /* 未知のキーは無視する */
    }
}

bool can_fault_load(CanFaultConfig *cfg, const char *filename) {
    uint32_t base_ms = cfg->base_ms;   /* can_fault_init時点の基準時刻を保持する（後述のfv.cfgで上書きしない） */

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        log_print_leveled(LOG_INFO, "CAN-FAULT", "No CAN fault file (no injection)");
        return false;
    }

    CanFaultValues fv;
    fv.mode_seen   = false;
    fv.mode_drop   = false;
    fv.target_seen = false;
    can_fault_init(&fv.cfg);

    char line[CAN_FAULT_LINE_SIZE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        can_fault_apply_line(&fv, line);
    }
    /* 読み込みモードのfcloseのため、失敗しても既読データの正しさに影響しない。戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)fclose(fp);

    if (fv.mode_seen && fv.mode_drop && fv.target_seen) {
        fv.cfg.active  = true;
        fv.cfg.base_ms = base_ms;
        *cfg = fv.cfg;
        log_print_leveled(LOG_INFO, "CAN-FAULT", "Loaded CAN fault file (DROP mode)");
        return true;
    }

    log_print_leveled(LOG_INFO, "CAN-FAULT", "Loaded CAN fault file (no injection)");
    return false;
}

bool can_fault_is_dropped(const CanFaultConfig *cfg, CanMessageId msg, uint32_t elapsed_ms) {
    if ((!cfg->active) || (cfg->target != msg)) {
        return false;
    }
    return (elapsed_ms >= cfg->start_ms) && (elapsed_ms <= cfg->end_ms);
}

/* timer_get_elapsed_msはシステム起動からの経過時間を返すため、cfg->base_msとの差分を取って
   プログラム起動からの経過時間に変換する。符号なし整数の引き算のため桁あふれをまたいでも正しい値になる */
static uint32_t can_fault_elapsed_since_start(const CanFaultConfig *cfg) {
    return timer_get_elapsed_ms() - cfg->base_ms;
}

void can_fault_send_engine_status(const CanFaultConfig *cfg, CanBus *bus, const VehicleSensorData *data) {
    if (can_fault_is_dropped(cfg, CAN_MSG_ENGINE_STATUS, can_fault_elapsed_since_start(cfg))) {
        return;   /* 意図的に送信しない(Timeoutを再現) */
    }
    can_send_engine_status(bus, data);
}

void can_fault_send_fault_status(const CanFaultConfig *cfg, CanBus *bus, const FaultManager *fault_mgr) {
    if (can_fault_is_dropped(cfg, CAN_MSG_FAULT_STATUS, can_fault_elapsed_since_start(cfg))) {
        return;   /* 意図的に送信しない(Timeoutを再現) */
    }
    can_send_fault_status(bus, fault_mgr);
}
