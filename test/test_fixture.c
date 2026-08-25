#include <stdio.h>
#include "fixture.h"
#include "test_common.h"

/* 本番のfixture.txtを壊さないよう、テスト専用のファイル名を使う */
#define TEST_FIXTURE_FILENAME "test_fixture.txt"

/* テキストをそのままファイルへ書き込む（fixture_applyを経由せず、任意の内容のファイルを再現するため） */
static void write_raw(const char *filename, const char *text) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return;   /* テスト環境で書き込めない場合は何もしない（通常は起こらない） */
    }
    fputs(text, fp);
    fclose(fp);
}

/* MODE=FIXEDと全センサキーを含むファイルを読み込むと、全てのフィールドに反映されることを確認する */
static void test_apply_all_keys(void) {
    write_raw(TEST_FIXTURE_FILENAME,
        "MODE=FIXED\n"
        "SPEED=110\n"
        "RPM=5500\n"
        "TEMP=95\n");

    VehicleSensorData data;
    sensor_init(&data);
    bool ok = fixture_apply(&data, TEST_FIXTURE_FILENAME);

    test_check("全キー反映: 戻り値はtrue(固定値を反映)", ok);
    test_check("全キー反映: speedが反映される", data.speed == 110);
    test_check("全キー反映: rpmが反映される", data.rpm == 5500);
    test_check("全キー反映: temperatureが反映される", data.temperature == 95);
}

/* 未知のキー・値欠落・数値以外の行は無視され、正常な行だけが反映されることを確認する */
static void test_apply_ignores_invalid_lines(void) {
    write_raw(TEST_FIXTURE_FILENAME,
        "MODE=FIXED\n"
        "UNKNOWN_KEY=123\n"
        "SPEED=\n"
        "TEMP=abc\n"
        "RPM=5000\n");

    VehicleSensorData data;
    sensor_init(&data);   /* speed=0, rpm=800, temperature=25 */
    bool ok = fixture_apply(&data, TEST_FIXTURE_FILENAME);

    test_check("不正行の無視: 戻り値はtrue(MODE=FIXEDは有効)", ok);
    test_check("不正行の無視: 正常な行(RPM)は反映される", data.rpm == 5000);
    test_check("不正行の無視: 値欠落の行の影響を受けずspeedはsensor_initの初期値のまま",
          data.speed == 0);
    test_check("不正行の無視: 数値以外の行の影響を受けずtemperatureはsensor_initの初期値のまま",
          data.temperature == 25);
}

/* 同じキーが複数回書かれた場合、後に書かれた値が採用される（後勝ち）ことを確認する */
static void test_apply_duplicate_key_last_wins(void) {
    write_raw(TEST_FIXTURE_FILENAME,
        "MODE=FIXED\n"
        "SPEED=80\n"
        "SPEED=120\n");

    VehicleSensorData data;
    sensor_init(&data);
    bool ok = fixture_apply(&data, TEST_FIXTURE_FILENAME);

    test_check("キー重複: 戻り値はtrue", ok);
    test_check("キー重複: 後に書かれた値が採用される(後勝ち)", data.speed == 120);
}

/* MODE=RANDOMの場合、戻り値がfalseになりデータが変更されないことを確認する */
static void test_apply_mode_random(void) {
    write_raw(TEST_FIXTURE_FILENAME,
        "MODE=RANDOM\n"
        "SPEED=110\n");   /* MODEがRANDOMならSPEED等は無視され、dataは変更されない想定 */

    VehicleSensorData data;
    data.speed       = 42;   /* sentinel値: 変更されていないことが分かるようにする */
    data.rpm         = 4242;
    data.temperature = 42;
    bool ok = fixture_apply(&data, TEST_FIXTURE_FILENAME);

    test_check("MODE=RANDOM: 戻り値はfalse", !ok);
    test_check("MODE=RANDOM: speedはsentinel値のまま変更されない", data.speed == 42);
    test_check("MODE=RANDOM: rpmはsentinel値のまま変更されない", data.rpm == 4242);
    test_check("MODE=RANDOM: temperatureはsentinel値のまま変更されない", data.temperature == 42);
}

int main(void) {
    test_apply_all_keys();
    test_apply_ignores_invalid_lines();
    test_apply_duplicate_key_last_wins();
    test_apply_mode_random();

    remove(TEST_FIXTURE_FILENAME);   /* テスト専用ファイルの後片付け */

    return test_summary();
}
