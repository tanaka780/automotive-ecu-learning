#include "unity.h"
#include "validate.h"

/* Unityが各テスト関数の前後に呼ぶ。今回は準備・後片付けの対象が無いため空のまま */
void setUp(void) {}
void tearDown(void) {}

/* speed（0〜120）の境界値・範囲外を確認する */
static void test_speed_range(void) {
    TEST_ASSERT_TRUE_MESSAGE(validate_in_range(SENSOR_SPEED, 0),   "speed: 下限0は範囲内");
    TEST_ASSERT_TRUE_MESSAGE(validate_in_range(SENSOR_SPEED, 120), "speed: 上限120は範囲内");
    TEST_ASSERT_TRUE_MESSAGE(validate_in_range(SENSOR_SPEED, 60),  "speed: 中間値60は範囲内");
    TEST_ASSERT_FALSE_MESSAGE(validate_in_range(SENSOR_SPEED, -1),  "speed: 下限未満-1は範囲外");
    TEST_ASSERT_FALSE_MESSAGE(validate_in_range(SENSOR_SPEED, 121), "speed: 上限超過121は範囲外");
}

/* rpm（0〜6000）の境界値・範囲外を確認する */
static void test_rpm_range(void) {
    TEST_ASSERT_TRUE_MESSAGE(validate_in_range(SENSOR_RPM, 0),    "rpm: 下限0は範囲内");
    TEST_ASSERT_TRUE_MESSAGE(validate_in_range(SENSOR_RPM, 6000), "rpm: 上限6000は範囲内");
    TEST_ASSERT_TRUE_MESSAGE(validate_in_range(SENSOR_RPM, 3000), "rpm: 中間値3000は範囲内");
    TEST_ASSERT_FALSE_MESSAGE(validate_in_range(SENSOR_RPM, -1),   "rpm: 下限未満-1は範囲外");
    TEST_ASSERT_FALSE_MESSAGE(validate_in_range(SENSOR_RPM, 6001), "rpm: 上限超過6001は範囲外");
}

/* temperature（25〜100）の境界値・範囲外を確認する */
static void test_temp_range(void) {
    TEST_ASSERT_TRUE_MESSAGE(validate_in_range(SENSOR_TEMP, 25),  "temp: 下限25は範囲内");
    TEST_ASSERT_TRUE_MESSAGE(validate_in_range(SENSOR_TEMP, 100), "temp: 上限100は範囲内");
    TEST_ASSERT_TRUE_MESSAGE(validate_in_range(SENSOR_TEMP, 60),  "temp: 中間値60は範囲内");
    TEST_ASSERT_FALSE_MESSAGE(validate_in_range(SENSOR_TEMP, 24),  "temp: 下限未満24は範囲外");
    TEST_ASSERT_FALSE_MESSAGE(validate_in_range(SENSOR_TEMP, 101), "temp: 上限超過101は範囲外");
}

/* SensorIdとして不正な値（SENSOR_COUNT以上）を渡した場合、常に範囲外になることを確認する */
static void test_invalid_sensor_id(void) {
    TEST_ASSERT_FALSE_MESSAGE(validate_in_range(SENSOR_COUNT, 50), "不正なSensorId: SENSOR_COUNTを渡すと範囲外");
}

/* LOG_LEVEL（0〜2）の境界値・範囲外を確認する */
static void test_log_level_range(void) {
    TEST_ASSERT_TRUE_MESSAGE(validate_log_level(0), "log_level: 下限0(LOG_INFO)は範囲内");
    TEST_ASSERT_TRUE_MESSAGE(validate_log_level(2), "log_level: 上限2(LOG_ERROR)は範囲内");
    TEST_ASSERT_TRUE_MESSAGE(validate_log_level(1), "log_level: 中間値1(LOG_WARNING)は範囲内");
    TEST_ASSERT_FALSE_MESSAGE(validate_log_level(-1), "log_level: 下限未満-1は範囲外");
    TEST_ASSERT_FALSE_MESSAGE(validate_log_level(3),  "log_level: 上限超過3は範囲外");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_speed_range);
    RUN_TEST(test_rpm_range);
    RUN_TEST(test_temp_range);
    RUN_TEST(test_invalid_sensor_id);
    RUN_TEST(test_log_level_range);

    return UNITY_END();
}
