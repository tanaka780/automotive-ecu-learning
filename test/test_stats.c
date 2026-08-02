#include <stdint.h>   /* UINT8_MAX / UINT16_MAX: 固定幅整数型の最大値 */
#include "sensor.h"
#include "stats.h"
#include "test_common.h"

/* stats.cはDtcRecordに依存しないため、test_common.cのtest_feed(DtcRecord前提)は使わず、
   このファイル専用の投入ヘルパーをローカルに定義する */
static void feed(VehicleStats *stats, uint8_t speed, uint16_t rpm, uint8_t temperature) {
    VehicleSensorData d;
    d.speed       = speed;
    d.rpm         = rpm;
    d.temperature = temperature;
    stats_update(stats, &d);
}

/* stats_initの直後、min側が型の最大値、max/sum/countが0になっていることを確認する */
static void test_init_values(void) {
    VehicleStats stats;
    stats_init(&stats);

    test_check("初期化直後: speed_minは型の最大値", stats.speed_min == UINT8_MAX);
    test_check("初期化直後: speed_maxは0", stats.speed_max == 0);
    test_check("初期化直後: speed_sumは0", stats.speed_sum == 0);

    test_check("初期化直後: rpm_minは型の最大値", stats.rpm_min == UINT16_MAX);
    test_check("初期化直後: rpm_maxは0", stats.rpm_max == 0);
    test_check("初期化直後: rpm_sumは0", stats.rpm_sum == 0);

    test_check("初期化直後: temp_minは型の最大値", stats.temp_min == UINT8_MAX);
    test_check("初期化直後: temp_maxは0", stats.temp_max == 0);
    test_check("初期化直後: temp_sumは0", stats.temp_sum == 0);

    test_check("初期化直後: countは0", stats.count == 0);
}

/* 1サンプルだけ投入した場合、min=max=その値、sumも同じ値、countが1になることを確認する */
static void test_single_sample(void) {
    VehicleStats stats;
    stats_init(&stats);

    feed(&stats, 50, 3000, 60);

    test_check("1サンプル後: speed_min=max=50", stats.speed_min == 50 && stats.speed_max == 50);
    test_check("1サンプル後: speed_sum=50", stats.speed_sum == 50);
    test_check("1サンプル後: rpm_min=max=3000", stats.rpm_min == 3000 && stats.rpm_max == 3000);
    test_check("1サンプル後: temp_min=max=60", stats.temp_min == 60 && stats.temp_max == 60);
    test_check("1サンプル後: countは1", stats.count == 1);
}

/* 複数サンプルにわたるmin/maxの更新・非更新を、値が下がる→中間に戻るケースで確認する */
static void test_min_max_update_across_samples(void) {
    VehicleStats stats;
    stats_init(&stats);

    feed(&stats, 50, 2000, 60);   /* Sample1: 基準値 */
    feed(&stats, 30, 4500, 40);   /* Sample2: speed/tempは下がり、rpmは上がる */

    test_check("Sample2後: speed_minが更新される(30)", stats.speed_min == 30);
    test_check("Sample2後: speed_maxは変化しない(50のまま)", stats.speed_max == 50);
    test_check("Sample2後: rpm_minは変化しない(2000のまま)", stats.rpm_min == 2000);
    test_check("Sample2後: rpm_maxが更新される(4500)", stats.rpm_max == 4500);
    test_check("Sample2後: temp_minが更新される(40)", stats.temp_min == 40);
    test_check("Sample2後: temp_maxは変化しない(60のまま)", stats.temp_max == 60);

    feed(&stats, 40, 3000, 50);   /* Sample3: これまでのmin/maxの中間値(更新されないはず) */

    test_check("Sample3後: speed_min/maxは変化しない(中間値のため)",
          stats.speed_min == 30 && stats.speed_max == 50);
    test_check("Sample3後: rpm_min/maxは変化しない(中間値のため)",
          stats.rpm_min == 2000 && stats.rpm_max == 4500);
    test_check("Sample3後: temp_min/maxは変化しない(中間値のため)",
          stats.temp_min == 40 && stats.temp_max == 60);
    test_check("Sample3後: countは3", stats.count == 3);
}

/* stats_printと同じ計算式(sum/countの整数除算)で、平均が期待通りになることを確認する */
static void test_average_calculation(void) {
    VehicleStats stats;
    stats_init(&stats);

    feed(&stats, 10, 1000, 30);
    feed(&stats, 20, 2000, 40);
    feed(&stats, 30, 4000, 50);   /* rpm合計7000 → 3で割ると2333.33...(整数除算で切り捨て) */

    test_check("平均: speed_sum/countが20", (stats.speed_sum / stats.count) == 20);
    test_check("平均: rpm_sum/countが2333(整数除算で切り捨て)", (stats.rpm_sum / stats.count) == 2333);
    test_check("平均: temp_sum/countが40", (stats.temp_sum / stats.count) == 40);
}

int main(void) {
    test_init_values();
    test_single_sample();
    test_min_max_update_across_samples();
    test_average_calculation();

    return test_summary();
}
