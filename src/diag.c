#include <stdio.h>
#include "diag.h"

/* DTC記録を初期値（0件、前回状態はNORMAL）にリセットする */
void diag_init(DtcRecord *dtc) {
    dtc->speed_crit_count   = 0;
    dtc->rpm_crit_count     = 0;
    dtc->temp_crit_count    = 0;
    dtc->previous.speed_level = LEVEL_NORMAL;
    dtc->previous.rpm_level   = LEVEL_NORMAL;
    dtc->previous.temp_level  = LEVEL_NORMAL;
}

/* 前回CRITICALでなく今回CRITICALになった瞬間（エッジ）だけを記録する */
void diag_check(DtcRecord *dtc, const SensorStatus *status) {
    if (dtc->previous.speed_level != LEVEL_CRITICAL && status->speed_level == LEVEL_CRITICAL) {
        dtc->speed_crit_count++;
    }
    if (dtc->previous.rpm_level != LEVEL_CRITICAL && status->rpm_level == LEVEL_CRITICAL) {
        dtc->rpm_crit_count++;
    }
    if (dtc->previous.temp_level != LEVEL_CRITICAL && status->temp_level == LEVEL_CRITICAL) {
        dtc->temp_crit_count++;
    }

    /* 次回比較のために今回の状態を保存する */
    dtc->previous = *status;
}

/* 記録されたDTC一覧をコンソールに出力する */
void diag_print(const DtcRecord *dtc) {
    printf("[DTC] Speed CRITICAL occurrences: %d\n", (int)dtc->speed_crit_count);
    printf("[DTC] RPM   CRITICAL occurrences: %d\n", (int)dtc->rpm_crit_count);
    printf("[DTC] Temp  CRITICAL occurrences: %d\n", (int)dtc->temp_crit_count);
}
