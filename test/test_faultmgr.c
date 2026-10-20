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

/* 確定・復帰の数え方だけを確かめるテストでfaultmgr_checkに渡す生の値（どれも値域内。Phase27で引数が増えた） */
static const VehicleSensorData in_range_raw = { .speed = 50, .rpm = 2000, .temperature = 70 };

/* faultmgr_init直後は全センサFAULT_NORMAL・カウンタ0であることを確認する */
static void test_init_state(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    for (int i = 0; i < (int)SENSOR_COUNT; i++) {
        TEST_ASSERT_TRUE_MESSAGE(fm.state[i] == FAULT_NORMAL, "init直後は全センサFAULT_NORMAL");
        TEST_ASSERT_EQUAL_MESSAGE(0, fm.bad_count[i], "init直後はbad_countが0");
        TEST_ASSERT_EQUAL_MESSAGE(0, fm.good_count[i], "init直後はgood_countが0");
        TEST_ASSERT_FALSE_MESSAGE(fm.out_of_range[i], "init直後は値域外の記録が無い");
    }
    TEST_ASSERT_EQUAL_MESSAGE(FAULTMGR_SAFE_TEMP, fm.last_valid.temperature, "init直後の直前の値はフェイルセーフ値");
}

/* Debounce回数(3)に届く前はFAULT_NORMALのままであることを確認する */
static void test_debounce_not_confirmed_before_threshold(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_CRITICAL, LEVEL_NORMAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical, &in_range_raw);   /* 1回目 */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_NORMAL, "CRITICAL1回目ではまだ確定しない");

    faultmgr_check(&fm, &critical, &in_range_raw);   /* 2回目（FAULTMGR_DEBOUNCE_COUNT=3の1つ手前） */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_NORMAL, "CRITICAL2回目でもまだ確定しない");
}

/* Debounce回数(3)に達した瞬間にFAULT_DEGRADEDへ確定することを確認する */
static void test_debounce_confirms_at_threshold(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_NORMAL, LEVEL_CRITICAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical, &in_range_raw);   /* 1回目 */
    faultmgr_check(&fm, &critical, &in_range_raw);   /* 2回目 */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_RPM] == FAULT_NORMAL, "3回目の直前まではFAULT_NORMAL");

    faultmgr_check(&fm, &critical, &in_range_raw);   /* 3回目でDebounce確定 */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_RPM] == FAULT_DEGRADED, "CRITICAL3回連続でFAULT_DEGRADEDに確定する");
}

/* CRITICALが連続せず途中で途切れると、連続回数が数え直しになることを確認する */
static void test_debounce_resets_on_interruption(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_CRITICAL, LEVEL_NORMAL, LEVEL_NORMAL);
    SensorStatus normal   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical, &in_range_raw);   /* Speed CRITICAL 1回目 */
    faultmgr_check(&fm, &critical, &in_range_raw);   /* Speed CRITICAL 2回目 */
    faultmgr_check(&fm, &normal, &in_range_raw);     /* 途中でNORMALに戻る（連続が途切れる） */
    faultmgr_check(&fm, &critical, &in_range_raw);   /* 途切れた後の1回目 */
    faultmgr_check(&fm, &critical, &in_range_raw);   /* 途切れた後の2回目（通算では4回目だが連続では2回目） */

    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_NORMAL,
          "通算4回でも連続が途切れていればまだ確定しない（Debounceは連続回数で判定する）");
}

/* Recovery回数(3)に届く前はFAULT_DEGRADEDのままであることを確認する */
static void test_recovery_not_confirmed_before_threshold(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_CRITICAL, LEVEL_NORMAL, LEVEL_NORMAL);
    SensorStatus normal   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical, &in_range_raw);
    faultmgr_check(&fm, &critical, &in_range_raw);
    faultmgr_check(&fm, &critical, &in_range_raw);   /* ここでFAULT_DEGRADEDに確定 */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_DEGRADED, "前提: Debounce確定済み");

    faultmgr_check(&fm, &normal, &in_range_raw);     /* NORMAL1回目 */
    faultmgr_check(&fm, &normal, &in_range_raw);     /* NORMAL2回目（FAULTMGR_RECOVERY_COUNT=3の1つ手前） */
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_DEGRADED, "NORMAL2回ではまだ復帰しない");
}

