#include <stdio.h>
#include <string.h>   /* strcat（test_temp_steps_maxでファイルの内容を組み立てる） */
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

/* IGNITION=ON/OFFがそれぞれstateに反映され、trueが返ることを確認する（Phase25） */
static void test_ignition_on_and_off(void) {
    IgnitionState state = IGNITION_OFF;
    write_raw(TEST_FIXTURE_FILENAME, "MODE=FIXED\nIGNITION=ON\n");
    TEST_ASSERT_TRUE_MESSAGE(fixture_load_ignition(&state, TEST_FIXTURE_FILENAME), "IGNITION=ON: 戻り値はtrue");
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "IGNITION=ON: stateはON");

    write_raw(TEST_FIXTURE_FILENAME, "MODE=FIXED\nIGNITION=OFF\n");
    TEST_ASSERT_TRUE_MESSAGE(fixture_load_ignition(&state, TEST_FIXTURE_FILENAME), "IGNITION=OFF: 戻り値はtrue");
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_OFF, "IGNITION=OFF: stateはOFF");
}

/* IGNITION行が無い場合・ファイルが無い場合はfalseを返し、stateを変更しないことを確認する（Phase25） */
static void test_ignition_absent(void) {
    IgnitionState state = IGNITION_ON;   /* 変更されていないことが分かるよう、初期値をONにしておく */
    write_raw(TEST_FIXTURE_FILENAME, "MODE=FIXED\nSPEED=50\n");
    TEST_ASSERT_FALSE_MESSAGE(fixture_load_ignition(&state, TEST_FIXTURE_FILENAME), "IGNITION行なし: 戻り値はfalse");
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "IGNITION行なし: stateは変更されない");

    remove(TEST_FIXTURE_FILENAME);
    TEST_ASSERT_FALSE_MESSAGE(fixture_load_ignition(&state, TEST_FIXTURE_FILENAME), "ファイルなし: 戻り値はfalse");
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "ファイルなし: stateは変更されない");
}

/* ON/OFF以外の値（小文字・数値・空）は無視され、有効な行の値は不正な行で上書きされないことを確認する（Phase25） */
static void test_ignition_ignores_invalid_values(void) {
    IgnitionState state = IGNITION_ON;
    write_raw(TEST_FIXTURE_FILENAME,
        "IGNITION=on\n"
        "IGNITION=1\n"
        "IGNITION=\n");
    TEST_ASSERT_FALSE_MESSAGE(fixture_load_ignition(&state, TEST_FIXTURE_FILENAME), "不正な値のみ: 戻り値はfalse");
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "不正な値のみ: stateは変更されない");

    write_raw(TEST_FIXTURE_FILENAME,
        "IGNITION=OFF\n"
        "IGNITION=on\n");
    TEST_ASSERT_TRUE_MESSAGE(fixture_load_ignition(&state, TEST_FIXTURE_FILENAME), "有効な行の後に不正な行: 戻り値はtrue");
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_OFF, "有効な行の後に不正な行: 有効な行の値(OFF)のまま");
}

/* IGNITION行が複数ある場合、後に書かれた値が採用される（後勝ち）ことを確認する（Phase25） */
static void test_ignition_duplicate_last_wins(void) {
    IgnitionState state = IGNITION_OFF;
    write_raw(TEST_FIXTURE_FILENAME, "IGNITION=OFF\nIGNITION=ON\n");
    TEST_ASSERT_TRUE_MESSAGE(fixture_load_ignition(&state, TEST_FIXTURE_FILENAME), "IGNITION重複: 戻り値はtrue");
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "IGNITION重複: 後に書かれた値(ON)が採用される");
}

