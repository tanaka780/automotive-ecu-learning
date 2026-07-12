#include <stdio.h>
#include "sensor.h"
#include "status.h"
#include "diag.h"

/* rand() ではなく固定値のセンサ値を使い、diag.c の動作を毎回同じ条件で確認する */

static int g_pass = 0;
static int g_fail = 0;

/* 期待した結果かどうかを表示し、件数を数える */
static void check(const char *label, int condition) {
    if (condition) {
        printf("[PASS] %s\n", label);
        g_pass++;
    } else {
        printf("[FAIL] %s\n", label);
        g_fail++;
    }
}

/* main.c の1サンプル分の流れ（status_check → diag_check）を再現する */
static void run_sample(DtcRecord *dtc, const VehicleSensorData *data) {
    SensorStatus status;
    status_check(&status, data);
    diag_check(dtc, &status, data);
}

/* 固定値のセンサ値を1サンプル分作って流し込む（各テストの入力を1行で書けるようにする） */
static void feed(DtcRecord *dtc, uint8_t speed, uint16_t rpm, uint8_t temperature) {
    VehicleSensorData d;
    d.speed       = speed;
    d.rpm         = rpm;
    d.temperature = temperature;
    run_sample(dtc, &d);
}

/* status_check の境界値を確認する（閾値ちょうどはWARNING、閾値+1でCRITICALになること） */
static void test_status_boundary(void) {
    VehicleSensorData d;
    SensorStatus status;

    d.speed = 100; d.rpm = 2000; d.temperature = 50;
    status_check(&status, &d);
    check("Speed=100はCRITICALではない(WARNING境界)", status.levels[SENSOR_SPEED] == LEVEL_WARNING);

    d.speed = 101;
    status_check(&status, &d);
    check("Speed=101でCRITICALになる(境界値+1)", status.levels[SENSOR_SPEED] == LEVEL_CRITICAL);

    d.speed = 50; d.rpm = 5000;
    status_check(&status, &d);
    check("RPM=5000はCRITICALではない(WARNING境界)", status.levels[SENSOR_RPM] == LEVEL_WARNING);

    d.rpm = 5001;
    status_check(&status, &d);
    check("RPM=5001でCRITICALになる(境界値+1)", status.levels[SENSOR_RPM] == LEVEL_CRITICAL);

    d.rpm = 2000; d.temperature = 90;
    status_check(&status, &d);
    check("Temp=90はCRITICALではない(WARNING境界)", status.levels[SENSOR_TEMP] == LEVEL_WARNING);

    d.temperature = 91;
    status_check(&status, &d);
    check("Temp=91でCRITICALになる(境界値+1)", status.levels[SENSOR_TEMP] == LEVEL_CRITICAL);
}

/* RPMの発生回数・状態区分（ACTIVE/HISTORY）・フリーズフレームの一連の流れを境界値で確認する */
static void test_rpm_lifecycle_and_freeze_frame(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    feed(&dtc, 50, 2000, 50);   /* Sample1: 全センサが正常値 */
    check("Sample1後: RPMはまだ発生なし(NONE)",
          dtc.entries[SENSOR_RPM].status == DTC_NONE);

    feed(&dtc, 50, 5000, 50);   /* Sample2: RPM境界値(5000)はまだCRITICALではない */
    check("Sample2後: RPM=5000ではDTC記録されない(境界値)",
          dtc.entries[SENSOR_RPM].status == DTC_NONE);
    check("Sample2後: RPM発生回数は0のまま(境界値)", dtc.entries[SENSOR_RPM].count == 0);

    feed(&dtc, 50, 5001, 50);   /* Sample3: RPM境界値+1で初めてCRITICALになる（エッジ） */
    check("Sample3後: RPM発生回数が1", dtc.entries[SENSOR_RPM].count == 1);
    check("Sample3後: RPM状態はACTIVE", dtc.entries[SENSOR_RPM].status == DTC_ACTIVE);
    check("Sample3後: フリーズフレームが記録された", dtc.freeze_frame.captured);
    check("Sample3後: フリーズフレームの原因はRPM",
          dtc.freeze_frame.trigger_sensor == SENSOR_RPM);
    check("Sample3後: フリーズフレームのRPM値が一致(5001)", dtc.freeze_frame.data.rpm == 5001);

    feed(&dtc, 50, 5600, 50);   /* Sample4: RPMがCRITICALのまま継続（二重カウント防止を確認） */
    check("Sample4後: RPM発生回数は1のまま(継続は二重カウントしない)",
          dtc.entries[SENSOR_RPM].count == 1);

    feed(&dtc, 50, 2000, 50);   /* Sample5: RPMがNORMALに戻る（ACTIVE→HISTORYへの遷移を確認） */
    check("Sample5後: RPM状態はHISTORY", dtc.entries[SENSOR_RPM].status == DTC_HISTORY);

    /* Sample6: RPMが最初と別の値(5700)で再度CRITICALになる（再発生のカウントと非上書きを確認） */
    feed(&dtc, 50, 5700, 50);
    check("Sample6後: RPM発生回数が2に増える(再発生)", dtc.entries[SENSOR_RPM].count == 2);
    check("Sample6後: RPM状態は再びACTIVE", dtc.entries[SENSOR_RPM].status == DTC_ACTIVE);
    check("Sample6後: フリーズフレームは最初のRPM=5001のまま上書きされない",
          dtc.freeze_frame.data.rpm == 5001);

    feed(&dtc, 50, 5700, 91);   /* Sample7: Tempも境界値+1でCRITICALになる（フリーズフレーム非上書きを確認） */
    check("Sample7後: Temp発生回数が1", dtc.entries[SENSOR_TEMP].count == 1);
    check("Sample7後: フリーズフレームの原因は依然RPM(上書きされない)",
          dtc.freeze_frame.trigger_sensor == SENSOR_RPM);
}