/* Recovery回数(3)に達した瞬間にFAULT_NORMALへ復帰し、bad_countも0に戻ることを確認する */
static void test_recovery_confirms_at_threshold(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_NORMAL, LEVEL_CRITICAL, LEVEL_NORMAL);
    SensorStatus normal   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical, &in_range_raw);
    faultmgr_check(&fm, &critical, &in_range_raw);
    faultmgr_check(&fm, &critical, &in_range_raw);   /* FAULT_DEGRADEDに確定 */

    faultmgr_check(&fm, &normal, &in_range_raw);     /* NORMAL1回目 */
    faultmgr_check(&fm, &normal, &in_range_raw);     /* NORMAL2回目 */
    faultmgr_check(&fm, &normal, &in_range_raw);     /* NORMAL3回目でRecovery確定 */

    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_RPM] == FAULT_NORMAL, "NORMAL3回連続でFAULT_NORMALに復帰する");
    TEST_ASSERT_EQUAL_MESSAGE(0, fm.bad_count[SENSOR_RPM], "復帰時にbad_countも0に戻る");
}

/* Degraded中にWARNING止まりで戻ると、NORMALの連続回数が途切れて復帰しないことを確認する */
static void test_recovery_resets_on_warning(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus critical = make_status(LEVEL_CRITICAL, LEVEL_NORMAL, LEVEL_NORMAL);
    SensorStatus normal   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);
    SensorStatus warning  = make_status(LEVEL_WARNING, LEVEL_NORMAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &critical, &in_range_raw);
    faultmgr_check(&fm, &critical, &in_range_raw);
    faultmgr_check(&fm, &critical, &in_range_raw);   /* FAULT_DEGRADEDに確定 */

    faultmgr_check(&fm, &normal, &in_range_raw);     /* NORMAL1回目 */
    faultmgr_check(&fm, &normal, &in_range_raw);     /* NORMAL2回目 */
    faultmgr_check(&fm, &warning, &in_range_raw);    /* WARNING止まりで連続が途切れる */
    faultmgr_check(&fm, &normal, &in_range_raw);     /* 途切れた後の1回目 */
    faultmgr_check(&fm, &normal, &in_range_raw);     /* 途切れた後の2回目（通算では4回だが連続では2回） */

    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_DEGRADED,
          "WARNING止まりでNORMALの連続が途切れると復帰しない（Recoveryも連続回数で判定する）");
}

/* センサごとに独立してDebounceが進み、他センサに影響しないことを確認する */
static void test_sensors_independent(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    /* RPMだけCRITICAL、Speed/TempはNORMALのまま3回連続 */
    SensorStatus status = make_status(LEVEL_NORMAL, LEVEL_CRITICAL, LEVEL_NORMAL);
    faultmgr_check(&fm, &status, &in_range_raw);
    faultmgr_check(&fm, &status, &in_range_raw);
    faultmgr_check(&fm, &status, &in_range_raw);

    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_RPM] == FAULT_DEGRADED, "RPMだけFAULT_DEGRADEDに確定する");
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_NORMAL, "SpeedはNORMALのままCRITICALにならず影響を受けない");
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_NORMAL, "TempもNORMALのままCRITICALにならず影響を受けない");
}

/* 水温は危険（オーバーヒート）が続いても確定せず、値域外が3回続いたら確定することを確認する（Phase27） */
static void test_temp_confirms_only_out_of_range(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus overheat = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_CRITICAL);
    SensorStatus open     = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_OUT_OF_RANGE);

    for (int n = 0; n < 5; n++) {
        faultmgr_check(&fm, &overheat, &in_range_raw);
    }
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_NORMAL, "水温の危険が5回続いても確定しない（オーバーヒートはセンサの故障ではない）");

    faultmgr_check(&fm, &open, &in_range_raw);
    faultmgr_check(&fm, &open, &in_range_raw);
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_NORMAL, "値域外2回ではまだ確定しない");
    faultmgr_check(&fm, &open, &in_range_raw);
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_DEGRADED, "値域外3回連続で確定する（危険からそのまま値域外になっても数える）");
}

