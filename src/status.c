#include <stdio.h>
#include "status.h"

/* speed / rpm / temp で同じ判定ロジックを共通化するヘルパー */
static SensorLevel classify_level(int value, int warn, int crit) {
    if (value > crit) return LEVEL_CRITICAL;
    if (value > warn) return LEVEL_WARNING;
    return LEVEL_NORMAL;
}

/* 各センサ値を閾値と比較して状態レベル（NORMAL / WARNING / CRITICAL）を分類する */
void status_check(SensorStatus *status, const VehicleSensorData *data) {
    status->speed_level = classify_level(data->speed,
                                         STATUS_SPEED_WARN, STATUS_SPEED_CRIT);
    status->rpm_level   = classify_level(data->rpm,
                                         STATUS_RPM_WARN,   STATUS_RPM_CRIT);
    status->temp_level  = classify_level(data->temperature,
                                         STATUS_TEMP_WARN,  STATUS_TEMP_CRIT);
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
    printf("[STATUS] Speed: %s | RPM: %s | Temp: %s\n",
           level_to_str(status->speed_level),
           level_to_str(status->rpm_level),
           level_to_str(status->temp_level));
}
