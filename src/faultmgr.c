#include <stdio.h>
#include "faultmgr.h"
#include "logger.h"

/* センサ種別から表示名を返す（faultmgr.c専用のログ表示ヘルパー。diag.cにも同名の役割を持つ
   static関数があるが、モジュール名を接頭辞にして区別する（MISRA 5.9）） */
static const char *faultmgr_sensor_name(SensorId sensor) {
    switch (sensor) {
        case SENSOR_SPEED: return "Speed";
        case SENSOR_RPM:   return "RPM";
        case SENSOR_TEMP:  return "Temp";
        default:           return "Unknown";
    }
}

/* 状態を初期値（FAULT_NORMAL、カウンタ0）にリセットする */
void faultmgr_init(FaultManager *fm) {
    /* SENSOR_COUNTはenum定数のため、int（i）との比較にはキャストが必要（MISRA 10.4） */
    for (int i = 0; i < (int)SENSOR_COUNT; i++) {
        fm->critical_count[i] = 0;
        fm->normal_count[i]   = 0;
        fm->state[i]          = FAULT_NORMAL;
    }
}

/* status_checkの分類結果を見て、センサ別にDebounce/Recoveryのカウントを進め、
   確定(FAULT_DEGRADED)・復帰(FAULT_NORMAL)への遷移を判定する。遷移した瞬間だけログを出す */
void faultmgr_check(FaultManager *fm, const SensorStatus *status) {
    for (int i = 0; i < (int)SENSOR_COUNT; i++) {
        if (fm->state[i] == FAULT_NORMAL) {
            if (status->levels[i] == LEVEL_CRITICAL) {
                fm->critical_count[i]++;
                if (fm->critical_count[i] >= FAULTMGR_DEBOUNCE_COUNT) {
                    fm->state[i]          = FAULT_DEGRADED;
                    fm->normal_count[i]   = 0;

                    char line[32];   /* "Degraded: Speed" が収まるサイズ */
                    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
                    (void)snprintf(line, sizeof(line), "Degraded: %s", faultmgr_sensor_name((SensorId)i));
                    log_print_leveled(LOG_ERROR, "FAULT", line);
                }
            } else {
                /* CRITICALが連続しなければ確定前とみなし、連続回数を数え直す */
                fm->critical_count[i] = 0;
            }
        } else {
            if (status->levels[i] == LEVEL_NORMAL) {
                fm->normal_count[i]++;
                if (fm->normal_count[i] >= FAULTMGR_RECOVERY_COUNT) {
                    fm->state[i]          = FAULT_NORMAL;
                    fm->critical_count[i] = 0;

                    char line[32];   /* "Recovered: Speed" が収まるサイズ */
                    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
                    (void)snprintf(line, sizeof(line), "Recovered: %s", faultmgr_sensor_name((SensorId)i));
                    log_print_leveled(LOG_INFO, "FAULT", line);
                }
            } else {
                /* NORMALが連続しなければ復帰前とみなし、連続回数を数え直す（WARNING止まりでは復帰しない） */
                fm->normal_count[i] = 0;
            }
        }
    }
}

/* rawのセンサ値は変更せず、FAULT_DEGRADED中のセンサだけフェイルセーフ値に差し替えたコピーをeffectiveに作る */
void faultmgr_apply_safe_values(const FaultManager *fm, const VehicleSensorData *raw, VehicleSensorData *effective) {
    *effective = *raw;

    if (fm->state[SENSOR_SPEED] == FAULT_DEGRADED) {
        effective->speed = FAULTMGR_SAFE_SPEED;
    }
    if (fm->state[SENSOR_RPM] == FAULT_DEGRADED) {
        effective->rpm = FAULTMGR_SAFE_RPM;
    }
    if (fm->state[SENSOR_TEMP] == FAULT_DEGRADED) {
        effective->temperature = FAULTMGR_SAFE_TEMP;
    }
}
