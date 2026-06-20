#include <stdio.h>
#include "alert.h"

/* 各センサ値を閾値と比較し、超えていれば警告を表示する */
void alert_check(const VehicleSensorData *data) {
    /* else if にしない: 複数項目が同時に閾値を超えてもそれぞれ警告を出すため */
    if (data->speed > ALERT_SPEED_MAX) {
        printf("[ALERT] Speed : %3d km/h  (limit: %d km/h)\n",
               data->speed, ALERT_SPEED_MAX);
    }
    if (data->rpm > ALERT_RPM_MAX) {
        printf("[ALERT] RPM   : %4d       (limit: %d)\n",
               data->rpm, ALERT_RPM_MAX);
    }
    if (data->temperature > ALERT_TEMP_MAX) {
        printf("[ALERT] Temp  : %3d C      (limit: %d C)\n",
               data->temperature, ALERT_TEMP_MAX);
    }
}