/* IGNITIONはMODEに関係なく有効で、fixture_applyの結果（MODE=RANDOMならfalse）にも影響しないことを確認する（Phase25） */
static void test_ignition_independent_of_mode(void) {
    write_raw(TEST_FIXTURE_FILENAME, "MODE=RANDOM\nIGNITION=ON\n");

    IgnitionState state = IGNITION_OFF;
    TEST_ASSERT_TRUE_MESSAGE(fixture_load_ignition(&state, TEST_FIXTURE_FILENAME), "MODE=RANDOM+IGNITION=ON: 戻り値はtrue");
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "MODE=RANDOM+IGNITION=ON: stateはON");

    VehicleSensorData data;
    sensor_init(&data);
    TEST_ASSERT_FALSE_MESSAGE(fixture_apply(&data, TEST_FIXTURE_FILENAME), "MODE=RANDOM+IGNITION=ON: fixture_applyは従来通りfalse");
}

/* TEMP@N行：N秒目より前は当てはまらず、N秒目以降は一番大きいNの値になることを確認する */
static void test_temp_steps_switch_by_sample(void) {
    write_raw(TEST_FIXTURE_FILENAME,
        "MODE=FIXED\n"
        "TEMP=70\n"
        "TEMP@4=80\n"
        "TEMP@6=75\n");
    FixtureTempSteps steps;
    TEST_ASSERT_TRUE_MESSAGE(fixture_load_temp_steps(&steps, TEST_FIXTURE_FILENAME), "TEMP@N行あり: 戻り値はtrue");
    TEST_ASSERT_EQUAL_MESSAGE(2, steps.count, "TEMP@N行あり: 2行読み込む");

    uint8_t temp = 70U;
    TEST_ASSERT_FALSE_MESSAGE(fixture_temp_at(&steps, 3, &temp), "3秒目: 当てはまる行は無い");
    TEST_ASSERT_EQUAL_MESSAGE(70, temp, "3秒目: 水温は変えない");
    TEST_ASSERT_TRUE(fixture_temp_at(&steps, 4, &temp));
    TEST_ASSERT_EQUAL_MESSAGE(80, temp, "4秒目: TEMP@4の値");
    TEST_ASSERT_TRUE(fixture_temp_at(&steps, 5, &temp));
    TEST_ASSERT_EQUAL_MESSAGE(80, temp, "5秒目: TEMP@4の値のまま");
    TEST_ASSERT_TRUE(fixture_temp_at(&steps, 6, &temp));
    TEST_ASSERT_EQUAL_MESSAGE(75, temp, "6秒目: TEMP@6の値");
    TEST_ASSERT_TRUE(fixture_temp_at(&steps, 20, &temp));
    TEST_ASSERT_EQUAL_MESSAGE(75, temp, "20秒目: 一番大きいN(6)の値");
}

/* 行の順番がNの順でなくても、Nで決まることを確認する */
static void test_temp_steps_order_independent(void) {
    write_raw(TEST_FIXTURE_FILENAME, "TEMP@6=75\nTEMP@4=80\n");
    FixtureTempSteps steps;
    (void)fixture_load_temp_steps(&steps, TEST_FIXTURE_FILENAME);
    uint8_t temp = 0U;
    TEST_ASSERT_TRUE(fixture_temp_at(&steps, 5, &temp));
    TEST_ASSERT_EQUAL_MESSAGE(80, temp, "5秒目: 後に書かれたTEMP@4の値");
    TEST_ASSERT_TRUE(fixture_temp_at(&steps, 6, &temp));
    TEST_ASSERT_EQUAL_MESSAGE(75, temp, "6秒目: 先に書かれたTEMP@6の値");
}

/* Nが1以上の整数でない行、値が数値でない・値域外の行は無視することを確認する */
static void test_temp_steps_ignores_invalid_lines(void) {
    write_raw(TEST_FIXTURE_FILENAME,
        "TEMP@0=80\n"
        "TEMP@-1=80\n"
        "TEMP@x=80\n"
        "TEMP@3a=80\n"
        "TEMP@=80\n"
        "TEMP@3=200\n"
        "TEMP@3=abc\n"
        "TEMP@3=\n"
        "TEMP=80\n");
    FixtureTempSteps steps;
    TEST_ASSERT_FALSE_MESSAGE(fixture_load_temp_steps(&steps, TEST_FIXTURE_FILENAME), "不正な行のみ: 戻り値はfalse");
    TEST_ASSERT_EQUAL_MESSAGE(0, steps.count, "不正な行のみ: 1行も読み込まない");

    write_raw(TEST_FIXTURE_FILENAME, "TEMP@3=200\nTEMP@3=78\n");
    TEST_ASSERT_TRUE(fixture_load_temp_steps(&steps, TEST_FIXTURE_FILENAME));
    uint8_t temp = 0U;
    TEST_ASSERT_TRUE(fixture_temp_at(&steps, 3, &temp));
    TEST_ASSERT_EQUAL_MESSAGE(78, temp, "値域外の行は無視し、正しい行の値を使う");
}

