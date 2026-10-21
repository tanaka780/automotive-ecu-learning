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

/* 水温は、値域外を水温センサの故障、値域内の危険をオーバーヒートとして、別の記録に数えることを確認する。
   2つの間を移ると、それぞれの記録で入った・出たと数える（Phase28） */
static void test_temp_records_counted_separately(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);    /* Sample1: 正常値 */
    test_feed(&dtc, 50, 2000, 0);     /* Sample2: 断線（値域外）になった瞬間 */
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_TEMP_SENSOR].count, "Sample2後: 値域外に入った瞬間に、水温センサの故障を1回数える");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_TEMP_SENSOR].status == DTC_ACTIVE, "Sample2後: TempSensorはACTIVE");
    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.entries[DTC_ID_OVERHEAT].count, "Sample2後: 値域外はオーバーヒートとして数えない");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger == DTC_ID_TEMP_SENSOR, "Sample2後: フリーズフレームの原因はTempSensor");
    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.freeze_frame.data.temperature, "Sample2後: フリーズフレームには生の値(0)が残る");

    test_feed(&dtc, 50, 2000, 115);   /* Sample3: 値域外 → 危険（センサが戻ったが高温） */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_TEMP_SENSOR].status == DTC_HISTORY, "Sample3後: 値域内に戻るとTempSensorはHISTORY");
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_OVERHEAT].count, "Sample3後: 値域内の危険で、オーバーヒートを1回数える");

    test_feed(&dtc, 50, 2000, 255);   /* Sample4: 危険 → 値域外（ショート） */
    TEST_ASSERT_EQUAL_MESSAGE(2, dtc.entries[DTC_ID_TEMP_SENSOR].count, "Sample4後: 水温センサの故障の記録では、2回目の発生");
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_OVERHEAT].count, "Sample4後: オーバーヒートは1回のまま");

    test_feed(&dtc, 50, 2000, 80);    /* Sample5: 正常に戻る */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_TEMP_SENSOR].status == DTC_HISTORY &&
                             dtc.entries[DTC_ID_OVERHEAT].status == DTC_HISTORY, "Sample5後: 2つともHISTORY");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger == DTC_ID_TEMP_SENSOR, "Sample5後: フリーズフレームは最初の1回のまま");
}

/* オーバーヒートの記録は、呼び出し側が判断に使える水温で決めたoverheatで数えることを確認する。
   オーバーヒート中にショートしても、判断に使える水温（直前の115℃）が危険なら、ACTIVEのまま残る（Phase28） */
static void test_overheat_record_follows_given_flag(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    VehicleSensorData d = { .speed = 50, .rpm = 2000, .temperature = 115 };
    SensorStatus status;

    status_check(&status, &d, test_default_config());
    diag_check(&dtc, &status, &d, true);
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_OVERHEAT].count, "115℃: オーバーヒートを1回数える");

    d.temperature = 255;   /* ショート。判断に使える水温は直前の115℃のまま */
    status_check(&status, &d, test_default_config());
    diag_check(&dtc, &status, &d, true);
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_OVERHEAT].status == DTC_ACTIVE, "ショート中も、オーバーヒートはACTIVEのまま");
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_OVERHEAT].count, "ショート中も、オーバーヒートは数え直さない");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_TEMP_SENSOR].status == DTC_ACTIVE, "ショートで、水温センサの故障はACTIVE");

    d.temperature = 80;
    status_check(&status, &d, test_default_config());
    diag_check(&dtc, &status, &d, false);
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_OVERHEAT].status == DTC_HISTORY, "80℃: オーバーヒートはHISTORY");
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
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].status == DTC_NONE, "Sample1後: RPMはまだ発生なし(NONE)");

    test_feed(&dtc, 50, 5000, 50);   /* Sample2: RPM境界値(5000)はまだCRITICALではない */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].status == DTC_NONE, "Sample2後: RPM=5000ではDTC記録されない(境界値)");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].count == 0, "Sample2後: RPM発生回数は0のまま(境界値)");

    test_feed(&dtc, 50, 5001, 50);   /* Sample3: RPM境界値+1で初めてCRITICALになる（エッジ） */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].count == 1, "Sample3後: RPM発生回数が1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].status == DTC_ACTIVE, "Sample3後: RPM状態はACTIVE");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.captured, "Sample3後: フリーズフレームが記録された");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger == DTC_ID_RPM, "Sample3後: フリーズフレームの原因はRPM");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.data.rpm == 5001, "Sample3後: フリーズフレームのRPM値が一致(5001)");

    test_feed(&dtc, 50, 5600, 50);   /* Sample4: RPMがCRITICALのまま継続（二重カウント防止を確認） */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].count == 1, "Sample4後: RPM発生回数は1のまま(継続は二重カウントしない)");

    test_feed(&dtc, 50, 2000, 50);   /* Sample5: RPMがNORMALに戻る（ACTIVE→HISTORYへの遷移を確認） */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].status == DTC_HISTORY, "Sample5後: RPM状態はHISTORY");

    /* Sample6: RPMが最初と別の値(5700)で再度CRITICALになる（再発生のカウントと非上書きを確認） */
    test_feed(&dtc, 50, 5700, 50);
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].count == 2, "Sample6後: RPM発生回数が2に増える(再発生)");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].status == DTC_ACTIVE, "Sample6後: RPM状態は再びACTIVE");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.data.rpm == 5001, "Sample6後: フリーズフレームは最初のRPM=5001のまま上書きされない");

    test_feed(&dtc, 50, 5700, 111);   /* Sample7: 水温も境界値+1で危険（オーバーヒート）になる（フリーズフレーム非上書きを確認） */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_OVERHEAT].count == 1, "Sample7後: Overheat発生回数が1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger == DTC_ID_RPM, "Sample7後: フリーズフレームの原因は依然RPM(上書きされない)");
}

