#include <stdio.h>
#include "diag.h"
#include "logger.h"

/* センサ種別から表示名を返す（本体は下記。diag_clear_sensorのログ表示で先に使うための前方宣言） */
static const char *sensor_name(SensorId sensor);

/* DTC記録を初期値（0件、前回状態はNORMAL、状態区分はNONE、フリーズフレーム未記録）にリセットする */
void diag_init(DtcRecord *dtc) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        dtc->entries[i].sensor  = (SensorId)i;
        dtc->entries[i].count   = 0;
        dtc->entries[i].status  = DTC_NONE;
        dtc->previous.levels[i] = LEVEL_NORMAL;
    }

    dtc->freeze_frame.data.speed       = 0;
    dtc->freeze_frame.data.rpm         = 0;
    dtc->freeze_frame.data.temperature = 0;
    dtc->freeze_frame.trigger_sensor   = SENSOR_SPEED;
    dtc->freeze_frame.captured         = false;
}

/* 前回CRITICALでなく今回CRITICALになった瞬間（エッジ）だけを発生回数として記録し、
   現在CRITICAL中ならACTIVE、CRITICALから外れたらHISTORYに遷移させる。
   最初のエッジでフリーズフレーム（車両状態のスナップショット）を1件だけ記録する */
void diag_check(DtcRecord *dtc, const SensorStatus *status, const VehicleSensorData *data) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (status->levels[i] == LEVEL_CRITICAL) {
            if (dtc->previous.levels[i] != LEVEL_CRITICAL) {
                dtc->entries[i].count++;
                /* 全体で最初のCRITICAL発生時だけスナップショットを記録する（以降は上書きしない） */
                if (!dtc->freeze_frame.captured) {
                    dtc->freeze_frame.data           = *data;
                    dtc->freeze_frame.trigger_sensor = (SensorId)i;
                    dtc->freeze_frame.captured       = true;
                }
            }
            dtc->entries[i].status = DTC_ACTIVE;
        } else if (dtc->entries[i].status == DTC_ACTIVE) {
            dtc->entries[i].status = DTC_HISTORY;
        }
    }

    /* 次回比較のために今回の状態を保存する */
    dtc->previous = *status;
}

/* 診断コマンド（"clear"相当）によるクリア要求を受けて、DTC記録・フリーズフレームを初期状態に戻す */
void diag_clear(DtcRecord *dtc) {
    diag_init(dtc);
    log_print_leveled(LOG_INFO, "DTC", "Cleared");
}

/* 診断コマンド（"clear <センサ名>"相当）によるクリア要求を受けて、指定したセンサ1件分のDTCエントリだけを初期状態に戻す。
   フリーズフレームは1件しか保持しないため、その原因が指定センサと一致する場合だけ合わせて初期状態に戻し、
   原因が別センサなら保持する（例：Speedの記録をクリアしても、原因がTempのフリーズフレームは残す） */
void diag_clear_sensor(DtcRecord *dtc, SensorId sensor) {
    dtc->entries[sensor].count  = 0;
    dtc->entries[sensor].status = DTC_NONE;

    if (dtc->freeze_frame.captured && dtc->freeze_frame.trigger_sensor == sensor) {
        dtc->freeze_frame.data.speed       = 0;
        dtc->freeze_frame.data.rpm         = 0;
        dtc->freeze_frame.data.temperature = 0;
        dtc->freeze_frame.trigger_sensor   = SENSOR_SPEED;
        dtc->freeze_frame.captured         = false;
    }

    char line[32];   /* "Cleared: Speed" 相当が収まるサイズ */
    snprintf(line, sizeof(line), "Cleared: %s", sensor_name(sensor));
    log_print_leveled(LOG_INFO, "DTC", line);
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

    for (int i = 0; i < SENSOR_COUNT; i++) {
        snprintf(line, sizeof(line), "%-6s%-8s CRITICAL occurrences: %d",
                 sensor_name(dtc->entries[i].sensor),
                 dtc_status_to_str(dtc->entries[i].status),
                 (int)dtc->entries[i].count);
        log_print_leveled(LOG_INFO, "DTC", line);
    }

    if (dtc->freeze_frame.captured) {
        snprintf(line, sizeof(line), "Freeze Frame (trigger: %s) Speed:%3d km/h RPM:%4d Temp:%3d C",
                 sensor_name(dtc->freeze_frame.trigger_sensor),
                 (int)dtc->freeze_frame.data.speed,
                 (int)dtc->freeze_frame.data.rpm,
                 (int)dtc->freeze_frame.data.temperature);
        log_print_leveled(LOG_INFO, "DTC", line);
    } else {
        log_print_leveled(LOG_INFO, "DTC", "Freeze Frame: not captured (no CRITICAL occurred)");
    }
}
