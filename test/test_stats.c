#include <stdint.h>   /* UINT8_MAX / UINT16_MAX: 固定幅整数型の最大値 */
#include "unity.h"
#include "sensor.h"
#include "stats.h"

void setUp(void) {}
void tearDown(void) {}

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

    TEST_ASSERT_TRUE_MESSAGE(stats.speed_min == UINT8_MAX, "初期化直後: speed_minは型の最大値");
    TEST_ASSERT_TRUE_MESSAGE(stats.speed_max == 0,          "初期化直後: speed_maxは0");
    TEST_ASSERT_TRUE_MESSAGE(stats.speed_sum == 0,          "初期化直後: speed_sumは0");

    TEST_ASSERT_TRUE_MESSAGE(stats.rpm_min == UINT16_MAX, "初期化直後: rpm_minは型の最大値");
    TEST_ASSERT_TRUE_MESSAGE(stats.rpm_max == 0,           "初期化直後: rpm_maxは0");
    TEST_ASSERT_TRUE_MESSAGE(stats.rpm_sum == 0,           "初期化直後: rpm_sumは0");

    TEST_ASSERT_TRUE_MESSAGE(stats.temp_min == UINT8_MAX, "初期化直後: temp_minは型の最大値");
    TEST_ASSERT_TRUE_MESSAGE(stats.temp_max == 0,          "初期化直後: temp_maxは0");
    TEST_ASSERT_TRUE_MESSAGE(stats.temp_sum == 0,          "初期化直後: temp_sumは0");

    TEST_ASSERT_TRUE_MESSAGE(stats.count == 0, "初期化直後: countは0");
}

/* 1サンプルだけ投入した場合、min=max=その値、sumも同じ値、countが1になることを確認する */
static void test_single_sample(void) {
    VehicleStats stats;
    stats_init(&stats);

    feed(&stats, 50, 3000, 60);

    TEST_ASSERT_TRUE_MESSAGE(stats.speed_min == 50 && stats.speed_max == 50, "1サンプル後: speed_min=max=50");
    TEST_ASSERT_TRUE_MESSAGE(stats.speed_sum == 50, "1サンプル後: speed_sum=50");
    TEST_ASSERT_TRUE_MESSAGE(stats.rpm_min == 3000 && stats.rpm_max == 3000, "1サンプル後: rpm_min=max=3000");
    TEST_ASSERT_TRUE_MESSAGE(stats.temp_min == 60 && stats.temp_max == 60, "1サンプル後: temp_min=max=60");
    TEST_ASSERT_TRUE_MESSAGE(stats.count == 1, "1サンプル後: countは1");
}

/* 複数サンプルにわたるmin/maxの更新・非更新を、値が下がる→中間に戻るケースで確認する */
static void test_min_max_update_across_samples(void) {
    VehicleStats stats;
    stats_init(&stats);

    feed(&stats, 50, 2000, 60);   /* Sample1: 基準値 */
    feed(&stats, 30, 4500, 40);   /* Sample2: speed/tempは下がり、rpmは上がる */

    TEST_ASSERT_TRUE_MESSAGE(stats.speed_min == 30, "Sample2後: speed_minが更新される(30)");
    TEST_ASSERT_TRUE_MESSAGE(stats.speed_max == 50, "Sample2後: speed_maxは変化しない(50のまま)");
    TEST_ASSERT_TRUE_MESSAGE(stats.rpm_min == 2000, "Sample2後: rpm_minは変化しない(2000のまま)");
    TEST_ASSERT_TRUE_MESSAGE(stats.rpm_max == 4500, "Sample2後: rpm_maxが更新される(4500)");
    TEST_ASSERT_TRUE_MESSAGE(stats.temp_min == 40, "Sample2後: temp_minが更新される(40)");
    TEST_ASSERT_TRUE_MESSAGE(stats.temp_max == 60, "Sample2後: temp_maxは変化しない(60のまま)");

    feed(&stats, 40, 3000, 50);   /* Sample3: これまでのmin/maxの中間値(更新されないはず) */

    TEST_ASSERT_TRUE_MESSAGE(stats.speed_min == 30 && stats.speed_max == 50,
          "Sample3後: speed_min/maxは変化しない(中間値のため)");
    TEST_ASSERT_TRUE_MESSAGE(stats.rpm_min == 2000 && stats.rpm_max == 4500,
          "Sample3後: rpm_min/maxは変化しない(中間値のため)");
    TEST_ASSERT_TRUE_MESSAGE(stats.temp_min == 40 && stats.temp_max == 60,
          "Sample3後: temp_min/maxは変化しない(中間値のため)");
    TEST_ASSERT_TRUE_MESSAGE(stats.count == 3, "Sample3後: countは3");
}

/* stats_printと同じ計算式(sum/countの整数除算)で、平均が期待通りになることを確認する */
static void test_average_calculation(void) {
    VehicleStats stats;
    stats_init(&stats);

    feed(&stats, 10, 1000, 30);
    feed(&stats, 20, 2000, 40);
    feed(&stats, 30, 4000, 50);   /* rpm合計7000 → 3で割ると2333.33...(整数除算で切り捨て) */

    TEST_ASSERT_TRUE_MESSAGE((stats.speed_sum / stats.count) == 20, "平均: speed_sum/countが20");
    TEST_ASSERT_TRUE_MESSAGE((stats.rpm_sum / stats.count) == 2333, "平均: rpm_sum/countが2333(整数除算で切り捨て)");
    TEST_ASSERT_TRUE_MESSAGE((stats.temp_sum / stats.count) == 40, "平均: temp_sum/countが40");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init_values);
    RUN_TEST(test_single_sample);
    RUN_TEST(test_min_max_update_across_samples);
    RUN_TEST(test_average_calculation);

    return UNITY_END();
}
