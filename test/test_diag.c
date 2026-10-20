#include "unity.h"
#include "sensor.h"
#include "status.h"
#include "diag.h"
#include "test_common.h"

void setUp(void) {}
void tearDown(void) {}

/* rand() ではなく固定値のセンサ値を使い、diag.c の動作を毎回同じ条件で確認する */
/* test_feed() は test_persist.c と共通のため test_common.h / .c に切り出している */

/* status_check の境界値を確認する（閾値ちょうどはWARNING、閾値+1でCRITICALになること） */
static void test_status_boundary(void) {
    VehicleSensorData d;
    SensorStatus status;

    d.speed = 100; d.rpm = 2000; d.temperature = 50;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_SPEED] == LEVEL_WARNING, "Speed=100はCRITICALではない(WARNING境界)");

    d.speed = 101;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_SPEED] == LEVEL_CRITICAL, "Speed=101でCRITICALになる(境界値+1)");

    d.speed = 50; d.rpm = 5000;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_RPM] == LEVEL_WARNING, "RPM=5000はCRITICALではない(WARNING境界)");

    d.rpm = 5001;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_RPM] == LEVEL_CRITICAL, "RPM=5001でCRITICALになる(境界値+1)");

    d.rpm = 2000; d.temperature = 110;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_TEMP] == LEVEL_WARNING, "Temp=110はCRITICALではない(WARNING境界)");

    d.temperature = 111;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_TEMP] == LEVEL_CRITICAL, "Temp=111でCRITICALになる(境界値+1)");
}

/* status_check は、値域（validate.c）の外の値を閾値より先に値域外（OUT_OF_RANGE）にすることを確認する（Phase27） */
static void test_status_out_of_range(void) {
    VehicleSensorData d;
    SensorStatus status;

    d.speed = 50; d.rpm = 2000; d.temperature = 130;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_TEMP] == LEVEL_CRITICAL, "Temp=130(値域の上限)は危険で、値域外ではない");

    d.temperature = 131;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_TEMP] == LEVEL_OUT_OF_RANGE, "Temp=131(上限+1)は危険ではなく値域外");

    d.temperature = 255;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_TEMP] == LEVEL_OUT_OF_RANGE, "Temp=255(ショートの再現)は値域外");

    d.temperature = 25;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_TEMP] == LEVEL_NORMAL, "Temp=25(値域の下限)は正常");

    d.temperature = 24;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_TEMP] == LEVEL_OUT_OF_RANGE, "Temp=24(下限-1)は値域外");

    d.temperature = 0;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_TEMP] == LEVEL_OUT_OF_RANGE, "Temp=0(断線の再現)は正常ではなく値域外");
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_SPEED] == LEVEL_NORMAL, "Tempが値域外でも、Speedの判定は変わらない");

    d.temperature = 50; d.speed = 121; d.rpm = 6001;
    status_check(&status, &d, test_default_config());
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_SPEED] == LEVEL_OUT_OF_RANGE, "Speed=121(上限+1)も値域外");
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_RPM] == LEVEL_OUT_OF_RANGE, "RPM=6001(上限+1)も値域外");
}

/* status_is_overheat は、状態判定と同じ閾値で危険（CRITICAL）の段階だけを高温とすることを確認する（Phase27） */
static void test_status_is_overheat(void) {
    TEST_ASSERT_FALSE_MESSAGE(status_is_overheat(110, test_default_config()), "110(危険の境界)は高温ではない");
    TEST_ASSERT_TRUE_MESSAGE(status_is_overheat(111, test_default_config()), "111(境界+1)は高温");
    TEST_ASSERT_TRUE_MESSAGE(status_is_overheat(130, test_default_config()), "130(値域の上限)も高温");
    TEST_ASSERT_FALSE_MESSAGE(status_is_overheat(131, test_default_config()), "131は値域外なので高温とは判断しない");
    TEST_ASSERT_FALSE_MESSAGE(status_is_overheat(255, test_default_config()), "255(ショートの再現)も高温とは判断しない");

    ConfigData cfg = *test_default_config();
    cfg.status_temp_crit = 100;
    TEST_ASSERT_TRUE_MESSAGE(status_is_overheat(101, &cfg), "閾値を下げれば、その閾値で判断する");
}

