#include <stdio.h>
#include <stdbool.h>  /* bool を使うために必要 */
#include "faultmgr.h"
#include "logger.h"
#include "debounce.h"

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
   確定(FAULT_DEGRADED)・復帰(FAULT_NORMAL)への遷移を判定する。遷移した瞬間だけログを出す。
   連続回数の数え方自体（Debounce/Recovery）はdebounce.cに切り出しており、ここではCRITICAL/NORMAL
   という「センサにとっての良し悪し」の意味づけと、遷移時のログ表示だけを担う（Phase20） */
void faultmgr_check(FaultManager *fm, const SensorStatus *status) {
    for (int i = 0; i < (int)SENSOR_COUNT; i++) {
        bool was_degraded = (fm->state[i] == FAULT_DEGRADED);
        bool is_critical  = (status->levels[i] == LEVEL_CRITICAL);
        bool is_normal    = (status->levels[i] == LEVEL_NORMAL);

        bool now_degraded = debounce_update(was_degraded, is_critical, is_normal,
                                             &fm->critical_count[i], &fm->normal_count[i],
                                             FAULTMGR_DEBOUNCE_COUNT, FAULTMGR_RECOVERY_COUNT);

        if (now_degraded != was_degraded) {
            fm->state[i] = now_degraded ? FAULT_DEGRADED : FAULT_NORMAL;

            char line[32];   /* "Degraded: Speed" / "Recovered: Speed" が収まるサイズ */
            if (now_degraded) {
                /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
                (void)snprintf(line, sizeof(line), "Degraded: %s", faultmgr_sensor_name((SensorId)i));
                log_print_leveled(LOG_ERROR, "FAULT", line);
            } else {
                (void)snprintf(line, sizeof(line), "Recovered: %s", faultmgr_sensor_name((SensorId)i));
                log_print_leveled(LOG_INFO, "FAULT", line);
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
