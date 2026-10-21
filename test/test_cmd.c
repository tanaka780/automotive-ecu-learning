#include "unity.h"
#include "sensor.h"
#include "diag.h"
#include "cmd.h"
#include "can_diag.h"
#include "test_common.h"

/* メーターECU役・ファン制御ECU役の通信の途絶の記録（Phase29）。消えたか・残ったかを見分けるため、毎回0以外にしておく */
static CanDtcRecord meter_dtc;
static CanDtcRecord fan_dtc;

void setUp(void) {
    can_diag_init(&meter_dtc);
    can_diag_init(&fan_dtc);
    meter_dtc.count[CAN_MSG_FAN_STATUS]     = 2;
    meter_dtc.status[CAN_MSG_FAN_STATUS]    = DTC_HISTORY;
    fan_dtc.count[CAN_MSG_ENGINE_STATUS]    = 1;
    fan_dtc.status[CAN_MSG_ENGINE_STATUS]   = DTC_ACTIVE;
}
void tearDown(void) {}

/* cmd_read_line（標準入力からの読み取り）は薄いI/Oのため自動テスト対象外とし、make runでの実行確認で扱う */
/* test_feed() は他のtest_*.cと共通のため test_common.h / .c に切り出している */

/* diag_clearが記録済みのDTC・フリーズフレームを初期状態に戻すことを確認する */
static void test_diag_clear_resets_record(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 101, 5001, 111);  /* 全センサをCRITICALにしてDTC・フリーズフレームを発生させる */
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.captured, "クリア前: フリーズフレームが記録されている");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 1, "クリア前: Speedの発生回数が1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].count == 1, "クリア前: RPMの発生回数が1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_OVERHEAT].count == 1, "クリア前: Overheatの発生回数が1（111℃は値域内の危険）");

    diag_clear(&dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 0, "クリア後: Speedの発生回数が0");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].count == 0, "クリア後: RPMの発生回数が0");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_OVERHEAT].count == 0, "クリア後: Overheatの発生回数が0");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].status == DTC_NONE, "クリア後: Speedの状態区分はNONE");
    TEST_ASSERT_FALSE_MESSAGE(dtc.freeze_frame.captured, "クリア後: フリーズフレームは未記録に戻る");
}

/* cmd_dispatchが"clear"を受け取ったときにdiag_clear相当の処理を行うことを確認する */
static void test_dispatch_clear_command(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);   /* Speedのみ CRITICAL */

    cmd_dispatch("clear", &dtc, &meter_dtc, &fan_dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 0, "\"clear\"でSpeedの発生回数が0に戻る");
    TEST_ASSERT_FALSE_MESSAGE(dtc.freeze_frame.captured, "\"clear\"でフリーズフレームも未記録に戻る");
}

/* cmd_dispatchが想定外の文字列を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_unknown_command_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("start", &dtc, &meter_dtc, &fan_dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 1, "想定外の文字列ではSpeedの発生回数が変化しない(1のまま)");
}

/* cmd_dispatchが空文字列(Enterのみ)を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_empty_input_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("", &dtc, &meter_dtc, &fan_dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 1, "空文字列ではSpeedの発生回数が変化しない(1のまま)");
}

/* cmd_dispatchが空白のみの入力を受け取ってもDTC記録を変更しないことを確認する(sscanfのトークンが0個のケース) */
static void test_dispatch_whitespace_only_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("   ", &dtc, &meter_dtc, &fan_dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 1, "空白のみの入力ではSpeedの発生回数が変化しない(1のまま)");
}

/* diag_clear_sensorが対象センサだけをリセットし、フリーズフレームの原因が対象センサと一致する場合は
   フリーズフレームも合わせて未記録に戻すことを確認する */
