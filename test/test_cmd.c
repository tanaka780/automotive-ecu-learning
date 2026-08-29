#include "unity.h"
#include "sensor.h"
#include "diag.h"
#include "cmd.h"
#include "test_common.h"

void setUp(void) {}
void tearDown(void) {}

/* cmd_read_line（標準入力からの読み取り）は薄いI/Oのため自動テスト対象外とし、make runでの実行確認で扱う */
/* test_feed() は他のtest_*.cと共通のため test_common.h / .c に切り出している */

/* diag_clearが記録済みのDTC・フリーズフレームを初期状態に戻すことを確認する */
static void test_diag_clear_resets_record(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 101, 5001, 91);   /* 全センサをCRITICALにしてDTC・フリーズフレームを発生させる */
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.captured, "クリア前: フリーズフレームが記録されている");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 1, "クリア前: Speedの発生回数が1");

    diag_clear(&dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 0, "クリア後: Speedの発生回数が0");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].count == 0, "クリア後: RPMの発生回数が0");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_TEMP].count == 0, "クリア後: Tempの発生回数が0");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].status == DTC_NONE, "クリア後: Speedの状態区分はNONE");
    TEST_ASSERT_FALSE_MESSAGE(dtc.freeze_frame.captured, "クリア後: フリーズフレームは未記録に戻る");
}

/* cmd_dispatchが"clear"を受け取ったときにdiag_clear相当の処理を行うことを確認する */
static void test_dispatch_clear_command(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);   /* Speedのみ CRITICAL */

    cmd_dispatch("clear", &dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 0, "\"clear\"でSpeedの発生回数が0に戻る");
    TEST_ASSERT_FALSE_MESSAGE(dtc.freeze_frame.captured, "\"clear\"でフリーズフレームも未記録に戻る");
}

/* cmd_dispatchが想定外の文字列を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_unknown_command_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("start", &dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 1, "想定外の文字列ではSpeedの発生回数が変化しない(1のまま)");
}

/* cmd_dispatchが空文字列(Enterのみ)を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_empty_input_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("", &dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 1, "空文字列ではSpeedの発生回数が変化しない(1のまま)");
}

/* cmd_dispatchが空白のみの入力を受け取ってもDTC記録を変更しないことを確認する(sscanfのトークンが0個のケース) */
static void test_dispatch_whitespace_only_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("   ", &dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 1, "空白のみの入力ではSpeedの発生回数が変化しない(1のまま)");
}

/* diag_clear_sensorが対象センサだけをリセットし、フリーズフレームの原因が対象センサと一致する場合は
   フリーズフレームも合わせて未記録に戻すことを確認する */
static void test_diag_clear_sensor_resets_target_and_matching_freeze_frame(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);   /* Speedのみ CRITICAL -> フリーズフレームの原因もSpeed */

    diag_clear_sensor(&dtc, SENSOR_SPEED);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 0, "対象センサ(Speed)の発生回数が0に戻る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].status == DTC_NONE, "対象センサ(Speed)の状態区分はNONE");
    TEST_ASSERT_FALSE_MESSAGE(dtc.freeze_frame.captured, "原因がSpeedのフリーズフレームは未記録に戻る");
}

/* diag_clear_sensorが、フリーズフレームの原因が対象センサと異なる場合はフリーズフレームを保持することを確認する */
static void test_diag_clear_sensor_keeps_freeze_frame_when_trigger_differs(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);   /* 1回目: Speedのみ CRITICAL -> フリーズフレームの原因はSpeed */
    test_feed(&dtc, 101, 5001, 50);   /* 2回目: RPMも新たにCRITICALへ(エッジ)。フリーズフレームは上書きされずSpeedのまま */

    diag_clear_sensor(&dtc, SENSOR_RPM);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].count == 0, "対象センサ(RPM)の発生回数が0に戻る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 1, "対象外センサ(Speed)の発生回数は変化しない(1のまま)");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.captured, "原因がSpeedのフリーズフレームは保持される");
    TEST_ASSERT_TRUE_MESSAGE(dtc.freeze_frame.trigger_sensor == SENSOR_SPEED, "フリーズフレームの原因はSpeedのまま");
}

/* cmd_dispatchが"clear <センサ名>"を受け取ったとき、指定センサだけをクリアし他センサは変更しないことを確認する */
static void test_dispatch_clear_scoped_command(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 5001, 50);   /* Speed・RPMが CRITICAL */

    cmd_dispatch("clear speed", &dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 0, "\"clear speed\"でSpeedの発生回数が0に戻る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_RPM].count == 1, "\"clear speed\"でRPMの発生回数は変化しない(1のまま)");
}

/* cmd_dispatchが"clear"に続く不正なセンサ名を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_clear_invalid_target_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("clear xyz", &dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 1, "不正なセンサ名ではSpeedの発生回数が変化しない(1のまま)");
}

/* cmd_dispatchが"clear <センサ名> <余分なトークン>"のような入力を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_clear_extra_token_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("clear speed extra", &dtc);

    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 1, "余分なトークンがあるとSpeedの発生回数が変化しない(1のまま)");
}

/* cmd_dispatchが前後に空白の付いた入力("clear"の前・後ろ)をトリムせず、
   完全一致しないものとしてDTC記録を変更しないことを確認する */
static void test_dispatch_surrounding_space_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch(" clear", &dtc);
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 1, "先頭に空白があるとSpeedの発生回数が変化しない(1のまま)");

    cmd_dispatch("clear ", &dtc);
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 1, "末尾に空白があるとSpeedの発生回数が変化しない(1のまま)");
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

    return UNITY_END();
}
