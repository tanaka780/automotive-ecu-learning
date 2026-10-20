#include <stdio.h>
#include "status.h"
#include "logger.h"
#include "validate.h" /* validate_in_range で値域外を見分けるために必要（Phase27） */

/* speed / rpm / temp で同じ判定ロジックを共通化するヘルパー。
   値域外は閾値より先に見る（例：水温255は「危険」ではなく「値域外」にする、Phase27） */
static SensorLevel classify_level(SensorId id, int value, int warn, int crit) {
    if (!validate_in_range(id, value)) return LEVEL_OUT_OF_RANGE;
    if (value > crit) return LEVEL_CRITICAL;
    if (value > warn) return LEVEL_WARNING;
    return LEVEL_NORMAL;
}

/* 各センサ値をconfigの閾値と比較して状態レベル（NORMAL / WARNING / CRITICAL / OUT_OF_RANGE）を分類する */
void status_check(SensorStatus *status, const VehicleSensorData *data, const ConfigData *config) {
    status->levels[SENSOR_SPEED] = classify_level(SENSOR_SPEED, data->speed,
                                                   config->status_speed_warn, config->status_speed_crit);
    status->levels[SENSOR_RPM]   = classify_level(SENSOR_RPM, data->rpm,
                                                   config->status_rpm_warn,   config->status_rpm_crit);
    status->levels[SENSOR_TEMP]  = classify_level(SENSOR_TEMP, data->temperature,
                                                   config->status_temp_warn,  config->status_temp_crit);
}

/* 判断の仕方（値域外を先に見て、閾値と比べる）を状態判定とそろえるため、同じclassify_levelを使う */
bool status_is_overheat(uint8_t temperature, const ConfigData *config) {
    return classify_level(SENSOR_TEMP, temperature,
                          config->status_temp_warn, config->status_temp_crit) == LEVEL_CRITICAL;
}

/* 文字列長を8文字で揃えて status_print の表示幅を合わせる */
static const char *level_to_str(SensorLevel level) {
    switch (level) {
        case LEVEL_NORMAL:   return "NORMAL  ";
        case LEVEL_WARNING:  return "WARNING ";
        case LEVEL_CRITICAL: return "CRITICAL";
        case LEVEL_OUT_OF_RANGE: return "OUTRANGE";
        default:             return "UNKNOWN ";
    }
}

/* 各センサの状態レベルを1行でコンソールに出力する */
void status_print(const SensorStatus *status) {
    char line[64];   /* "Speed: NORMAL   | RPM: NORMAL   | Temp: NORMAL  " が収まるサイズ */
    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)snprintf(line, sizeof(line), "Speed: %s | RPM: %s | Temp: %s",
             level_to_str(status->levels[SENSOR_SPEED]),
             level_to_str(status->levels[SENSOR_RPM]),
             level_to_str(status->levels[SENSOR_TEMP]));
    log_print_leveled(LOG_INFO, "STATUS", line);
}
