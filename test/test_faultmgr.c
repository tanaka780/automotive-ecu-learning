#include "unity.h"
#include "faultmgr.h"

void setUp(void) {}
void tearDown(void) {}

/* 指定した3センサのレベルを持つSensorStatusを組み立てる（faultmgr.cはstatus_checkを経由せず
   SensorStatusだけを見るため、値域チェック等を気にせず直接組み立てられる） */
static SensorStatus make_status(SensorLevel speed, SensorLevel rpm, SensorLevel temp) {
    SensorStatus status;
    status.levels[SENSOR_SPEED] = speed;
    status.levels[SENSOR_RPM]   = rpm;
    status.levels[SENSOR_TEMP]  = temp;
    return status;
}

/* faultmgr_init直後は全センサFAULT_NORMAL・カウンタ0であることを確認する */
static void test_init_state(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    for (int i = 0; i < (int)SENSOR_COUNT; i++) {
        TEST_ASSERT_TRUE_MESSAGE(fm.state[i] == FAULT_NORMAL, "init直後は全センサFAULT_NORMAL");
        TEST_ASSERT_EQUAL_MESSAGE(0, fm.critical_count[i], "init直後はcritical_countが0");
        TEST_ASSERT_EQUAL_MESSAGE(0, fm.normal_count[i], "init直後はnormal_countが0");
    }
}

/* Debounce回数(3)に届く前はFAULT_NORMALのままであることを確認する */
static void test_debounce_not_confirmed_before_threshold(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_CRITICAL, LEVEL_NORMAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical);   /* 1回目 */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_NORMAL, "CRITICAL1回目ではまだ確定しない");

    faultmgr_check(&fm, &critical);   /* 2回目（FAULTMGR_DEBOUNCE_COUNT=3の1つ手前） */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_NORMAL, "CRITICAL2回目でもまだ確定しない");
}

/* Debounce回数(3)に達した瞬間にFAULT_DEGRADEDへ確定することを確認する */
static void test_debounce_confirms_at_threshold(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_NORMAL, LEVEL_CRITICAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical);   /* 1回目 */
    faultmgr_check(&fm, &critical);   /* 2回目 */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_RPM] == FAULT_NORMAL, "3回目の直前まではFAULT_NORMAL");

    faultmgr_check(&fm, &critical);   /* 3回目でDebounce確定 */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_RPM] == FAULT_DEGRADED, "CRITICAL3回連続でFAULT_DEGRADEDに確定する");
}

/* CRITICALが連続せず途中で途切れると、連続回数が数え直しになることを確認する */
static void test_debounce_resets_on_interruption(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_CRITICAL);
    SensorStatus normal   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical);   /* Temp CRITICAL 1回目 */
    faultmgr_check(&fm, &critical);   /* Temp CRITICAL 2回目 */
    faultmgr_check(&fm, &normal);     /* 途中でNORMALに戻る（連続が途切れる） */
    faultmgr_check(&fm, &critical);   /* 途切れた後の1回目 */
    faultmgr_check(&fm, &critical);   /* 途切れた後の2回目（通算では4回目だが連続では2回目） */

    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_NORMAL,
          "通算4回でも連続が途切れていればまだ確定しない（Debounceは連続回数で判定する）");
}

/* Recovery回数(3)に届く前はFAULT_DEGRADEDのままであることを確認する */
static void test_recovery_not_confirmed_before_threshold(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_CRITICAL, LEVEL_NORMAL, LEVEL_NORMAL);
    SensorStatus normal   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical);
    faultmgr_check(&fm, &critical);
    faultmgr_check(&fm, &critical);   /* ここでFAULT_DEGRADEDに確定 */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_DEGRADED, "前提: Debounce確定済み");

    faultmgr_check(&fm, &normal);     /* NORMAL1回目 */
    faultmgr_check(&fm, &normal);     /* NORMAL2回目（FAULTMGR_RECOVERY_COUNT=3の1つ手前） */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_DEGRADED, "NORMAL2回ではまだ復帰しない");
}

/* Recovery回数(3)に達した瞬間にFAULT_NORMALへ復帰し、critical_countも0に戻ることを確認する */
static void test_recovery_confirms_at_threshold(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_NORMAL, LEVEL_CRITICAL, LEVEL_NORMAL);
    SensorStatus normal   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical);
    faultmgr_check(&fm, &critical);
    faultmgr_check(&fm, &critical);   /* FAULT_DEGRADEDに確定 */

    faultmgr_check(&fm, &normal);     /* NORMAL1回目 */
    faultmgr_check(&fm, &normal);     /* NORMAL2回目 */
    faultmgr_check(&fm, &normal);     /* NORMAL3回目でRecovery確定 */

    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_RPM] == FAULT_NORMAL, "NORMAL3回連続でFAULT_NORMALに復帰する");
    TEST_ASSERT_EQUAL_MESSAGE(0, fm.critical_count[SENSOR_RPM], "復帰時にcritical_countも0に戻る");
}

