#include "sensor.h"
#include "diag.h"
#include "cmd.h"
#include "test_common.h"

/* cmd_read_line（標準入力からの読み取り）は薄いI/Oのため自動テスト対象外とし、make runでの実行確認で扱う */
/* test_check() / test_feed() は他のtest_*.cと共通のため test_common.h / .c に切り出している */

/* diag_clearが記録済みのDTC・フリーズフレームを初期状態に戻すことを確認する */
static void test_diag_clear_resets_record(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 101, 5001, 91);   /* 全センサをCRITICALにしてDTC・フリーズフレームを発生させる */
    test_check("クリア前: フリーズフレームが記録されている", dtc.freeze_frame.captured);
    test_check("クリア前: Speedの発生回数が1", dtc.entries[SENSOR_SPEED].count == 1);

    diag_clear(&dtc);

    test_check("クリア後: Speedの発生回数が0", dtc.entries[SENSOR_SPEED].count == 0);
    test_check("クリア後: RPMの発生回数が0", dtc.entries[SENSOR_RPM].count == 0);
    test_check("クリア後: Tempの発生回数が0", dtc.entries[SENSOR_TEMP].count == 0);
    test_check("クリア後: Speedの状態区分はNONE", dtc.entries[SENSOR_SPEED].status == DTC_NONE);
    test_check("クリア後: フリーズフレームは未記録に戻る", !dtc.freeze_frame.captured);
}

/* cmd_dispatchが"clear"を受け取ったときにdiag_clear相当の処理を行うことを確認する */
static void test_dispatch_clear_command(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);   /* Speedのみ CRITICAL */

    cmd_dispatch("clear", &dtc);

    test_check("\"clear\"でSpeedの発生回数が0に戻る", dtc.entries[SENSOR_SPEED].count == 0);
    test_check("\"clear\"でフリーズフレームも未記録に戻る", !dtc.freeze_frame.captured);
}

/* cmd_dispatchが想定外の文字列を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_unknown_command_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("start", &dtc);

    test_check("想定外の文字列ではSpeedの発生回数が変化しない(1のまま)", dtc.entries[SENSOR_SPEED].count == 1);
}

/* cmd_dispatchが空文字列(Enterのみ)を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_empty_input_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("", &dtc);

    test_check("空文字列ではSpeedの発生回数が変化しない(1のまま)", dtc.entries[SENSOR_SPEED].count == 1);
}

/* cmd_dispatchが空白のみの入力を受け取ってもDTC記録を変更しないことを確認する(sscanfのトークンが0個のケース) */
static void test_dispatch_whitespace_only_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("   ", &dtc);

    test_check("空白のみの入力ではSpeedの発生回数が変化しない(1のまま)", dtc.entries[SENSOR_SPEED].count == 1);
}

/* diag_clear_sensorが対象センサだけをリセットし、フリーズフレームの原因が対象センサと一致する場合は
   フリーズフレームも合わせて未記録に戻すことを確認する */
static void test_diag_clear_sensor_resets_target_and_matching_freeze_frame(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);   /* Speedのみ CRITICAL -> フリーズフレームの原因もSpeed */

    diag_clear_sensor(&dtc, SENSOR_SPEED);

    test_check("対象センサ(Speed)の発生回数が0に戻る", dtc.entries[SENSOR_SPEED].count == 0);
    test_check("対象センサ(Speed)の状態区分はNONE", dtc.entries[SENSOR_SPEED].status == DTC_NONE);
    test_check("原因がSpeedのフリーズフレームは未記録に戻る", !dtc.freeze_frame.captured);
}

/* diag_clear_sensorが、フリーズフレームの原因が対象センサと異なる場合はフリーズフレームを保持することを確認する */
static void test_diag_clear_sensor_keeps_freeze_frame_when_trigger_differs(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);   /* 1回目: Speedのみ CRITICAL -> フリーズフレームの原因はSpeed */
    test_feed(&dtc, 101, 5001, 50);   /* 2回目: RPMも新たにCRITICALへ(エッジ)。フリーズフレームは上書きされずSpeedのまま */

    diag_clear_sensor(&dtc, SENSOR_RPM);

    test_check("対象センサ(RPM)の発生回数が0に戻る", dtc.entries[SENSOR_RPM].count == 0);
    test_check("対象外センサ(Speed)の発生回数は変化しない(1のまま)", dtc.entries[SENSOR_SPEED].count == 1);
    test_check("原因がSpeedのフリーズフレームは保持される", dtc.freeze_frame.captured);
    test_check("フリーズフレームの原因はSpeedのまま", dtc.freeze_frame.trigger_sensor == SENSOR_SPEED);
}

/* cmd_dispatchが"clear <センサ名>"を受け取ったとき、指定センサだけをクリアし他センサは変更しないことを確認する */
static void test_dispatch_clear_scoped_command(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 5001, 50);   /* Speed・RPMが CRITICAL */

    cmd_dispatch("clear speed", &dtc);

    test_check("\"clear speed\"でSpeedの発生回数が0に戻る", dtc.entries[SENSOR_SPEED].count == 0);
    test_check("\"clear speed\"でRPMの発生回数は変化しない(1のまま)", dtc.entries[SENSOR_RPM].count == 1);
}

/* cmd_dispatchが"clear"に続く不正なセンサ名を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_clear_invalid_target_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("clear xyz", &dtc);

    test_check("不正なセンサ名ではSpeedの発生回数が変化しない(1のまま)", dtc.entries[SENSOR_SPEED].count == 1);
}

/* cmd_dispatchが"clear <センサ名> <余分なトークン>"のような入力を受け取ってもDTC記録を変更しないことを確認する */
static void test_dispatch_clear_extra_token_does_not_clear(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 101, 2000, 50);

    cmd_dispatch("clear speed extra", &dtc);

    test_check("余分なトークンがあるとSpeedの発生回数が変化しない(1のまま)", dtc.entries[SENSOR_SPEED].count == 1);
}

int main(void) {
    test_diag_clear_resets_record();
    test_dispatch_clear_command();
    test_dispatch_unknown_command_does_not_clear();
    test_dispatch_empty_input_does_not_clear();
    test_dispatch_whitespace_only_does_not_clear();
    test_diag_clear_sensor_resets_target_and_matching_freeze_frame();
    test_diag_clear_sensor_keeps_freeze_frame_when_trigger_differs();
    test_dispatch_clear_scoped_command();
    test_dispatch_clear_invalid_target_does_not_clear();
    test_dispatch_clear_extra_token_does_not_clear();

    return test_summary();
}