/* エンジンをかけ直したとき（diag_reset_previous）は前回の異常の有無だけが戻り、回数・状態区分・フリーズフレームは残る。
   そのため、かけ直した後も異常が続いていれば、異常に入った瞬間としてもう1回数えることを確認する */
static void test_reset_previous_counts_again(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 110, 2000, 70);   /* Sample1: 車速の危険に入った瞬間 */
    test_feed(&dtc, 110, 2000, 70);   /* Sample2: 危険が続く */
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_SPEED].count, "前提: 危険が続いても1回");

    diag_reset_previous(&dtc);
    bool all_reset = true;
    for (int i = 0; i < (int)DTC_ID_COUNT; i++) {
        if (dtc.previous_bad[i]) {
            all_reset = false;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(all_reset, "かけ直し後: 前回の異常の有無は全て異常なし");
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_SPEED].count, "かけ直し後: 回数は残る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].status == DTC_ACTIVE, "かけ直し後: 状態区分は残る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.captured && (dtc.freeze_frame.trigger == DTC_ID_SPEED),
                             "かけ直し後: フリーズフレームは残る");

    test_feed(&dtc, 115, 2000, 70);   /* かけ直した後も危険（値を変えて、フリーズフレームの上書きを見分ける） */
    TEST_ASSERT_EQUAL_MESSAGE(2, dtc.entries[DTC_ID_SPEED].count, "かけ直し後も危険なら、もう1回数える");
    TEST_ASSERT_EQUAL_MESSAGE(110, dtc.freeze_frame.data.speed, "フリーズフレームは最初の1回のまま");
}

/* CRITICALから完全にNORMALへ戻らずWARNINGどまりでも、ACTIVE→HISTORYへ遷移することを確認する */
static void test_active_to_history_via_warning(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    test_feed(&dtc, 50, 2000, 111);  /* Sample2: Tempが境界値+1でCRITICALになる */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_OVERHEAT].status == DTC_ACTIVE, "Sample2後: Overheat状態はACTIVE");

    test_feed(&dtc, 50, 2000, 105);  /* Sample3: TempはWARNINGまでしか下がらない(NORMALには戻らない) */
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_OVERHEAT].status == DTC_HISTORY,
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
          dtc.entries[DTC_ID_SPEED].count == 0 &&
          dtc.entries[DTC_ID_RPM].count == 0 &&
          dtc.entries[DTC_ID_TEMP_SENSOR].count == 0 &&
          dtc.entries[DTC_ID_OVERHEAT].count == 0,
          "CRITICALが一度も発生しなければどの記録も発生回数は0");
}

/* SpeedとRPMが同じサンプルで同時にCRITICALになったときの優先順位（配列の並び順）を確認する */
static void test_freeze_frame_tie_break_speed_vs_rpm(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);    /* Sample1: 正常値 */
    test_feed(&dtc, 101, 5001, 50);   /* Sample2: SpeedとRPMが境界値+1で同時にCRITICALになる */

    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger == DTC_ID_SPEED,
          "Speed vs RPM同時発生: フリーズフレームの原因はSpeed(配列の並び順が早い)");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 1, "Speed vs RPM同時発生: Speedの発生回数も1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].count == 1, "Speed vs RPM同時発生: RPMの発生回数も1(両方カウントされる)");
}

/* RPMとTempが同じサンプルで同時にCRITICALになったときの優先順位（配列の並び順）を確認する */
static void test_freeze_frame_tie_break_rpm_vs_temp(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    test_feed(&dtc, 50, 5001, 111);  /* Sample2: RPMとTempが境界値+1で同時にCRITICALになる */

    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger == DTC_ID_RPM,
          "RPM vs Temp同時発生: フリーズフレームの原因はRPM(配列の並び順が早い)");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].count == 1, "RPM vs Temp同時発生: RPMの発生回数も1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_OVERHEAT].count == 1, "RPM vs Temp同時発生: Overheatの発生回数も1(両方カウントされる)");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_status_boundary);
    RUN_TEST(test_status_check_custom_config);
    RUN_TEST(test_status_out_of_range);
    RUN_TEST(test_status_is_overheat);
    RUN_TEST(test_temp_records_counted_separately);
    RUN_TEST(test_overheat_record_follows_given_flag);
    RUN_TEST(test_rpm_lifecycle_and_freeze_frame);
    RUN_TEST(test_reset_previous_counts_again);
    RUN_TEST(test_active_to_history_via_warning);
    RUN_TEST(test_freeze_frame_not_captured_without_critical);
    RUN_TEST(test_freeze_frame_tie_break_speed_vs_rpm);
    RUN_TEST(test_freeze_frame_tie_break_rpm_vs_temp);

    return UNITY_END();
}
