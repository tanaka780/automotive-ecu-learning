#include <stdio.h>
#include "status.h"
#include "logger.h"

/* speed / rpm / temp で同じ判定ロジックを共通化するヘルパー */
static SensorLevel classify_level(int value, int warn, int crit) {
    if (value > crit) return LEVEL_CRITICAL;
    if (value > warn) return LEVEL_WARNING;
    return LEVEL_NORMAL;
}

/* 各センサ値をconfigの閾値と比較して状態レベル（NORMAL / WARNING / CRITICAL）を分類する */
void status_check(SensorStatus *status, const VehicleSensorData *data, const ConfigData *config) {
    status->levels[SENSOR_SPEED] = classify_level(data->speed,
                                                   config->status_speed_warn, config->status_speed_crit);
    status->levels[SENSOR_RPM]   = classify_level(data->rpm,
                                                   config->status_rpm_warn,   config->status_rpm_crit);
    status->levels[SENSOR_TEMP]  = classify_level(data->temperature,
                                                   config->status_temp_warn,  config->status_temp_crit);
}

/* 文字列長を8文字で揃えて status_print の表示幅を合わせる */
static const char *level_to_str(SensorLevel level) {
    switch (level) {
        case LEVEL_NORMAL:   return "NORMAL  ";
        case LEVEL_WARNING:  return "WARNING ";
        case LEVEL_CRITICAL: return "CRITICAL";
        default:             return "UNKNOWN ";
    }
}

/* 各センサの状態レベルを1行でコンソールに出力する */
void status_print(const SensorStatus *status) {
    char line[64];   /* "Speed: NORMAL   | RPM: NORMAL   | Temp: NORMAL  " が収まるサイズ */
    snprintf(line, sizeof(line), "Speed: %s | RPM: %s | Temp: %s",
             level_to_str(status->levels[SENSOR_SPEED]),
             level_to_str(status->levels[SENSOR_RPM]),
             level_to_str(status->levels[SENSOR_TEMP]));
    log_print_tagged("STATUS", line);
}
