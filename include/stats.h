#ifndef STATS_H
#define STATS_H

#include "sensor.h"   /* VehicleSensorData を参照するために必要 */

/*
 * VehicleStats: センサ値の統計データをまとめる型
 *
 * sensor.c は「今この瞬間のセンサ値」を管理する
 * stats.c は「これまでの値の最小・最大・合計」を管理する
 * 責務が違うため、sensor.c とは別モジュールにする
 */
typedef struct {
    int speed_min;   /* 車速の最小値 [km/h] */
    int speed_max;   /* 車速の最大値 [km/h] */
    int speed_sum;   /* 車速の合計（平均計算に使う） */

    int rpm_min;     /* RPM の最小値 */
    int rpm_max;     /* RPM の最大値 */
    int rpm_sum;     /* RPM の合計 */

    int temp_min;    /* 水温の最小値 [℃] */
    int temp_max;    /* 水温の最大値 [℃] */
    int temp_sum;    /* 水温の合計 */

    int count;       /* 記録したサンプル数 */
} VehicleStats;

/*
 * 関数宣言:
 * stats_update の第2引数は const → センサ値を「読むだけ」で書き換えない
 */
void stats_init(VehicleStats *stats);
void stats_update(VehicleStats *stats, const VehicleSensorData *data);
void stats_print(const VehicleStats *stats);

#endif /* STATS_H */
