#include <stdio.h>
#include "alert.h"
#include "logger.h"

/* 各センサ値をconfigの閾値と比較し、超えていれば警告を表示する */
void alert_check(const VehicleSensorData *data, const ConfigData *config) {
    char line[64];   /* 各if内で使い回す */

    /* else if にしない: 複数項目が同時に閾値を超えてもそれぞれ警告を出すため */
    if (data->speed > config->alert_speed_max) {
        snprintf(line, sizeof(line), "Speed : %3d km/h  (limit: %d km/h)",
                 data->speed, config->alert_speed_max);
        log_print_leveled(LOG_WARNING, "ALERT", line);
    }
    if (data->rpm > config->alert_rpm_max) {
        snprintf(line, sizeof(line), "RPM   : %4d       (limit: %d)",
                 data->rpm, config->alert_rpm_max);
        log_print_leveled(LOG_WARNING, "ALERT", line);
    }
    if (data->temperature > config->alert_temp_max) {
        snprintf(line, sizeof(line), "Temp  : %3d C      (limit: %d C)",
                 data->temperature, config->alert_temp_max);
        log_print_leveled(LOG_WARNING, "ALERT", line);
    }
}
