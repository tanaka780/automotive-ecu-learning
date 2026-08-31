#include <stdio.h>
#include <stdint.h>   /* UINT8_MAX / UINT16_MAX: 固定幅整数型の最大値 */
#include "stats.h"
#include "logger.h"

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
    if (data->speed < stats->speed_min) {
        stats->speed_min = data->speed;
    }
    if (data->speed > stats->speed_max) {
        stats->speed_max = data->speed;
    }
    stats->speed_sum += data->speed;

    if (data->rpm < stats->rpm_min) {
        stats->rpm_min = data->rpm;
    }
    if (data->rpm > stats->rpm_max) {
        stats->rpm_max = data->rpm;
    }
    stats->rpm_sum += data->rpm;

    if (data->temperature < stats->temp_min) {
        stats->temp_min = data->temperature;
    }
    if (data->temperature > stats->temp_max) {
        stats->temp_max = data->temperature;
    }
    stats->temp_sum += data->temperature;

    stats->count++;
}

/* count == 0 のとき sum / count は未定義動作になるため事前にチェックする */
void stats_print(const VehicleStats *stats) {
    /* countはuint8_tのため、比較対象の0もunsignedとして明示する（MISRA 10.4） */
    if (stats->count == 0U) {
        log_print("No data");
        return;
    }

    char line[64];   /* 1行ずつ組み立てて出力するため、呼び出しのたびに使い回す */

    log_print("");   /* 見出し前の空行（元のprintf("\n...")の空行部分に相当） */

    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)snprintf(line, sizeof(line), "--- Stats (%d samples) ---", (int)stats->count);
    log_print(line);

    /* 複合式（除算）を直接キャストせず、一旦変数で受けてからキャストする（MISRA 10.8） */
    uint16_t speed_avg = stats->speed_sum / stats->count;
    (void)snprintf(line, sizeof(line), "Speed: min=%3d  max=%3d  avg=%3d km/h",
             (int)stats->speed_min, (int)stats->speed_max, (int)speed_avg);
    log_print(line);

    uint32_t rpm_avg = stats->rpm_sum / stats->count;
    (void)snprintf(line, sizeof(line), "RPM  : min=%4d  max=%4d  avg=%4d",
             (int)stats->rpm_min, (int)stats->rpm_max, (int)rpm_avg);
    log_print(line);

    uint16_t temp_avg = stats->temp_sum / stats->count;
    (void)snprintf(line, sizeof(line), "Temp : min=%3d  max=%3d  avg=%3d C",
             (int)stats->temp_min, (int)stats->temp_max, (int)temp_avg);
    log_print(line);
}