/* 値域外に入った瞬間もDTCとして数え、危険と値域外の間を移っても数え直さないことを確認する（Phase27） */
static void test_dtc_counts_out_of_range(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);    /* Sample1: 正常値 */
    test_feed(&dtc, 50, 2000, 0);     /* Sample2: 断線（値域外）になった瞬間 */
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[SENSOR_TEMP].count, "Sample2後: 値域外に入った瞬間に1回数える");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_TEMP].status == DTC_ACTIVE, "Sample2後: Temp状態はACTIVE");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.captured, "Sample2後: 値域外でもフリーズフレームを記録する");
    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.freeze_frame.data.temperature, "Sample2後: フリーズフレームには生の値(0)が残る");

    test_feed(&dtc, 50, 2000, 115);   /* Sample3: 値域外 → 危険（センサが戻ったが高温） */
    test_feed(&dtc, 50, 2000, 255);   /* Sample4: 危険 → 値域外（オーバーヒート中にショート） */
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[SENSOR_TEMP].count, "Sample4後: 危険と値域外の間を移っても1回のまま");

    test_feed(&dtc, 50, 2000, 80);    /* Sample5: 正常に戻る */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_TEMP].status == DTC_HISTORY, "Sample5後: 値域内の正常に戻るとHISTORY");
    test_feed(&dtc, 50, 2000, 255);   /* Sample6: 再び値域外 */
    TEST_ASSERT_EQUAL_MESSAGE(2, dtc.entries[SENSOR_TEMP].count, "Sample6後: 正常を挟んだ値域外は再発生として2回目");
    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.freeze_frame.data.temperature, "Sample6後: フリーズフレームは最初の1回のまま");
}

/* 非デフォルトのConfigDataを渡すと、デフォルトならNORMAL/WARNINGどまりの値でも
   下げた閾値に応じてWARNING/CRITICALに変わることを確認する */
static void test_status_check_custom_config(void) {
    ConfigData cfg = *test_default_config();
    cfg.status_speed_warn = 40;   /* デフォルト80から引き下げ */
    cfg.status_speed_crit = 50;   /* デフォルト100から引き下げ */

    VehicleSensorData d;
    SensorStatus status;

    d.speed = 45; d.rpm = 2000; d.temperature = 50;   /* デフォルトならNORMALな速度 */
    status_check(&status, &d, &cfg);
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_SPEED] == LEVEL_WARNING,
          "非デフォルトconfig: Speed=45はデフォルトならNORMALだがWARNINGになる");

    d.speed = 51;   /* 新しいCRITICAL境界値+1 */
    status_check(&status, &d, &cfg);
    TEST_ASSERT_TRUE_MESSAGE(status.levels[SENSOR_SPEED] == LEVEL_CRITICAL,
          "非デフォルトconfig: Speed=51で新しい閾値通りCRITICALになる");
}

/* RPMの発生回数・状態区分（ACTIVE/HISTORY）・フリーズフレームの一連の流れを境界値で確認する */
static void test_rpm_lifecycle_and_freeze_frame(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);   /* Sample1: 全センサが正常値 */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].status == DTC_NONE, "Sample1後: RPMはまだ発生なし(NONE)");

    test_feed(&dtc, 50, 5000, 50);   /* Sample2: RPM境界値(5000)はまだCRITICALではない */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].status == DTC_NONE, "Sample2後: RPM=5000ではDTC記録されない(境界値)");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].count == 0, "Sample2後: RPM発生回数は0のまま(境界値)");

    test_feed(&dtc, 50, 5001, 50);   /* Sample3: RPM境界値+1で初めてCRITICALになる（エッジ） */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].count == 1, "Sample3後: RPM発生回数が1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].status == DTC_ACTIVE, "Sample3後: RPM状態はACTIVE");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.captured, "Sample3後: フリーズフレームが記録された");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger_sensor == SENSOR_RPM, "Sample3後: フリーズフレームの原因はRPM");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.data.rpm == 5001, "Sample3後: フリーズフレームのRPM値が一致(5001)");

    test_feed(&dtc, 50, 5600, 50);   /* Sample4: RPMがCRITICALのまま継続（二重カウント防止を確認） */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].count == 1, "Sample4後: RPM発生回数は1のまま(継続は二重カウントしない)");

    test_feed(&dtc, 50, 2000, 50);   /* Sample5: RPMがNORMALに戻る（ACTIVE→HISTORYへの遷移を確認） */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].status == DTC_HISTORY, "Sample5後: RPM状態はHISTORY");

    /* Sample6: RPMが最初と別の値(5700)で再度CRITICALになる（再発生のカウントと非上書きを確認） */
    test_feed(&dtc, 50, 5700, 50);
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].count == 2, "Sample6後: RPM発生回数が2に増える(再発生)");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].status == DTC_ACTIVE, "Sample6後: RPM状態は再びACTIVE");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.data.rpm == 5001, "Sample6後: フリーズフレームは最初のRPM=5001のまま上書きされない");

    test_feed(&dtc, 50, 5700, 111);   /* Sample7: Tempも境界値+1でCRITICALになる（フリーズフレーム非上書きを確認） */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_TEMP].count == 1, "Sample7後: Temp発生回数が1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger_sensor == SENSOR_RPM, "Sample7後: フリーズフレームの原因は依然RPM(上書きされない)");
}

