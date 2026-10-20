#include <stdio.h>
#include "diag.h"
#include "logger.h"
#include "dtc_status.h"

/* センサ種別・記録の種類から表示名を返す（本体は下記。diag_clear_sensorのログ表示で先に使うための前方宣言） */
static const char *sensor_name(SensorId sensor);
static const char *dtc_name(DtcId id);

/* DTC記録を初期値（0件、前回は異常なし、状態区分はNONE、フリーズフレーム未記録）にリセットする */
void diag_init(DtcRecord *dtc) {
    /* DTC_ID_COUNTはenum定数のため、int（i）との比較にはキャストが必要（MISRA 10.4） */
    for (int i = 0; i < (int)DTC_ID_COUNT; i++) {
        dtc->entries[i].id     = (DtcId)i;
        dtc->entries[i].count  = 0;
        dtc->entries[i].status = DTC_NONE;
        dtc->previous_bad[i]   = false;
    }

    dtc->freeze_frame.data.speed       = 0;
    dtc->freeze_frame.data.rpm         = 0;
    dtc->freeze_frame.data.temperature = 0;
    dtc->freeze_frame.trigger          = DTC_ID_SPEED;
    dtc->freeze_frame.captured         = false;
}

/* 車速・回転数で異常とする段階：危険（CRITICAL）か値域外（OUT_OF_RANGE、Phase27）。
   危険と値域外の間を移る場合は異常が続いているため、数え直さない */
static bool diag_is_bad(SensorLevel level) {
    return (level == LEVEL_CRITICAL) || (level == LEVEL_OUT_OF_RANGE);
}

/* 記録の種類ごとに、今回異常かを決める（Phase28）。水温は、センサの故障（値域外）と
   オーバーヒート（判断に使える水温が危険）を別の記録にする */
static bool diag_dtc_is_bad(DtcId id, const SensorStatus *status, bool overheat) {
    switch (id) {
        case DTC_ID_SPEED:       return diag_is_bad(status->levels[SENSOR_SPEED]);
        case DTC_ID_RPM:         return diag_is_bad(status->levels[SENSOR_RPM]);
        case DTC_ID_TEMP_SENSOR: return (status->levels[SENSOR_TEMP] == LEVEL_OUT_OF_RANGE);
        case DTC_ID_OVERHEAT:    return overheat;
        default:                 return false;
    }
}

/* 前回異常でなく今回異常になった瞬間（エッジ）だけを発生回数として記録し、
   現在異常中ならACTIVE、異常から外れたらHISTORYに遷移させる（発生回数・状態区分の更新自体は
   dtc_status_updateに委譲する、Phase21）。最初のエッジでフリーズフレーム（車両状態のスナップショット）
   を1件だけ記録する処理は、物理センサ固有のためdiag.cに残す */
void diag_check(DtcRecord *dtc, const SensorStatus *status, const VehicleSensorData *data, bool overheat) {
    for (int i = 0; i < (int)DTC_ID_COUNT; i++) {
        bool was_bad = dtc->previous_bad[i];
        bool is_bad  = diag_dtc_is_bad((DtcId)i, status, overheat);

        /* 全体で最初の異常の発生時だけスナップショットを記録する（以降は上書きしない） */
        if (is_bad && (!was_bad) && (!dtc->freeze_frame.captured)) {
            dtc->freeze_frame.data     = *data;
            dtc->freeze_frame.trigger  = (DtcId)i;
            dtc->freeze_frame.captured = true;
        }

        dtc_status_update(&dtc->entries[i].count, &dtc->entries[i].status, was_bad, is_bad);
        /* 次回比較のために今回の異常の有無を保存する */
        dtc->previous_bad[i] = is_bad;
    }
}

/* 診断コマンド（"clear"相当）によるクリア要求を受けて、DTC記録・フリーズフレームを初期状態に戻す */
void diag_clear(DtcRecord *dtc) {
    diag_init(dtc);
    log_print_leveled(LOG_INFO, "DTC", "Cleared");
}

/* 記録の種類が、どのセンサの記録かを返す（センサ単位のクリアで使う、Phase28）。
   水温センサの故障とオーバーヒートは、どちらも水温の記録 */
