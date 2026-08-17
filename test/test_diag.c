#include "sensor.h"
#include "status.h"
#include "diag.h"
#include "test_common.h"

/* rand() ではなく固定値のセンサ値を使い、diag.c の動作を毎回同じ条件で確認する */
/* test_check() / test_feed() は test_persist.c と共通のため test_common.h / .c に切り出している */

/* status_check の境界値を確認する（閾値ちょうどはWARNING、閾値+1でCRITICALになること） */
static void test_status_boundary(void) {
    VehicleSensorData d;
    SensorStatus status;

    d.speed = 100; d.rpm = 2000; d.temperature = 50;
    status_check(&status, &d, test_default_config());
    test_check("Speed=100はCRITICALではない(WARNING境界)", status.levels[SENSOR_SPEED] == LEVEL_WARNING);

    d.speed = 101;
    status_check(&status, &d, test_default_config());
    test_check("Speed=101でCRITICALになる(境界値+1)", status.levels[SENSOR_SPEED] == LEVEL_CRITICAL);

    d.speed = 50; d.rpm = 5000;
    status_check(&status, &d, test_default_config());
    test_check("RPM=5000はCRITICALではない(WARNING境界)", status.levels[SENSOR_RPM] == LEVEL_WARNING);

    d.rpm = 5001;
    status_check(&status, &d, test_default_config());
    test_check("RPM=5001でCRITICALになる(境界値+1)", status.levels[SENSOR_RPM] == LEVEL_CRITICAL);

    d.rpm = 2000; d.temperature = 90;
    status_check(&status, &d, test_default_config());
    test_check("Temp=90はCRITICALではない(WARNING境界)", status.levels[SENSOR_TEMP] == LEVEL_WARNING);

    d.temperature = 91;
    status_check(&status, &d, test_default_config());
    test_check("Temp=91でCRITICALになる(境界値+1)", status.levels[SENSOR_TEMP] == LEVEL_CRITICAL);
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
    test_check("非デフォルトconfig: Speed=45はデフォルトならNORMALだがWARNINGになる",
          status.levels[SENSOR_SPEED] == LEVEL_WARNING);

    d.speed = 51;   /* 新しいCRITICAL境界値+1 */
    status_check(&status, &d, &cfg);
    test_check("非デフォルトconfig: Speed=51で新しい閾値通りCRITICALになる",
          status.levels[SENSOR_SPEED] == LEVEL_CRITICAL);
}

/* RPMの発生回数・状態区分（ACTIVE/HISTORY）・フリーズフレームの一連の流れを境界値で確認する */
static void test_rpm_lifecycle_and_freeze_frame(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);   /* Sample1: 全センサが正常値 */
    test_check("Sample1後: RPMはまだ発生なし(NONE)",
          dtc.entries[SENSOR_RPM].status == DTC_NONE);

    test_feed(&dtc, 50, 5000, 50);   /* Sample2: RPM境界値(5000)はまだCRITICALではない */
    test_check("Sample2後: RPM=5000ではDTC記録されない(境界値)",
          dtc.entries[SENSOR_RPM].status == DTC_NONE);
    test_check("Sample2後: RPM発生回数は0のまま(境界値)", dtc.entries[SENSOR_RPM].count == 0);

    test_feed(&dtc, 50, 5001, 50);   /* Sample3: RPM境界値+1で初めてCRITICALになる（エッジ） */
    test_check("Sample3後: RPM発生回数が1", dtc.entries[SENSOR_RPM].count == 1);
    test_check("Sample3後: RPM状態はACTIVE", dtc.entries[SENSOR_RPM].status == DTC_ACTIVE);
    test_check("Sample3後: フリーズフレームが記録された", dtc.freeze_frame.captured);
    test_check("Sample3後: フリーズフレームの原因はRPM",
          dtc.freeze_frame.trigger_sensor == SENSOR_RPM);
    test_check("Sample3後: フリーズフレームのRPM値が一致(5001)", dtc.freeze_frame.data.rpm == 5001);

    test_feed(&dtc, 50, 5600, 50);   /* Sample4: RPMがCRITICALのまま継続（二重カウント防止を確認） */
    test_check("Sample4後: RPM発生回数は1のまま(継続は二重カウントしない)",
          dtc.entries[SENSOR_RPM].count == 1);

    test_feed(&dtc, 50, 2000, 50);   /* Sample5: RPMがNORMALに戻る（ACTIVE→HISTORYへの遷移を確認） */
    test_check("Sample5後: RPM状態はHISTORY", dtc.entries[SENSOR_RPM].status == DTC_HISTORY);

    /* Sample6: RPMが最初と別の値(5700)で再度CRITICALになる（再発生のカウントと非上書きを確認） */
    test_feed(&dtc, 50, 5700, 50);
    test_check("Sample6後: RPM発生回数が2に増える(再発生)", dtc.entries[SENSOR_RPM].count == 2);
    test_check("Sample6後: RPM状態は再びACTIVE", dtc.entries[SENSOR_RPM].status == DTC_ACTIVE);
    test_check("Sample6後: フリーズフレームは最初のRPM=5001のまま上書きされない",
          dtc.freeze_frame.data.rpm == 5001);

    test_feed(&dtc, 50, 5700, 91);   /* Sample7: Tempも境界値+1でCRITICALになる（フリーズフレーム非上書きを確認） */
    test_check("Sample7後: Temp発生回数が1", dtc.entries[SENSOR_TEMP].count == 1);
    test_check("Sample7後: フリーズフレームの原因は依然RPM(上書きされない)",
          dtc.freeze_frame.trigger_sensor == SENSOR_RPM);
}

