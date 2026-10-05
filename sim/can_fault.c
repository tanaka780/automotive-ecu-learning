#include <stdio.h>
#include <string.h>
#include "can_fault.h"
#include "timer.h"
#include "logger.h"

/* CAN Fault設定ファイルの1行（"TARGET=ENGINE_STATUS"等）が収まるサイズ */
#define CAN_FAULT_LINE_SIZE 64

/* ファイルを読みながら組み立てる途中結果。mode_seen/parsed_modeでMODE行の有無と内容を、
   target_seenでTARGET行の有無を保持する（TARGET未指定なら対象を特定できないため無効扱いにする） */
typedef struct {
    bool           mode_seen;
    CanFaultMode   parsed_mode;
    bool           target_seen;
    CanFaultConfig cfg;
} CanFaultValues;

void can_fault_init(CanFaultConfig *cfg) {
    cfg->mode     = CAN_FAULT_MODE_NONE;
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
        if (strcmp(str_value, "DROP") == 0) {
            fv->parsed_mode = CAN_FAULT_MODE_DROP;
        } else if (strcmp(str_value, "CORRUPT") == 0) {
            fv->parsed_mode = CAN_FAULT_MODE_CORRUPT;
        } else {
            fv->parsed_mode = CAN_FAULT_MODE_NONE;   /* NORMAL・不正な値は注入なし扱い */
        }
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
    fv.parsed_mode = CAN_FAULT_MODE_NONE;
    fv.target_seen = false;
    can_fault_init(&fv.cfg);

    char line[CAN_FAULT_LINE_SIZE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        can_fault_apply_line(&fv, line);
    }
    /* 読み込みモードのfcloseのため、失敗しても既読データの正しさに影響しない。戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)fclose(fp);

    /* CORRUPTはENGINE_STATUSのみ対応する。FAULT_STATUSはビットフラグのみでcan.cにInvalid Data
       判定ロジックが無く、Corruptしても検出できず意味を持たないため、無効な組み合わせとして
       注入なし扱いにする（Phase22拡張バックログで決定） */
    bool corrupt_target_invalid = (fv.parsed_mode == CAN_FAULT_MODE_CORRUPT)
                                 && (fv.cfg.target != CAN_MSG_ENGINE_STATUS);

    if (fv.mode_seen && fv.target_seen && (fv.parsed_mode != CAN_FAULT_MODE_NONE) && !corrupt_target_invalid) {
        fv.cfg.mode    = fv.parsed_mode;
        fv.cfg.base_ms = base_ms;
        *cfg = fv.cfg;
        log_print_leveled(LOG_INFO, "CAN-FAULT",
                           (fv.parsed_mode == CAN_FAULT_MODE_DROP) ? "Loaded CAN fault file (DROP mode)"
                                                                    : "Loaded CAN fault file (CORRUPT mode)");
        return true;
    }

    log_print_leveled(LOG_INFO, "CAN-FAULT", "Loaded CAN fault file (no injection)");
    return false;
}

bool can_fault_is_dropped(const CanFaultConfig *cfg, CanMessageId msg, uint32_t elapsed_ms) {
    if ((cfg->mode != CAN_FAULT_MODE_DROP) || (cfg->target != msg)) {
        return false;
    }
    return (elapsed_ms >= cfg->start_ms) && (elapsed_ms <= cfg->end_ms);
}

bool can_fault_is_corrupted(const CanFaultConfig *cfg, CanMessageId msg, uint32_t elapsed_ms) {
    if ((cfg->mode != CAN_FAULT_MODE_CORRUPT) || (cfg->target != msg)) {
        return false;
    }
    return (elapsed_ms >= cfg->start_ms) && (elapsed_ms <= cfg->end_ms);
}

/* timer_get_elapsed_msはシステム起動からの経過時間を返すため、cfg->base_msとの差分を取って
   プログラム起動からの経過時間に変換する。符号なし整数の引き算のため桁あふれをまたいでも正しい値になる */
static uint32_t can_fault_elapsed_since_start(const CanFaultConfig *cfg) {
    return timer_get_elapsed_ms() - cfg->base_ms;
}

/* 値域外に固定した送信専用の破損値（Phase22拡張バックログ）。0xFFはspeed(0〜120)・rpm(0〜6000、
   2バイトとも0xFFで65535)・temperature(25〜100)のいずれでも確実に値域外になる。実車のSNA
   （Signal Not Available、全ビット1）の慣例にも近い。実センサ値(dataの指す先)は変更しないため、
   diag.c/faultmgr.c等の物理センサ診断には影響しない */
static void build_corrupted_engine_status(VehicleSensorData *out) {
    out->speed       = 0xFFU;
    out->rpm         = 0xFFFFU;
    out->temperature = 0xFFU;
}

void can_fault_send_engine_status(const CanFaultConfig *cfg, CanBus *bus, const VehicleSensorData *data) {
    uint32_t elapsed_ms = can_fault_elapsed_since_start(cfg);

    if (can_fault_is_dropped(cfg, CAN_MSG_ENGINE_STATUS, elapsed_ms)) {
        return;   /* 意図的に送信しない(Timeoutを再現) */
    }

    if (can_fault_is_corrupted(cfg, CAN_MSG_ENGINE_STATUS, elapsed_ms)) {
        VehicleSensorData corrupted;
        build_corrupted_engine_status(&corrupted);
        can_send_engine_status(bus, &corrupted);   /* Invalid Dataを再現(送信専用コピーのみ改ざん) */
        return;
    }

    can_send_engine_status(bus, data);
}

void can_fault_send_fault_status(const CanFaultConfig *cfg, CanBus *bus, const FaultManager *fault_mgr) {
    if (can_fault_is_dropped(cfg, CAN_MSG_FAULT_STATUS, can_fault_elapsed_since_start(cfg))) {
        return;   /* 意図的に送信しない(Timeoutを再現) */
    }
    can_send_fault_status(bus, fault_mgr);
}