static SensorId diag_dtc_sensor(DtcId id) {
    switch (id) {
        case DTC_ID_SPEED: return SENSOR_SPEED;
        case DTC_ID_RPM:   return SENSOR_RPM;
        default:           return SENSOR_TEMP;
    }
}

/* 診断コマンド（"clear <センサ名>"相当）によるクリア要求を受けて、指定したセンサの記録だけを初期状態に戻す。
   フリーズフレームは1件しか保持しないため、その原因が消した記録の場合だけ合わせて初期状態に戻し、
   原因が別の記録なら保持する（例：Speedの記録をクリアしても、原因がOverheatのフリーズフレームは残す） */
void diag_clear_sensor(DtcRecord *dtc, SensorId sensor) {
    for (int i = 0; i < (int)DTC_ID_COUNT; i++) {
        if (diag_dtc_sensor((DtcId)i) == sensor) {
            dtc->entries[i].count  = 0;
            dtc->entries[i].status = DTC_NONE;
        }
    }

    /* &&の左右それぞれに括弧を付け、演算子の優先順位を明示する（MISRA 12.1） */
    if ((dtc->freeze_frame.captured) && (diag_dtc_sensor(dtc->freeze_frame.trigger) == sensor)) {
        dtc->freeze_frame.data.speed       = 0;
        dtc->freeze_frame.data.rpm         = 0;
        dtc->freeze_frame.data.temperature = 0;
        dtc->freeze_frame.trigger          = DTC_ID_SPEED;
        dtc->freeze_frame.captured         = false;
    }

    char line[32];   /* "Cleared: Speed" 相当が収まるサイズ */
    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)snprintf(line, sizeof(line), "Cleared: %s", sensor_name(sensor));
    log_print_leveled(LOG_INFO, "DTC", line);
}

/* センサ種別から表示名を返す（diag_clear_sensorの表示ヘルパー） */
static const char *sensor_name(SensorId sensor) {
    switch (sensor) {
        case SENSOR_SPEED: return "Speed";
        case SENSOR_RPM:   return "RPM";
        case SENSOR_TEMP:  return "Temp";
        default:           return "Unknown";
    }
}

/* 記録の種類から表示名を返す（diag_printの表示ヘルパー、Phase28） */
static const char *dtc_name(DtcId id) {
    switch (id) {
        case DTC_ID_SPEED:       return "Speed";
        case DTC_ID_RPM:         return "RPM";
        case DTC_ID_TEMP_SENSOR: return "TempSensor";
        case DTC_ID_OVERHEAT:    return "Overheat";
        default:                 return "Unknown";
    }
}

/* DTC状態区分を表示名に変換する（diag_print専用の表示ヘルパー） */
static const char *dtc_status_to_str(DtcStatus status) {
    switch (status) {
        case DTC_NONE:    return "NONE   ";
        case DTC_ACTIVE:  return "ACTIVE ";
        case DTC_HISTORY: return "HISTORY";
        default:          return "UNKNOWN";
    }
}

/* 記録されたDTC一覧をコンソールに出力する */
void diag_print(const DtcRecord *dtc) {
    /* Freeze Frame最悪ケース(64文字)に余裕を持たせたサイズ。他モジュールの64との慣習を保つため2のべき乗にする */
    char line[128];

    for (int i = 0; i < (int)DTC_ID_COUNT; i++) {
        /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
        (void)snprintf(line, sizeof(line), "%-11s%-8s occurrences: %d",
                 dtc_name(dtc->entries[i].id),
                 dtc_status_to_str(dtc->entries[i].status),
                 (int)dtc->entries[i].count);
        log_print_leveled(LOG_INFO, "DTC", line);
    }

    if (dtc->freeze_frame.captured) {
        /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
        (void)snprintf(line, sizeof(line), "Freeze Frame (trigger: %s) Speed:%3d km/h RPM:%4d Temp:%3d C",
                 dtc_name(dtc->freeze_frame.trigger),
                 (int)dtc->freeze_frame.data.speed,
                 (int)dtc->freeze_frame.data.rpm,
                 (int)dtc->freeze_frame.data.temperature);
        log_print_leveled(LOG_INFO, "DTC", line);
    } else {
        log_print_leveled(LOG_INFO, "DTC", "Freeze Frame: not captured (no DTC occurred)");
    }
}