/* CRITICALから完全にNORMALへ戻らずWARNINGどまりでも、ACTIVE→HISTORYへ遷移することを確認する */
static void test_active_to_history_via_warning(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    test_feed(&dtc, 50, 2000, 91);   /* Sample2: Tempが境界値+1でCRITICALになる */
    test_check("Sample2後: Temp状態はACTIVE", dtc.entries[SENSOR_TEMP].status == DTC_ACTIVE);

    test_feed(&dtc, 50, 2000, 85);   /* Sample3: TempはWARNINGまでしか下がらない(NORMALには戻らない) */
    test_check("Sample3後: WARNINGに下がっただけでもHISTORYに遷移する",
          dtc.entries[SENSOR_TEMP].status == DTC_HISTORY);
}

/* CRITICALが一度も発生しなければ、DTCもフリーズフレームも記録されないことを確認する */
static void test_freeze_frame_not_captured_without_critical(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    test_feed(&dtc, 90, 4500, 85);   /* Sample2: 全センサWARNINGまで(CRITICALには到達しない) */

    test_check("CRITICALが一度も発生しなければフリーズフレームは記録されない",
          !dtc.freeze_frame.captured);
    test_check("CRITICALが一度も発生しなければどのセンサも発生回数は0",
          dtc.entries[SENSOR_SPEED].count == 0 &&
          dtc.entries[SENSOR_RPM].count == 0 &&
          dtc.entries[SENSOR_TEMP].count == 0);
}

/* SpeedとRPMが同じサンプルで同時にCRITICALになったときの優先順位（配列の並び順）を確認する */
static void test_freeze_frame_tie_break_speed_vs_rpm(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);    /* Sample1: 正常値 */
    test_feed(&dtc, 101, 5001, 50);   /* Sample2: SpeedとRPMが境界値+1で同時にCRITICALになる */

    test_check("Speed vs RPM同時発生: フリーズフレームの原因はSpeed(配列の並び順が早い)",
          dtc.freeze_frame.trigger_sensor == SENSOR_SPEED);
    test_check("Speed vs RPM同時発生: Speedの発生回数も1", dtc.entries[SENSOR_SPEED].count == 1);
    test_check("Speed vs RPM同時発生: RPMの発生回数も1(両方カウントされる)",
          dtc.entries[SENSOR_RPM].count == 1);
}

/* RPMとTempが同じサンプルで同時にCRITICALになったときの優先順位（配列の並び順）を確認する */
static void test_freeze_frame_tie_break_rpm_vs_temp(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);   /* Sample1: 正常値 */
    test_feed(&dtc, 50, 5001, 91);   /* Sample2: RPMとTempが境界値+1で同時にCRITICALになる */

    test_check("RPM vs Temp同時発生: フリーズフレームの原因はRPM(配列の並び順が早い)",
          dtc.freeze_frame.trigger_sensor == SENSOR_RPM);
    test_check("RPM vs Temp同時発生: RPMの発生回数も1", dtc.entries[SENSOR_RPM].count == 1);
    test_check("RPM vs Temp同時発生: Tempの発生回数も1(両方カウントされる)",
          dtc.entries[SENSOR_TEMP].count == 1);
}

int main(void) {
    test_status_boundary();
    test_status_check_custom_config();
    test_rpm_lifecycle_and_freeze_frame();
    test_active_to_history_via_warning();
    test_freeze_frame_not_captured_without_critical();
    test_freeze_frame_tie_break_speed_vs_rpm();
    test_freeze_frame_tie_break_rpm_vs_temp();

    return test_summary();
}