/* CRITICALから完全にNORMALへ戻らずWARNINGどまりでも、ACTIVE→HISTORYへ遷移することを確認する */
static void test_active_to_history_via_warning(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    test_feed(&dtc, 50, 2000, 111);  /* Sample2: Tempが境界値+1でCRITICALになる */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_TEMP].status == DTC_ACTIVE, "Sample2後: Temp状態はACTIVE");

    test_feed(&dtc, 50, 2000, 105);  /* Sample3: TempはWARNINGまでしか下がらない(NORMALには戻らない) */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_TEMP].status == DTC_HISTORY,
          "Sample3後: WARNINGに下がっただけでもHISTORYに遷移する");
}

/* CRITICALが一度も発生しなければ、DTCもフリーズフレームも記録されないことを確認する */
static void test_freeze_frame_not_captured_without_critical(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    test_feed(&dtc, 90, 4500, 105);  /* Sample2: 全センサWARNINGまで(CRITICALには到達しない) */

    TEST_ASSERT_FALSE_MESSAGE(dtc.freeze_frame.captured, "CRITICALが一度も発生しなければフリーズフレームは記録されない");
    TEST_ASSERT_TRUE_MESSAGE(
          dtc.entries[SENSOR_SPEED].count == 0 &&
          dtc.entries[SENSOR_RPM].count == 0 &&
          dtc.entries[SENSOR_TEMP].count == 0,
          "CRITICALが一度も発生しなければどのセンサも発生回数は0");
}

/* SpeedとRPMが同じサンプルで同時にCRITICALになったときの優先順位（配列の並び順）を確認する */
static void test_freeze_frame_tie_break_speed_vs_rpm(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);    /* Sample1: 正常値 */
    test_feed(&dtc, 101, 5001, 50);   /* Sample2: SpeedとRPMが境界値+1で同時にCRITICALになる */

    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger_sensor == SENSOR_SPEED,
          "Speed vs RPM同時発生: フリーズフレームの原因はSpeed(配列の並び順が早い)");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 1, "Speed vs RPM同時発生: Speedの発生回数も1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].count == 1, "Speed vs RPM同時発生: RPMの発生回数も1(両方カウントされる)");
}

/* RPMとTempが同じサンプルで同時にCRITICALになったときの優先順位（配列の並び順）を確認する */
static void test_freeze_frame_tie_break_rpm_vs_temp(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    test_feed(&dtc, 50, 5001, 111);  /* Sample2: RPMとTempが境界値+1で同時にCRITICALになる */

    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger_sensor == SENSOR_RPM,
          "RPM vs Temp同時発生: フリーズフレームの原因はRPM(配列の並び順が早い)");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].count == 1, "RPM vs Temp同時発生: RPMの発生回数も1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_TEMP].count == 1, "RPM vs Temp同時発生: Tempの発生回数も1(両方カウントされる)");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_status_boundary);
    RUN_TEST(test_status_check_custom_config);
    RUN_TEST(test_status_out_of_range);
    RUN_TEST(test_status_is_overheat);
    RUN_TEST(test_dtc_counts_out_of_range);
    RUN_TEST(test_rpm_lifecycle_and_freeze_frame);
    RUN_TEST(test_active_to_history_via_warning);
    RUN_TEST(test_freeze_frame_not_captured_without_critical);
    RUN_TEST(test_freeze_frame_tie_break_speed_vs_rpm);
    RUN_TEST(test_freeze_frame_tie_break_rpm_vs_temp);

    return UNITY_END();
}
