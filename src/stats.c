#include <stdio.h>
#include <stdint.h>   /* UINT8_MAX / UINT16_MAX: 固定幅整数型の最大値 */
#include "stats.h"

/* 最小値を型の最大値で初期化: 最初のサンプルで必ず更新されることを保証するため */
void stats_init(VehicleStats *stats) {
    stats->speed_min = UINT8_MAX;
    stats->speed_max = 0;
    stats->speed_sum = 0;

    stats->rpm_min   = UINT16_MAX;
    stats->rpm_max   = 0;
    stats->rpm_sum   = 0;

    stats->temp_min  = UINT8_MAX;
    stats->temp_max  = 0;
    stats->temp_sum  = 0;

    stats->count = 0;
}

/* 今回のサンプル値で min / max / sum と count を更新する */
void stats_update(VehicleStats *stats, const VehicleSensorData *data) {
    if (data->speed < stats->speed_min) stats->speed_min = data->speed;
    if (data->speed > stats->speed_max) stats->speed_max = data->speed;
    stats->speed_sum += data->speed;

    if (data->rpm < stats->rpm_min) stats->rpm_min = data->rpm;
    if (data->rpm > stats->rpm_max) stats->rpm_max = data->rpm;
    stats->rpm_sum += data->rpm;

    if (data->temperature < stats->temp_min) stats->temp_min = data->temperature;
    if (data->temperature > stats->temp_max) stats->temp_max = data->temperature;
    stats->temp_sum += data->temperature;

    stats->count++;
}

/* count == 0 のとき sum / count は未定義動作になるため事前にチェックする */
void stats_print(const VehicleStats *stats) {
    if (stats->count == 0) {
        printf("No data\n");
        return;
    }
    printf("\n--- Stats (%d samples) ---\n", (int)stats->count);
    printf("Speed: min=%3d  max=%3d  avg=%3d km/h\n",
           (int)stats->speed_min, (int)stats->speed_max,
           (int)(stats->speed_sum / stats->count));
    printf("RPM  : min=%4d  max=%4d  avg=%4d\n",
           (int)stats->rpm_min, (int)stats->rpm_max,
           (int)(stats->rpm_sum / stats->count));
    printf("Temp : min=%3d  max=%3d  avg=%3d C\n",
           (int)stats->temp_min, (int)stats->temp_max,
           (int)(stats->temp_sum / stats->count));
}
