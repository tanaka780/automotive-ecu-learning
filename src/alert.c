#include <stdio.h>
#include "alert.h"

/*
 * alert_check: 各センサ値を閾値と比較し、超えていれば警告を表示する
 *
 * const VehicleSensorData *data: センサ値を読むだけで書き換えない
 *
 * alert.c の責務は「閾値チェックと警告出力」のみ
 * センサ値の更新は sensor.c、統計の記録は stats.c が担当する
 * この関数は何も書き換えず、表示だけを行う
 */
void alert_check(const VehicleSensorData *data) {
    /*
     * 各項目を独立した if で確認する
     * else if にしないのは、複数の項目が同時に閾値を超える場合も
     * それぞれ警告を出す必要があるため
     */
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