/* 水温の値域外の連続は、値域内の値（危険を含む）で途切れることを確認する（Phase27） */
static void test_temp_out_of_range_resets_on_in_range(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus overheat = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_CRITICAL);
    SensorStatus open     = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_OUT_OF_RANGE);

    faultmgr_check(&fm, &open, &in_range_raw);
    faultmgr_check(&fm, &open, &in_range_raw);
    faultmgr_check(&fm, &overheat, &in_range_raw);   /* 値域内の危険で連続が途切れる */
    faultmgr_check(&fm, &open, &in_range_raw);
    faultmgr_check(&fm, &open, &in_range_raw);
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_NORMAL, "値域外は通算4回でも、連続2回なら確定しない");
}

/* 水温は、値域内の値なら危険（高温のまま）でも3回続けば復帰することを確認する（Phase27） */
static void test_temp_recovers_on_in_range_even_if_critical(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus open     = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_OUT_OF_RANGE);
    SensorStatus overheat = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_CRITICAL);
    SensorStatus warning  = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_WARNING);

    faultmgr_check(&fm, &open, &in_range_raw);
    faultmgr_check(&fm, &open, &in_range_raw);
    faultmgr_check(&fm, &open, &in_range_raw);
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_DEGRADED, "前提: 値域外3回で確定済み");

    faultmgr_check(&fm, &overheat, &in_range_raw);
    faultmgr_check(&fm, &warning, &in_range_raw);
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_DEGRADED, "値域内2回ではまだ復帰しない");
    faultmgr_check(&fm, &overheat, &in_range_raw);
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_NORMAL, "危険・注意を含む値域内3回で復帰する（センサは直った）");
}

/* 復帰を待つ間に値域外が戻ると、値域内の連続が途切れることを確認する（Phase27） */
static void test_temp_recovery_resets_on_out_of_range(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus open   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_OUT_OF_RANGE);
    SensorStatus normal = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &open, &in_range_raw);
    faultmgr_check(&fm, &open, &in_range_raw);
    faultmgr_check(&fm, &open, &in_range_raw);       /* 確定 */
    faultmgr_check(&fm, &normal, &in_range_raw);
    faultmgr_check(&fm, &normal, &in_range_raw);
    faultmgr_check(&fm, &open, &in_range_raw);       /* 値域外に戻り、連続が途切れる */
    faultmgr_check(&fm, &normal, &in_range_raw);
    faultmgr_check(&fm, &normal, &in_range_raw);
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_DEGRADED, "値域内は通算4回でも、連続2回なら復帰しない");
}

/* 車速・回転数は今までどおりで、値域外も危険と同じく悪いとして数えることを確認する（Phase27） */
static void test_speed_rpm_count_out_of_range_as_bad(void) {
    FaultManager fm;
    faultmgr_init(&fm);

    SensorStatus mixed1 = make_status(LEVEL_CRITICAL, LEVEL_OUT_OF_RANGE, LEVEL_NORMAL);
    SensorStatus mixed2 = make_status(LEVEL_OUT_OF_RANGE, LEVEL_CRITICAL, LEVEL_NORMAL);

    faultmgr_check(&fm, &mixed1, &in_range_raw);
    faultmgr_check(&fm, &mixed2, &in_range_raw);
    faultmgr_check(&fm, &mixed1, &in_range_raw);
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_SPEED] == FAULT_DEGRADED, "Speed: 危険と値域外が混ざって3回続けば確定する");
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_RPM] == FAULT_DEGRADED, "RPM: 危険と値域外が混ざって3回続けば確定する");
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
    faultmgr_check(&fm, &critical, &in_range_raw);
    faultmgr_check(&fm, &critical, &in_range_raw);
    faultmgr_check(&fm, &critical, &in_range_raw);

    VehicleSensorData raw = { .speed = 110, .rpm = 4500, .temperature = 70 };
    VehicleSensorData effective;
    faultmgr_apply_safe_values(&fm, &raw, &effective);

    TEST_ASSERT_EQUAL_MESSAGE(FAULTMGR_SAFE_SPEED, effective.speed, "Degraded中のspeedはフェイルセーフ値に差し替わる");
    TEST_ASSERT_EQUAL_MESSAGE(raw.rpm, effective.rpm, "NORMALなrpmはrawのまま");
    TEST_ASSERT_EQUAL_MESSAGE(raw.temperature, effective.temperature, "NORMALなtemperatureはrawのまま");
    TEST_ASSERT_EQUAL_MESSAGE(110, raw.speed, "raw自体は差し替え後も変更されない（diag_check等が引き続き本当の値を見られる）");
}

