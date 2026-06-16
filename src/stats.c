#include <stdio.h>
#include <limits.h>   /* INT_MAX: int 型が取れる最大値 */
#include "stats.h"

/*
 * stats_init: 統計データを初期状態に設定する
 *
 * 最小値の初期値に INT_MAX を使う理由:
 *   最初のサンプルが何であっても「今の値 < INT_MAX」が必ず成立する
 *   → 最初のサンプルで確実に最小値が更新される
 *   例: speed_min = INT_MAX のとき speed=60 が来ると
 *       60 < INT_MAX が成立し、speed_min = 60 に更新される
 */
void stats_init(VehicleStats *stats) {
    stats->speed_min = INT_MAX;
    stats->speed_max = 0;
    stats->speed_sum = 0;

    stats->rpm_min   = INT_MAX;
    stats->rpm_max   = 0;
    stats->rpm_sum   = 0;

    stats->temp_min  = INT_MAX;
    stats->temp_max  = 0;
    stats->temp_sum  = 0;

    stats->count = 0;
}

/*
 * stats_update: センサ値を1サンプル分、統計に反映する
 *
 * const VehicleSensorData *data: センサ値を「読むだけ」で書き換えない
 * stats は自分のメンバー（speed_min など）を書き換えるが、
 * data の中身（センサ値）には触らない
 */
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

/*
 * stats_print: 統計結果をコンソールに表示する
 *
 * 平均は sum / count で計算する（整数除算のため小数点以下は切り捨て）
 * count == 0 のとき除算すると未定義動作になるため、事前にチェックする
 */
void stats_print(const VehicleStats *stats) {
    if (stats->count == 0) {
        printf("No data\n");
        return;
    }
    printf("\n--- Stats (%d samples) ---\n", stats->count);
    printf("Speed: min=%3d  max=%3d  avg=%3d km/h\n",
           stats->speed_min, stats->speed_max, stats->speed_sum / stats->count);
    printf("RPM  : min=%4d  max=%4d  avg=%4d\n",
           stats->rpm_min, stats->rpm_max, stats->rpm_sum / stats->count);
    printf("Temp : min=%3d  max=%3d  avg=%3d C\n",
           stats->temp_min, stats->temp_max, stats->temp_sum / stats->count);
}