/* CRITICALから完全にNORMALへ戻らずWARNINGどまりでも、ACTIVE→HISTORYへ遷移することを確認する */
static void test_active_to_history_via_warning(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    feed(&dtc, 50, 2000, 91);   /* Sample2: Tempが境界値+1でCRITICALになる */
    check("Sample2後: Temp状態はACTIVE", dtc.entries[SENSOR_TEMP].status == DTC_ACTIVE);

    feed(&dtc, 50, 2000, 85);   /* Sample3: TempはWARNINGまでしか下がらない(NORMALには戻らない) */
    check("Sample3後: WARNINGに下がっただけでもHISTORYに遷移する",
          dtc.entries[SENSOR_TEMP].status == DTC_HISTORY);
}

/* CRITICALが一度も発生しなければ、DTCもフリーズフレームも記録されないことを確認する */
static void test_freeze_frame_not_captured_without_critical(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    feed(&dtc, 90, 4500, 85);   /* Sample2: 全センサWARNINGまで(CRITICALには到達しない) */

    check("CRITICALが一度も発生しなければフリーズフレームは記録されない",
          !dtc.freeze_frame.captured);
    check("CRITICALが一度も発生しなければどのセンサも発生回数は0",
          dtc.entries[SENSOR_SPEED].count == 0 &&
          dtc.entries[SENSOR_RPM].count == 0 &&
          dtc.entries[SENSOR_TEMP].count == 0);
}

/* SpeedとRPMが同じサンプルで同時にCRITICALになったときの優先順位（配列の並び順）を確認する */
static void test_freeze_frame_tie_break_speed_vs_rpm(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    feed(&dtc, 50, 2000, 50);    /* Sample1: 正常値 */
    feed(&dtc, 101, 5001, 50);   /* Sample2: SpeedとRPMが境界値+1で同時にCRITICALになる */

    check("Speed vs RPM同時発生: フリーズフレームの原因はSpeed(配列の並び順が早い)",
          dtc.freeze_frame.trigger_sensor == SENSOR_SPEED);
    check("Speed vs RPM同時発生: Speedの発生回数も1", dtc.entries[SENSOR_SPEED].count == 1);
    check("Speed vs RPM同時発生: RPMの発生回数も1(両方カウントされる)",
          dtc.entries[SENSOR_RPM].count == 1);
}

/* RPMとTempが同じサンプルで同時にCRITICALになったときの優先順位（配列の並び順）を確認する */
static void test_freeze_frame_tie_break_rpm_vs_temp(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    feed(&dtc, 50, 5001, 91);   /* Sample2: RPMとTempが境界値+1で同時にCRITICALになる */

    check("RPM vs Temp同時発生: フリーズフレームの原因はRPM(配列の並び順が早い)",
          dtc.freeze_frame.trigger_sensor == SENSOR_RPM);
    check("RPM vs Temp同時発生: RPMの発生回数も1", dtc.entries[SENSOR_RPM].count == 1);
    check("RPM vs Temp同時発生: Tempの発生回数も1(両方カウントされる)",
          dtc.entries[SENSOR_TEMP].count == 1);
}

int main(void) {
    test_status_boundary();
    test_rpm_lifecycle_and_freeze_frame();
    test_active_to_history_via_warning();
    test_freeze_frame_not_captured_without_critical();
    test_freeze_frame_tie_break_speed_vs_rpm();
    test_freeze_frame_tie_break_rpm_vs_temp();

    printf("\n--- Test Summary: %d passed, %d failed ---\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
