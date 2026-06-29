#include <stdio.h>
#include "diag.h"

/* DTC記録を初期値（0件、前回状態はNORMAL）にリセットする */
void diag_init(DtcRecord *dtc) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        dtc->entries[i].sensor = (SensorId)i;
        dtc->entries[i].count  = 0;
    }
    dtc->previous.speed_level = LEVEL_NORMAL;
    dtc->previous.rpm_level   = LEVEL_NORMAL;
    dtc->previous.temp_level  = LEVEL_NORMAL;
}

/* 前回CRITICALでなく今回CRITICALになった瞬間（エッジ）だけを記録する */
void diag_check(DtcRecord *dtc, const SensorStatus *status) {
    /* status.cを変更せずに配列ループで扱うため、個別フィールドを一時配列へ詰め替える */
    const SensorLevel current[SENSOR_COUNT] = {
        status->speed_level,
        status->rpm_level,
        status->temp_level
    };
    const SensorLevel previous[SENSOR_COUNT] = {
        dtc->previous.speed_level,
        dtc->previous.rpm_level,
        dtc->previous.temp_level
    };

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (previous[i] != LEVEL_CRITICAL && current[i] == LEVEL_CRITICAL) {
            dtc->entries[i].count++;
        }
    }

    /* 次回比較のために今回の状態を保存する */
    dtc->previous = *status;
}

/* センサ種別から表示名を返す（diag_print専用の表示ヘルパー） */
static const char *sensor_name(SensorId sensor) {
    switch (sensor) {
        case SENSOR_SPEED: return "Speed";
        case SENSOR_RPM:   return "RPM";
        case SENSOR_TEMP:  return "Temp";
        default:           return "Unknown";
    }
}

/* 記録されたDTC一覧をコンソールに出力する */
void diag_print(const DtcRecord *dtc) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        printf("[DTC] %-6sCRITICAL occurrences: %d\n",
               sensor_name(dtc->entries[i].sensor),
               (int)dtc->entries[i].count);
    }
}