/* 同じNが2回あれば後に書かれた値を使い、行数は増えないことを確認する（後勝ち） */
static void test_temp_steps_duplicate_last_wins(void) {
    write_raw(TEST_FIXTURE_FILENAME, "TEMP@4=80\nTEMP@4=85\n");
    FixtureTempSteps steps;
    (void)fixture_load_temp_steps(&steps, TEST_FIXTURE_FILENAME);
    TEST_ASSERT_EQUAL_MESSAGE(1, steps.count, "同じNは1行として持つ");
    uint8_t temp = 0U;
    TEST_ASSERT_TRUE(fixture_temp_at(&steps, 4, &temp));
    TEST_ASSERT_EQUAL_MESSAGE(85, temp, "後に書かれた値(85)を使う");
}

/* TEMP@N行が無い場合・ファイルが無い場合はfalseを返し、count=0にすることを確認する */
static void test_temp_steps_absent(void) {
    FixtureTempSteps steps;
    write_raw(TEST_FIXTURE_FILENAME, "MODE=FIXED\nTEMP=95\n");
    TEST_ASSERT_FALSE_MESSAGE(fixture_load_temp_steps(&steps, TEST_FIXTURE_FILENAME), "TEMP@N行なし: 戻り値はfalse");
    TEST_ASSERT_EQUAL_MESSAGE(0, steps.count, "TEMP@N行なし: count=0");

    remove(TEST_FIXTURE_FILENAME);
    TEST_ASSERT_FALSE_MESSAGE(fixture_load_temp_steps(&steps, TEST_FIXTURE_FILENAME), "ファイルなし: 戻り値はfalse");
    TEST_ASSERT_EQUAL_MESSAGE(0, steps.count, "ファイルなし: count=0");
}

/* FIXTURE_TEMP_STEP_MAXを超えた行は無視することを確認する */
static void test_temp_steps_max(void) {
    char text[512] = "";
    for (int n = 1; n <= (int)FIXTURE_TEMP_STEP_MAX + 1; n++) {
        char line[32];
        (void)snprintf(line, sizeof(line), "TEMP@%d=%d\n", n, 50 + n);
        strcat(text, line);
    }
    write_raw(TEST_FIXTURE_FILENAME, text);
    FixtureTempSteps steps;
    (void)fixture_load_temp_steps(&steps, TEST_FIXTURE_FILENAME);
    TEST_ASSERT_EQUAL_MESSAGE(FIXTURE_TEMP_STEP_MAX, steps.count, "上限までだけ読み込む");
    uint8_t temp = 0U;
    TEST_ASSERT_TRUE(fixture_temp_at(&steps, (int)FIXTURE_TEMP_STEP_MAX + 1, &temp));
    TEST_ASSERT_EQUAL_MESSAGE(50 + (int)FIXTURE_TEMP_STEP_MAX, temp, "上限を超えた行(N=17)は無視し、N=16の値を使う");
}