static void test_diag_clear_sensor_resets_target_and_matching_freeze_frame(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);   /* Speedのみ CRITICAL -> フリーズフレームの原因もSpeed */

    diag_clear_sensor(&dtc, SENSOR_SPEED);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 0, "対象センサ(Speed)の発生回数が0に戻る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].status == DTC_NONE, "対象センサ(Speed)の状態区分はNONE");
    TEST_ASSERT_FALSE_MESSAGE(dtc.freeze_frame.captured, "原因がSpeedのフリーズフレームは未記録に戻る");
}

/* diag_clear_sensorが、フリーズフレームの原因が対象センサと異なる場合はフリーズフレームを保持することを確認する */
static void test_diag_clear_sensor_keeps_freeze_frame_when_trigger_differs(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);   /* 1回目: Speedのみ CRITICAL -> フリーズフレームの原因はSpeed */
    test_feed(&dtc, 101, 5001, 50);   /* 2回目: RPMも新たにCRITICALへ(エッジ)。フリーズフレームは上書きされずSpeedのまま */

    diag_clear_sensor(&dtc, SENSOR_RPM);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].count == 0, "対象センサ(RPM)の発生回数が0に戻る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 1, "対象外センサ(Speed)の発生回数は変化しない(1のまま)");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.captured, "原因がSpeedのフリーズフレームは保持される");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger == DTC_ID_SPEED, "フリーズフレームの原因はSpeedのまま");
}

/* cmd_dispatchが"clear <センサ名>"を受け取ったとき、指定センサだけをクリアし他センサは変更しないことを確認する */
static void test_dispatch_clear_scoped_command(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 5001, 50);   /* Speed・RPMが CRITICAL */

    cmd_dispatch("clear speed", &dtc, &meter_dtc, &fan_dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 0, "\"clear speed\"でSpeedの発生回数が0に戻る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_RPM].count == 1, "\"clear speed\"でRPMの発生回数は変化しない(1のまま)");
}

/* cmd_dispatchが"clear"に続く不正なセンサ名を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_clear_invalid_target_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("clear xyz", &dtc, &meter_dtc, &fan_dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 1, "不正なセンサ名ではSpeedの発生回数が変化しない(1のまま)");
}

/* cmd_dispatchが"clear <センサ名> <余分なトークン>"のような入力を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_clear_extra_token_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("clear speed extra", &dtc, &meter_dtc, &fan_dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 1, "余分なトークンがあるとSpeedの発生回数が変化しない(1のまま)");
}

/* cmd_dispatchが前後に空白の付いた入力("clear"の前・後ろ)をトリムせず、
   完全一致しないものとしてDTC記録を変更しないことを確認する */
static void test_dispatch_surrounding_space_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch(" clear", &dtc, &meter_dtc, &fan_dtc);
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 1, "先頭に空白があるとSpeedの発生回数が変化しない(1のまま)");

    cmd_dispatch("clear ", &dtc, &meter_dtc, &fan_dtc);
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 1, "末尾に空白があるとSpeedの発生回数が変化しない(1のまま)");
}

/* "clear temp"は、水温センサの故障とオーバーヒートの記録を2つとも消し、原因がどちらかのフリーズフレームも消すことを確認する。
   ほかのセンサを消しても、原因がオーバーヒートのフリーズフレームは残ることも確認する（Phase28） */
