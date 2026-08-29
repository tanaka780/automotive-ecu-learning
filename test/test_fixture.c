#include <stdio.h>
#include "unity.h"
#include "sensor.h"
#include "fixture.h"

void setUp(void) {}
void tearDown(void) {}

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

    TEST_ASSERT_TRUE_MESSAGE(ok, "全キー反映: 戻り値はtrue(固定値を反映)");
    TEST_ASSERT_TRUE_MESSAGE(data.speed == 110, "全キー反映: speedが反映される");
    TEST_ASSERT_TRUE_MESSAGE(data.rpm == 5500, "全キー反映: rpmが反映される");
    TEST_ASSERT_TRUE_MESSAGE(data.temperature == 95, "全キー反映: temperatureが反映される");
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

    TEST_ASSERT_TRUE_MESSAGE(ok, "不正行の無視: 戻り値はtrue(MODE=FIXEDは有効)");
    TEST_ASSERT_TRUE_MESSAGE(data.rpm == 5000, "不正行の無視: 正常な行(RPM)は反映される");
    TEST_ASSERT_TRUE_MESSAGE(data.speed == 0, "不正行の無視: 値欠落の行の影響を受けずspeedはsensor_initの初期値のまま");
    TEST_ASSERT_TRUE_MESSAGE(data.temperature == 25, "不正行の無視: 数値以外の行の影響を受けずtemperatureはsensor_initの初期値のまま");
}

/* 値域外の値は無視され、他の正常な値は反映されることを確認する */
static void test_apply_ignores_out_of_range_values(void) {
    write_raw(TEST_FIXTURE_FILENAME,
        "MODE=FIXED\n"
        "SPEED=200\n"
        "TEMP=10\n"
        "RPM=5000\n");

    VehicleSensorData data;
    sensor_init(&data);   /* speed=0, rpm=800, temperature=25 */
    bool ok = fixture_apply(&data, TEST_FIXTURE_FILENAME);

    TEST_ASSERT_TRUE_MESSAGE(ok, "値域外の無視: 戻り値はtrue(MODE=FIXEDは有効)");
    TEST_ASSERT_TRUE_MESSAGE(data.rpm == 5000, "値域外の無視: 正常な行(RPM)は反映される");
    TEST_ASSERT_TRUE_MESSAGE(data.speed == 0, "値域外の無視: speedの上限(120)を超える値は反映されずsensor_initの初期値のまま");
    TEST_ASSERT_TRUE_MESSAGE(data.temperature == 25, "値域外の無視: tempの下限(25)未満の値は反映されずsensor_initの初期値のまま");
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

    TEST_ASSERT_TRUE_MESSAGE(ok, "キー重複: 戻り値はtrue");
    TEST_ASSERT_TRUE_MESSAGE(data.speed == 120, "キー重複: 後に書かれた値が採用される(後勝ち)");
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

    TEST_ASSERT_FALSE_MESSAGE(ok, "MODE=RANDOM: 戻り値はfalse");
    TEST_ASSERT_TRUE_MESSAGE(data.speed == 42, "MODE=RANDOM: speedはsentinel値のまま変更されない");
    TEST_ASSERT_TRUE_MESSAGE(data.rpm == 4242, "MODE=RANDOM: rpmはsentinel値のまま変更されない");
    TEST_ASSERT_TRUE_MESSAGE(data.temperature == 42, "MODE=RANDOM: temperatureはsentinel値のまま変更されない");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_apply_all_keys);
    RUN_TEST(test_apply_ignores_invalid_lines);
    RUN_TEST(test_apply_ignores_out_of_range_values);
    RUN_TEST(test_apply_duplicate_key_last_wins);
    RUN_TEST(test_apply_mode_random);

    int result = UNITY_END();
    remove(TEST_FIXTURE_FILENAME);   /* テスト専用ファイルの後片付け */
    return result;
}