/* 1サンプル分（faultmgr_check→faultmgr_apply_safe_values）を行い、差し替え後の値を返す */
static VehicleSensorData feed(FaultManager *fm, const SensorStatus *status, uint8_t temperature) {
    VehicleSensorData raw = { .speed = 50, .rpm = 2000, .temperature = temperature };
    VehicleSensorData effective;
    faultmgr_check(fm, status, &raw);
    faultmgr_apply_safe_values(fm, &raw, &effective);
    TEST_ASSERT_EQUAL_MESSAGE(temperature, raw.temperature, "rawは差し替えで変更されない");
    return effective;
}

/* 確定するまでの値域外は、値域内だった直前の値に差し替え、確定後はフェイルセーフ値にすることを確認する（Phase27） */
static void test_out_of_range_uses_last_valid_until_confirmed(void) {
    FaultManager fm;
    faultmgr_init(&fm);
    SensorStatus normal = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);
    SensorStatus open   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_OUT_OF_RANGE);

    TEST_ASSERT_EQUAL_MESSAGE(100, feed(&fm, &normal, 100).temperature, "値域内の値はそのまま");
    TEST_ASSERT_EQUAL_MESSAGE(100, feed(&fm, &open, 0).temperature, "値域外1回目: 断線の0ではなく直前の100");
    TEST_ASSERT_EQUAL_MESSAGE(100, feed(&fm, &open, 0).temperature, "値域外2回目: 直前の100のまま");
    TEST_ASSERT_EQUAL_MESSAGE(FAULTMGR_SAFE_TEMP, feed(&fm, &open, 0).temperature, "値域外3回目で確定: フェイルセーフ値を優先する");
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_DEGRADED, "前提: 3回目で確定している");
}

/* 起動してから値域内の値が一度も無いまま値域外になった場合は、フェイルセーフ値を使うことを確認する（Phase27） */
static void test_out_of_range_from_start_uses_safe_value(void) {
    FaultManager fm;
    faultmgr_init(&fm);
    SensorStatus shrt = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_OUT_OF_RANGE);

    TEST_ASSERT_EQUAL_MESSAGE(FAULTMGR_SAFE_TEMP, feed(&fm, &shrt, 255).temperature,
          "最初から値域外: 直前の値が無いので、ショートの255ではなくフェイルセーフ値");
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_NORMAL, "前提: まだ確定していない");
}

/* 確定前に値域内へ戻れば生の値を使い、直前の値もその値に更新されることを確認する（Phase27） */
static void test_last_valid_follows_latest_in_range_value(void) {
    FaultManager fm;
    faultmgr_init(&fm);
    SensorStatus normal   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);
    SensorStatus open     = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_OUT_OF_RANGE);
    SensorStatus overheat = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_CRITICAL);

    (void)feed(&fm, &normal, 100);
    (void)feed(&fm, &open, 0);
    TEST_ASSERT_EQUAL_MESSAGE(115, feed(&fm, &overheat, 115).temperature, "値域内に戻れば、危険な高さでも生の値");
    TEST_ASSERT_EQUAL_MESSAGE(115, feed(&fm, &open, 0).temperature, "次の値域外では、更新された直前の値(115)を使う");
}