static void test_dispatch_clear_temp_clears_both_temp_records(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 50, 2000, 115);   /* オーバーヒート -> フリーズフレームの原因はOverheat */
    test_feed(&dtc, 50, 2000, 0);     /* 断線（値域外） -> 水温センサの故障 */
    test_feed(&dtc, 101, 2000, 70);   /* 車速の危険 */
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_OVERHEAT].count, "前提: Overheatの発生回数が1");
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_TEMP_SENSOR].count, "前提: TempSensorの発生回数が1");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger == DTC_ID_OVERHEAT, "前提: フリーズフレームの原因はOverheat");

    cmd_dispatch("clear speed", &dtc, &meter_dtc, &fan_dtc);
    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.entries[DTC_ID_SPEED].count, "\"clear speed\"でSpeedの発生回数が0に戻る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.captured, "\"clear speed\"では、原因がOverheatのフリーズフレームは残る");

    cmd_dispatch("clear temp", &dtc, &meter_dtc, &fan_dtc);
    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.entries[DTC_ID_TEMP_SENSOR].count, "\"clear temp\"でTempSensorの発生回数が0に戻る");
    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.entries[DTC_ID_OVERHEAT].count, "\"clear temp\"でOverheatの発生回数も0に戻る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_TEMP_SENSOR].status == DTC_NONE &&
                             dtc.entries[DTC_ID_OVERHEAT].status == DTC_NONE, "\"clear temp\"で2つとも状態区分はNONE");
    TEST_ASSERT_FALSE_MESSAGE(dtc.freeze_frame.captured, "\"clear temp\"で、原因がOverheatのフリーズフレームも消える");
}

/* "clear"（対象を指定しない）は、メーターECU役・ファン制御ECU役の通信の途絶の記録も消す（Phase29） */
static void test_dispatch_clear_clears_all_ecus(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 110, 2000, 50);

    cmd_dispatch("clear", &dtc, &meter_dtc, &fan_dtc);

    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.entries[DTC_ID_SPEED].count, "\"clear\"でエンジン監視ECUの記録が消える");
    TEST_ASSERT_EQUAL_MESSAGE(0, meter_dtc.count[CAN_MSG_FAN_STATUS], "\"clear\"でメーターECU役の記録も消える");
    TEST_ASSERT_TRUE_MESSAGE(meter_dtc.status[CAN_MSG_FAN_STATUS] == DTC_NONE, "メーターECU役の状態区分はNONE");
    TEST_ASSERT_EQUAL_MESSAGE(0, fan_dtc.count[CAN_MSG_ENGINE_STATUS], "\"clear\"でファン制御ECU役の記録も消える");
    TEST_ASSERT_TRUE_MESSAGE(fan_dtc.status[CAN_MSG_ENGINE_STATUS] == DTC_NONE, "ファン制御ECU役の状態区分はNONE");
}

/* "clear <センサ名>"と想定外の入力は、メーターECU役・ファン制御ECU役の記録を消さない（Phase29） */
static void test_dispatch_other_commands_keep_other_ecus(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    const char *lines[] = { "clear speed", "clear temp", "clear xyz", "start", "" };
    for (int i = 0; i < 5; i++) {
        cmd_dispatch(lines[i], &dtc, &meter_dtc, &fan_dtc);
        TEST_ASSERT_EQUAL_MESSAGE(2, meter_dtc.count[CAN_MSG_FAN_STATUS], "メーターECU役の記録は残る");
        TEST_ASSERT_EQUAL_MESSAGE(1, fan_dtc.count[CAN_MSG_ENGINE_STATUS], "ファン制御ECU役の記録は残る");
    }
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_diag_clear_resets_record);
    RUN_TEST(test_dispatch_clear_command);
    RUN_TEST(test_dispatch_unknown_command_does_not_clear);
    RUN_TEST(test_dispatch_empty_input_does_not_clear);
    RUN_TEST(test_dispatch_whitespace_only_does_not_clear);
    RUN_TEST(test_diag_clear_sensor_resets_target_and_matching_freeze_frame);
    RUN_TEST(test_diag_clear_sensor_keeps_freeze_frame_when_trigger_differs);
    RUN_TEST(test_dispatch_clear_scoped_command);
    RUN_TEST(test_dispatch_clear_invalid_target_does_not_clear);
    RUN_TEST(test_dispatch_clear_extra_token_does_not_clear);
    RUN_TEST(test_dispatch_surrounding_space_does_not_clear);
    RUN_TEST(test_dispatch_clear_temp_clears_both_temp_records);
    RUN_TEST(test_dispatch_clear_clears_all_ecus);
    RUN_TEST(test_dispatch_other_commands_keep_other_ecus);

    return UNITY_END();
}