/* TEMP@N行があっても、fixture_applyとfixture_load_ignitionの結果は変わらないことを確認する */
static void test_temp_steps_do_not_change_apply(void) {
    write_raw(TEST_FIXTURE_FILENAME, "MODE=FIXED\nTEMP=70\nTEMP@1=95\nIGNITION=ON\n");
    VehicleSensorData data;
    sensor_init(&data);
    TEST_ASSERT_TRUE(fixture_apply(&data, TEST_FIXTURE_FILENAME));
    TEST_ASSERT_EQUAL_MESSAGE(70, data.temperature, "fixture_applyはTEMP=の値を使い、TEMP@Nは無視する");
    IgnitionState state = IGNITION_OFF;
    TEST_ASSERT_TRUE(fixture_load_ignition(&state, TEST_FIXTURE_FILENAME));
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "IGNITIONの読み込みに影響しない");
}

/* IGNITION@N行：N秒目より前は当てはまらず、N秒目以降は一番大きいNの状態になることを確認する（行の順番にはよらない） */
static void test_ignition_steps_switch_by_sample(void) {
    write_raw(TEST_FIXTURE_FILENAME,
        "IGNITION=ON\n"
        "IGNITION@8=ON\n"
        "IGNITION@5=OFF\n");
    FixtureIgnitionSteps steps;
    TEST_ASSERT_TRUE_MESSAGE(fixture_load_ignition_steps(&steps, TEST_FIXTURE_FILENAME), "IGNITION@N行あり: 戻り値はtrue");
    TEST_ASSERT_EQUAL_MESSAGE(2, steps.count, "IGNITION@N行あり: 2行読み込む");

    IgnitionState state = IGNITION_ON;
    TEST_ASSERT_FALSE_MESSAGE(fixture_ignition_at(&steps, 4, &state), "4秒目: 当てはまる行は無い");
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "4秒目: 状態は変えない");
    TEST_ASSERT_TRUE(fixture_ignition_at(&steps, 5, &state));
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_OFF, "5秒目: IGNITION@5の状態(OFF)");
    TEST_ASSERT_TRUE(fixture_ignition_at(&steps, 7, &state));
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_OFF, "7秒目: IGNITION@5の状態(OFF)のまま");
    TEST_ASSERT_TRUE(fixture_ignition_at(&steps, 8, &state));
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "8秒目: 先に書かれたIGNITION@8の状態(ON)");
}

/* Nが1以上の整数でない行、ON/OFF以外の値の行は無視し、同じNは後に書かれた状態を使うことを確認する */
static void test_ignition_steps_invalid_and_duplicate(void) {
    write_raw(TEST_FIXTURE_FILENAME,
        "IGNITION@0=OFF\n"
        "IGNITION@x=OFF\n"
        "IGNITION@3a=OFF\n"
        "IGNITION@3=off\n"
        "IGNITION@3=1\n"
        "IGNITION@3=\n");
    FixtureIgnitionSteps steps;
    TEST_ASSERT_FALSE_MESSAGE(fixture_load_ignition_steps(&steps, TEST_FIXTURE_FILENAME), "不正な行のみ: 戻り値はfalse");
    TEST_ASSERT_EQUAL_MESSAGE(0, steps.count, "不正な行のみ: 1行も読み込まない");

    write_raw(TEST_FIXTURE_FILENAME, "IGNITION@3=OFF\nIGNITION@3=ON\n");
    (void)fixture_load_ignition_steps(&steps, TEST_FIXTURE_FILENAME);
    TEST_ASSERT_EQUAL_MESSAGE(1, steps.count, "同じNは1行として持つ");
    IgnitionState state = IGNITION_OFF;
    TEST_ASSERT_TRUE(fixture_ignition_at(&steps, 3, &state));
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "後に書かれた状態(ON)を使う");
}

/* IGNITION@N行が無い場合・ファイルが無い場合はfalseを返し、count=0にすることを確認する */
static void test_ignition_steps_absent(void) {
    FixtureIgnitionSteps steps;
    write_raw(TEST_FIXTURE_FILENAME, "MODE=FIXED\nIGNITION=ON\n");
    TEST_ASSERT_FALSE_MESSAGE(fixture_load_ignition_steps(&steps, TEST_FIXTURE_FILENAME), "IGNITION@N行なし: 戻り値はfalse");
    TEST_ASSERT_EQUAL_MESSAGE(0, steps.count, "IGNITION@N行なし: count=0");

    remove(TEST_FIXTURE_FILENAME);
    TEST_ASSERT_FALSE_MESSAGE(fixture_load_ignition_steps(&steps, TEST_FIXTURE_FILENAME), "ファイルなし: 戻り値はfalse");
    TEST_ASSERT_EQUAL_MESSAGE(0, steps.count, "ファイルなし: count=0");
}

