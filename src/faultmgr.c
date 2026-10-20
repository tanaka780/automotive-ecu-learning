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
        fm->bad_count[i]    = 0;
        fm->good_count[i]   = 0;
        fm->state[i]        = FAULT_NORMAL;
        fm->out_of_range[i] = false;
    }
    /* 値域内の値をまだ受け取っていない間は、フェイルセーフ値を直前の値の代わりにする（Phase27） */
    fm->last_valid.speed       = FAULTMGR_SAFE_SPEED;
    fm->last_valid.rpm         = FAULTMGR_SAFE_RPM;
    fm->last_valid.temperature = FAULTMGR_SAFE_TEMP;
}

/* 今回の値域外のセンサを記録し、値域内のセンサは値をlast_validに覚える（Phase27）。
   VehicleSensorDataはセンサごとに型の違うフィールドを持つため、配列のループではなく1つずつ書く */
static void faultmgr_remember_valid(FaultManager *fm, const SensorStatus *status, const VehicleSensorData *raw) {
    for (int i = 0; i < (int)SENSOR_COUNT; i++) {
        fm->out_of_range[i] = (status->levels[i] == LEVEL_OUT_OF_RANGE);
    }
    if (!fm->out_of_range[SENSOR_SPEED]) {
        fm->last_valid.speed = raw->speed;
    }
    if (!fm->out_of_range[SENSOR_RPM]) {
        fm->last_valid.rpm = raw->rpm;
    }
    if (!fm->out_of_range[SENSOR_TEMP]) {
        fm->last_valid.temperature = raw->temperature;
    }
}

/* 確定へ進める「悪い」と、復帰へ進める「良い」を、センサごとに決める（Phase27）。
   水温は、値域内の高い値を本当の高温（オーバーヒート）として扱うため、値域外（断線・ショート）だけで確定する。
   復帰は値域内の値（注意・危険を含む）の継続とし、高温のままセンサだけが直った場合も復帰できるようにする。
   車速・回転数は今までどおり危険の継続で確定し、正常の継続で復帰する（値域外も悪いとして数える） */
static void faultmgr_judge(SensorId sensor, SensorLevel level, bool *is_bad, bool *is_good) {
    if (sensor == SENSOR_TEMP) {
        *is_bad  = (level == LEVEL_OUT_OF_RANGE);
        *is_good = (level != LEVEL_OUT_OF_RANGE);
    } else {
        *is_bad  = (level == LEVEL_CRITICAL) || (level == LEVEL_OUT_OF_RANGE);
        *is_good = (level == LEVEL_NORMAL);
    }
}

/* status_checkの分類結果を見て、センサ別にDebounce/Recoveryのカウントを進め、
   確定(FAULT_DEGRADED)・復帰(FAULT_NORMAL)への遷移を判定する。遷移した瞬間だけログを出す。
   連続回数の数え方自体（Debounce/Recovery）はdebounce.cに切り出しており、ここではfaultmgr_judgeによる
   「センサにとっての良し悪し」の意味づけと、遷移時のログ表示だけを担う（Phase20） */
void faultmgr_check(FaultManager *fm, const SensorStatus *status, const VehicleSensorData *raw) {
    faultmgr_remember_valid(fm, status, raw);

    for (int i = 0; i < (int)SENSOR_COUNT; i++) {
        bool was_degraded = (fm->state[i] == FAULT_DEGRADED);
        bool is_bad;
        bool is_good;
        faultmgr_judge((SensorId)i, status->levels[i], &is_bad, &is_good);

        bool now_degraded = debounce_update(was_degraded, is_bad, is_good,
                                             &fm->bad_count[i], &fm->good_count[i],
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

/* rawのセンサ値は変更せず、FAULT_DEGRADED中のセンサはフェイルセーフ値に、まだ確定していない値域外のセンサは
   直前の値域内の値に差し替えたコピーをeffectiveに作る。確定中は値域外でもフェイルセーフ値を優先するため、後に書く */
void faultmgr_apply_safe_values(const FaultManager *fm, const VehicleSensorData *raw, VehicleSensorData *effective) {
    *effective = *raw;

    /* 確定するまでの間、値域外の値（断線・ショート）を警告・統計・CAN送信へ流さない（Phase27） */
    if (fm->out_of_range[SENSOR_SPEED]) {
        effective->speed = fm->last_valid.speed;
    }
    if (fm->out_of_range[SENSOR_RPM]) {
        effective->rpm = fm->last_valid.rpm;
    }
    if (fm->out_of_range[SENSOR_TEMP]) {
        effective->temperature = fm->last_valid.temperature;
    }

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

/* 直前の値（last_valid）は、値域内なら毎回今回の値で更新しているので、そのまま判断に使える水温になる */
uint8_t faultmgr_usable_temperature(const FaultManager *fm) {
    return fm->last_valid.temperature;
}