/* 復帰を待つ間も値域内の値は直前の値として覚え、復帰後の値域外ではその値を使うことを確認する（Phase27） */
static void test_last_valid_updates_while_degraded(void) {
    FaultManager fm;
    faultmgr_init(&fm);
    SensorStatus open   = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_OUT_OF_RANGE);
    SensorStatus normal = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);

    (void)feed(&fm, &open, 0);
    (void)feed(&fm, &open, 0);
    (void)feed(&fm, &open, 0);   /* 確定 */
    TEST_ASSERT_EQUAL_MESSAGE(FAULTMGR_SAFE_TEMP, feed(&fm, &normal, 93).temperature, "復帰待ち: 値域内でもフェイルセーフ値");
    (void)feed(&fm, &normal, 93);
    TEST_ASSERT_EQUAL_MESSAGE(93, feed(&fm, &normal, 93).temperature, "値域内3回で復帰: 生の値");
    TEST_ASSERT_EQUAL_MESSAGE(93, feed(&fm, &open, 0).temperature, "復帰後の値域外: 復帰待ちの間に覚えた93");
}

/* 車速・回転数も、確定前の値域外は直前の値に差し替えることを確認する（Phase27） */
static void test_speed_rpm_out_of_range_use_last_valid(void) {
    FaultManager fm;
    faultmgr_init(&fm);
    SensorStatus normal = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_NORMAL);
    SensorStatus bad    = make_status(LEVEL_OUT_OF_RANGE, LEVEL_OUT_OF_RANGE, LEVEL_NORMAL);
    VehicleSensorData first  = { .speed = 60, .rpm = 3000, .temperature = 70 };
    VehicleSensorData broken = { .speed = 200, .rpm = 9000, .temperature = 70 };
    VehicleSensorData effective;

    faultmgr_check(&fm, &normal, &first);
    faultmgr_check(&fm, &bad, &broken);
    faultmgr_apply_safe_values(&fm, &broken, &effective);
    TEST_ASSERT_EQUAL_MESSAGE(60, effective.speed, "車速の値域外: 直前の60");
    TEST_ASSERT_EQUAL_MESSAGE(3000, effective.rpm, "回転数の値域外: 直前の3000");
    TEST_ASSERT_EQUAL_MESSAGE(70, effective.temperature, "値域内の水温はそのまま");
}

/* 判断に使える水温は、値域内なら今回の値、値域外なら確定の前も後も直前の値域内の値で、
   フェイルセーフ値に差し替わらないことを確認する（Phase27） */
static void test_usable_temperature(void) {
    FaultManager fm;
    faultmgr_init(&fm);
    SensorStatus overheat = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_CRITICAL);
    SensorStatus shrt     = make_status(LEVEL_NORMAL, LEVEL_NORMAL, LEVEL_OUT_OF_RANGE);

    TEST_ASSERT_EQUAL_MESSAGE(FAULTMGR_SAFE_TEMP, faultmgr_usable_temperature(&fm), "起動直後はフェイルセーフ値");
    (void)feed(&fm, &overheat, 115);
    TEST_ASSERT_EQUAL_MESSAGE(115, faultmgr_usable_temperature(&fm), "値域内なら今回の値");
    (void)feed(&fm, &shrt, 255);
    (void)feed(&fm, &shrt, 255);
    (void)feed(&fm, &shrt, 255);
    TEST_ASSERT_TRUE_MESSAGE(fm.state[SENSOR_TEMP] == FAULT_DEGRADED, "前提: 確定している");
    TEST_ASSERT_EQUAL_MESSAGE(115, faultmgr_usable_temperature(&fm), "確定中も、壊れる前の115（フェイルセーフ値ではない）");
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
    RUN_TEST(test_temp_confirms_only_out_of_range);
    RUN_TEST(test_temp_out_of_range_resets_on_in_range);
    RUN_TEST(test_temp_recovers_on_in_range_even_if_critical);
    RUN_TEST(test_temp_recovery_resets_on_out_of_range);
    RUN_TEST(test_speed_rpm_count_out_of_range_as_bad);
    RUN_TEST(test_apply_safe_values_all_normal);
    RUN_TEST(test_apply_safe_values_degraded_sensor_only);
    RUN_TEST(test_out_of_range_uses_last_valid_until_confirmed);
    RUN_TEST(test_out_of_range_from_start_uses_safe_value);
    RUN_TEST(test_last_valid_follows_latest_in_range_value);
    RUN_TEST(test_last_valid_updates_while_degraded);
    RUN_TEST(test_speed_rpm_out_of_range_use_last_valid);
    RUN_TEST(test_usable_temperature);

    return UNITY_END();
}
