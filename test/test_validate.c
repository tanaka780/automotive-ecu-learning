#include "test_common.h"
#include "validate.h"

/* speed（0〜120）の境界値・範囲外を確認する */
static void test_speed_range(void) {
    test_check("speed: 下限0は範囲内", validate_in_range(SENSOR_SPEED, 0));
    test_check("speed: 上限120は範囲内", validate_in_range(SENSOR_SPEED, 120));
    test_check("speed: 中間値60は範囲内", validate_in_range(SENSOR_SPEED, 60));
    test_check("speed: 下限未満-1は範囲外", !validate_in_range(SENSOR_SPEED, -1));
    test_check("speed: 上限超過121は範囲外", !validate_in_range(SENSOR_SPEED, 121));
}

/* rpm（0〜6000）の境界値・範囲外を確認する */
static void test_rpm_range(void) {
    test_check("rpm: 下限0は範囲内", validate_in_range(SENSOR_RPM, 0));
    test_check("rpm: 上限6000は範囲内", validate_in_range(SENSOR_RPM, 6000));
    test_check("rpm: 中間値3000は範囲内", validate_in_range(SENSOR_RPM, 3000));
    test_check("rpm: 下限未満-1は範囲外", !validate_in_range(SENSOR_RPM, -1));
    test_check("rpm: 上限超過6001は範囲外", !validate_in_range(SENSOR_RPM, 6001));
}

/* temperature（25〜100）の境界値・範囲外を確認する */
static void test_temp_range(void) {
    test_check("temp: 下限25は範囲内", validate_in_range(SENSOR_TEMP, 25));
    test_check("temp: 上限100は範囲内", validate_in_range(SENSOR_TEMP, 100));
    test_check("temp: 中間値60は範囲内", validate_in_range(SENSOR_TEMP, 60));
    test_check("temp: 下限未満24は範囲外", !validate_in_range(SENSOR_TEMP, 24));
    test_check("temp: 上限超過101は範囲外", !validate_in_range(SENSOR_TEMP, 101));
}

/* SensorIdとして不正な値（SENSOR_COUNT以上）を渡した場合、常に範囲外になることを確認する */
static void test_invalid_sensor_id(void) {
    test_check("不正なSensorId: SENSOR_COUNTを渡すと範囲外", !validate_in_range(SENSOR_COUNT, 50));
}

/* LOG_LEVEL（0〜2）の境界値・範囲外を確認する */
static void test_log_level_range(void) {
    test_check("log_level: 下限0(LOG_INFO)は範囲内", validate_log_level(0));
    test_check("log_level: 上限2(LOG_ERROR)は範囲内", validate_log_level(2));
    test_check("log_level: 中間値1(LOG_WARNING)は範囲内", validate_log_level(1));
    test_check("log_level: 下限未満-1は範囲外", !validate_log_level(-1));
    test_check("log_level: 上限超過3は範囲外", !validate_log_level(3));
}

int main(void) {
    test_speed_range();
    test_rpm_range();
    test_temp_range();
    test_invalid_sensor_id();
    test_log_level_range();

    return test_summary();
}
