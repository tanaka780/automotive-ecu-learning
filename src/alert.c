#include <stdio.h>
#include "alert.h"
#include "logger.h"

/* 各センサ値を閾値と比較し、超えていれば警告を表示する */
void alert_check(const VehicleSensorData *data) {
    char line[64];   /* 各if内で使い回す */

    /* else if にしない: 複数項目が同時に閾値を超えてもそれぞれ警告を出すため */
    if (data->speed > ALERT_SPEED_MAX) {
        snprintf(line, sizeof(line), "Speed : %3d km/h  (limit: %d km/h)",
                 data->speed, ALERT_SPEED_MAX);
        log_print_tagged("ALERT", line);
    }
    if (data->rpm > ALERT_RPM_MAX) {
        snprintf(line, sizeof(line), "RPM   : %4d       (limit: %d)",
                 data->rpm, ALERT_RPM_MAX);
        log_print_tagged("ALERT", line);
    }
    if (data->temperature > ALERT_TEMP_MAX) {
        snprintf(line, sizeof(line), "Temp  : %3d C      (limit: %d C)",
                 data->temperature, ALERT_TEMP_MAX);
        log_print_tagged("ALERT", line);
    }
}