/* FIXTURE_IGNITION_STEP_MAXを超えた行は無視することを確認する */
static void test_ignition_steps_max(void) {
    char text[512] = "";
    for (int n = 1; n <= (int)FIXTURE_IGNITION_STEP_MAX + 1; n++) {
        char line[32];
        (void)snprintf(line, sizeof(line), "IGNITION@%d=%s\n", n, (n % 2 == 0) ? "ON" : "OFF");
        strcat(text, line);
    }
    write_raw(TEST_FIXTURE_FILENAME, text);
    FixtureIgnitionSteps steps;
    (void)fixture_load_ignition_steps(&steps, TEST_FIXTURE_FILENAME);
    TEST_ASSERT_EQUAL_MESSAGE(FIXTURE_IGNITION_STEP_MAX, steps.count, "上限までだけ読み込む");
    IgnitionState state = IGNITION_OFF;
    TEST_ASSERT_TRUE(fixture_ignition_at(&steps, (int)FIXTURE_IGNITION_STEP_MAX + 1, &state));
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "上限を超えた行(N=17、OFF)は無視し、N=16の状態(ON)を使う");
}

/* IGNITION@N行があっても、fixture_load_ignition・fixture_applyの結果は変わらないことを確認する */
static void test_ignition_steps_do_not_change_others(void) {
    write_raw(TEST_FIXTURE_FILENAME, "MODE=FIXED\nTEMP=70\nIGNITION=ON\nIGNITION@2=OFF\n");
    IgnitionState state = IGNITION_OFF;
    TEST_ASSERT_TRUE(fixture_load_ignition(&state, TEST_FIXTURE_FILENAME));
    TEST_ASSERT_TRUE_MESSAGE(state == IGNITION_ON, "fixture_load_ignitionはIGNITION=の状態(ON)を使い、IGNITION@Nは無視する");
    VehicleSensorData data;
    sensor_init(&data);
    TEST_ASSERT_TRUE(fixture_apply(&data, TEST_FIXTURE_FILENAME));
    TEST_ASSERT_EQUAL_MESSAGE(70, data.temperature, "fixture_applyの結果に影響しない");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_apply_all_keys);
    RUN_TEST(test_apply_ignores_invalid_lines);
    RUN_TEST(test_apply_ignores_out_of_range_values);
    RUN_TEST(test_apply_duplicate_key_last_wins);
    RUN_TEST(test_apply_mode_random);
    RUN_TEST(test_ignition_on_and_off);
    RUN_TEST(test_ignition_absent);
    RUN_TEST(test_ignition_ignores_invalid_values);
    RUN_TEST(test_ignition_duplicate_last_wins);
    RUN_TEST(test_ignition_independent_of_mode);
    RUN_TEST(test_temp_steps_switch_by_sample);
    RUN_TEST(test_temp_steps_order_independent);
    RUN_TEST(test_temp_steps_ignores_invalid_lines);
    RUN_TEST(test_temp_steps_duplicate_last_wins);
    RUN_TEST(test_temp_steps_absent);
    RUN_TEST(test_temp_steps_max);
    RUN_TEST(test_temp_steps_do_not_change_apply);
    RUN_TEST(test_ignition_steps_switch_by_sample);
    RUN_TEST(test_ignition_steps_invalid_and_duplicate);
    RUN_TEST(test_ignition_steps_absent);
    RUN_TEST(test_ignition_steps_max);
    RUN_TEST(test_ignition_steps_do_not_change_others);

    int result = UNITY_END();
    remove(TEST_FIXTURE_FILENAME);   /* テスト専用ファイルの後片付け */
    return result;
}