/* Degraded中にWARNING止まりで戻ると、NORMALの連続回数が途切れて復帰しないことを確認する */
static void test_recovery_resets_on_warning(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_CRITICAL);
    SensorStatus normal   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);
    SensorStatus warning  = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_WARNING);

    faultmgr_check(&fm, &critical);
    faultmgr_check(&fm, &critical);
    faultmgr_check(&fm, &critical);   /* FAULT_DEGRADEDに確定 */

    faultmgr_check(&fm, &normal);     /* NORMAL1回目 */
    faultmgr_check(&fm, &normal);     /* NORMAL2回目 */
    faultmgr_check(&fm, &warning);    /* WARNING止まりで連続が途切れる */
    faultmgr_check(&fm, &normal);     /* 途切れた後の1回目 */
    faultmgr_check(&fm, &normal);     /* 途切れた後の2回目（通算では4回だが連続では2回） */

    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_DEGRADED,
          "WARNING止まりでNORMALの連続が途切れると復帰しない（Recoveryも連続回数で判定する）");
}

/* センサごとに独立してDebounceが進み、他センサに影響しないことを確認する */
static void test_sensors_independent(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    /* RPMだけCRITICAL、Speed/TempはNORMALのまま3回連続 */
    SensorStatus status = make_status(LEVEL_NORMAL, LEVEL_CRITICAL, LEVEL_NORMAL);
    faultmgr_check(&fm, &status);
    faultmgr_check(&fm, &status);
    faultmgr_check(&fm, &status);

    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_RPM] == FAULT_DEGRADED, "RPMだけFAULT_DEGRADEDに確定する");
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_NORMAL, "SpeedはNORMALのままCRITICALにならず影響を受けない");
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_NORMAL, "TempもNORMALのままCRITICALにならず影響を受けない");
}

/* 全センサFAULT_NORMALのときは、effectiveがrawと完全に一致することを確認する */
static void test_apply_safe_values_all_normal(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    VehicleSensorData raw = { .speed = 90, .rpm = 4500, .temperature = 70 };
    VehicleSensorData effective;
    faultmgr_apply_safe_values(&fm, &raw, &effective);

    TEST_ASSERT_EQUAL_MESSAGE(raw.speed, effective.speed, "全センサNORMAL: speedはrawのまま");
    TEST_ASSERT_EQUAL_MESSAGE(raw.rpm, effective.rpm, "全センサNORMAL: rpmはrawのまま");
    TEST_ASSERT_EQUAL_MESSAGE(raw.temperature, effective.temperature, "全センサNORMAL: temperatureはrawのまま");
}

/* Degraded中のセンサだけフェイルセーフ値に差し替わり、raw自体は変更されないことを確認する */
static void test_apply_safe_values_degraded_sensor_only(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    /* Speedだけを3回連続CRITICALにしてFAULT_DEGRADEDへ確定させる */
    SensorStatus critical = make_status(LEVEL_CRITICAL, LEVEL_NORMAL, LEVEL_NORMAL);
    faultmgr_check(&fm, &critical);
    faultmgr_check(&fm, &critical);
    faultmgr_check(&fm, &critical);

    VehicleSensorData raw = { .speed = 110, .rpm = 4500, .temperature = 70 };
    VehicleSensorData effective;
    faultmgr_apply_safe_values(&fm, &raw, &effective);

    TEST_ASSERT_EQUAL_MESSAGE(FAULTMGR_SAFE_SPEED, effective.speed, "Degraded中のspeedはフェイルセーフ値に差し替わる");
    TEST_ASSERT_EQUAL_MESSAGE(raw.rpm, effective.rpm, "NORMALなrpmはrawのまま");
    TEST_ASSERT_EQUAL_MESSAGE(raw.temperature, effective.temperature, "NORMALなtemperatureはrawのまま");
    TEST_ASSERT_EQUAL_MESSAGE(110, raw.speed, "raw自体は差し替え後も変更されない（diag_check等が引き続き本当の値を見られる）");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init_state);
    RUN_TEST(test_debounce_not_confirmed_before_threshold);
    RUN_TEST(test_debounce_confirms_at_threshold);
    RUN_TEST(test_debounce_resets_on_interruption);
    RUN_TEST(test_recovery_not_confirmed_before_threshold);
    RUN_TEST(test_recovery_confirms_at_threshold);
    RUN_TEST(test_recovery_resets_on_warning);
    RUN_TEST(test_sensors_independent);
    RUN_TEST(test_apply_safe_values_all_normal);
    RUN_TEST(test_apply_safe_values_degraded_sensor_only);

    return UNITY_END();
}
