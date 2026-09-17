#include <stdio.h>
#include <stdbool.h>
#include "can_diag.h"
#include "logger.h"

/* メッセージ種別から表示名を返す（can_diag_print専用の表示ヘルパー） */
static const char *can_msg_name(CanMessageId msg) {
    switch (msg) {
        case CAN_MSG_ENGINE_STATUS: return "EngineStatus";
        case CAN_MSG_FAULT_STATUS:  return "FaultStatus";
        default:                    return "Unknown";
    }
}

/* DTC状態区分を表示名に変換する（can_diag_print専用の表示ヘルパー。diag.cの同名static関数と
   識別子が重複していたため、Phase14のMISRA 5.9対応（config_apply_line/fixture_apply_line）と
   同じ基準でモジュール名を接頭辞にした） */
static const char *can_diag_dtc_status_to_str(DtcStatus status) {
    switch (status) {
        case DTC_NONE:    return "NONE   ";
        case DTC_ACTIVE:  return "ACTIVE ";
        case DTC_HISTORY: return "HISTORY";
        default:          return "UNKNOWN";
    }
}

/* 記録を初期値（0件、状態区分はDTC_NONE）にリセットする */
void can_diag_init(CanDtcRecord *rec) {
    for (int i = 0; i < (int)CAN_MSG_COUNT; i++) {
        rec->count[i]  = 0U;
        rec->status[i] = DTC_NONE;
    }
}

/* CanLinkStateの前回/今回比較をwas_bad/is_badに変換し、判定自体はdtc_status_updateに委譲する */
void can_diag_check(CanDtcRecord *rec, CanMessageId msg, CanLinkState previous, CanLinkState current) {
    bool was_bad = (previous == CAN_LINK_LOST);
    bool is_bad  = (current  == CAN_LINK_LOST);

    dtc_status_update(&rec->count[msg], &rec->status[msg], was_bad, is_bad);
}

/* 記録された内容をコンソールに出力する */
void can_diag_print(const CanDtcRecord *rec) {
    char line[64];   /* "EngineStatus  HISTORY link lost occurrences: 255" が収まるサイズ */

    for (int i = 0; i < (int)CAN_MSG_COUNT; i++) {
        /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
        (void)snprintf(line, sizeof(line), "%-14s%-8s link lost occurrences: %d",
                        can_msg_name((CanMessageId)i),
                        can_diag_dtc_status_to_str(rec->status[i]),
                        (int)rec->count[i]);
        log_print_leveled(LOG_INFO, "CAN-DTC", line);
    }
}
