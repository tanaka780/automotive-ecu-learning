#include <stdio.h>
#include "unity.h"
#include "sensor.h"
#include "status.h"
#include "diag.h"
#include "persist.h"
#include "test_common.h"

void setUp(void) {}
void tearDown(void) {}

/* 本番のdtc_data.txtを壊さないよう、テスト専用のファイル名を使う */
#define TEST_PERSIST_FILENAME "test_dtc_data.txt"

/* test_feed() は test_diag.c と共通のため test_common.h / .c に切り出している */

/* 読み込み失敗時にdtcが変更されていないことを確認するため、見分けやすい値を設定する */
static void set_sentinel(DtcRecord *dtc) {
    diag_init(dtc);
    dtc->entries[SENSOR_SPEED].count  = 42;
    dtc->entries[SENSOR_SPEED].status = DTC_HISTORY;
}

/* テキストをそのままファイルへ書き込む（persist.cを経由せず、壊れたファイルを再現するため） */
static void write_raw(const char *filename, const char *text) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return;   /* テスト環境で書き込めない場合は何もしない（通常は起こらない） */
    }
    fputs(text, fp);
    fclose(fp);
}

/* 保存→読み込みの往復で、entries・freeze_frameの内容が一致することを確認する。
   previousは保存対象外の設計のため、読み込み後は常にNORMALに戻ることも確認する */
static void test_round_trip(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);    /* Sample1: 正常値 */
    test_feed(&dtc, 50, 5001, 50);    /* Sample2: RPMがCRITICALになる（フリーズフレーム記録） */
    test_feed(&dtc, 50, 2000, 50);    /* Sample3: RPMがNORMALに戻る → HISTORYへ遷移 */
    test_feed(&dtc, 101, 2000, 91);   /* Sample4: SpeedとTempが同時にCRITICALになる */

    persist_save_dtc(&dtc, TEST_PERSIST_FILENAME);

    DtcRecord loaded;
    diag_init(&loaded);
    bool ok = persist_load_dtc(&loaded, TEST_PERSIST_FILENAME);

    TEST_ASSERT_TRUE_MESSAGE(ok, "往復一致: 読み込みが成功する");
    TEST_ASSERT_TRUE_MESSAGE(loaded.entries[SENSOR_SPEED].count == dtc.entries[SENSOR_SPEED].count,
          "往復一致: Speedの発生回数が一致");
    TEST_ASSERT_TRUE_MESSAGE(loaded.entries[SENSOR_SPEED].status == dtc.entries[SENSOR_SPEED].status,
          "往復一致: Speedの状態区分が一致");
    TEST_ASSERT_TRUE_MESSAGE(loaded.entries[SENSOR_RPM].count == dtc.entries[SENSOR_RPM].count,
          "往復一致: RPMの発生回数が一致");
    TEST_ASSERT_TRUE_MESSAGE(loaded.entries[SENSOR_RPM].status == dtc.entries[SENSOR_RPM].status,
          "往復一致: RPMの状態区分が一致(HISTORY)");
    TEST_ASSERT_TRUE_MESSAGE(loaded.entries[SENSOR_TEMP].count == dtc.entries[SENSOR_TEMP].count,
          "往復一致: Tempの発生回数が一致");
    TEST_ASSERT_TRUE_MESSAGE(loaded.entries[SENSOR_TEMP].status == dtc.entries[SENSOR_TEMP].status,
          "往復一致: Tempの状態区分が一致");
    TEST_ASSERT_TRUE_MESSAGE(loaded.freeze_frame.captured == dtc.freeze_frame.captured,
          "往復一致: フリーズフレームの記録有無が一致");
    TEST_ASSERT_TRUE_MESSAGE(loaded.freeze_frame.trigger_sensor == dtc.freeze_frame.trigger_sensor,
          "往復一致: フリーズフレームの原因センサが一致(RPM)");
    TEST_ASSERT_TRUE_MESSAGE(loaded.freeze_frame.data.rpm == dtc.freeze_frame.data.rpm,
          "往復一致: フリーズフレームのRPM値が一致");

    bool previous_reset = true;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (loaded.previous.levels[i] != LEVEL_NORMAL) {
            previous_reset = false;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(previous_reset, "往復一致: previousは保存対象外のためNORMALに戻る");
}

/* ファイルが存在しない場合、失敗を返しdtcを変更しないことを確認する */
static void test_load_file_not_found(void) {
    remove(TEST_PERSIST_FILENAME);   /* ファイルが無い状態を保証する */

    DtcRecord dtc;
    set_sentinel(&dtc);

    bool ok = persist_load_dtc(&dtc, TEST_PERSIST_FILENAME);

    TEST_ASSERT_FALSE_MESSAGE(ok, "ファイル無し: 読み込みは失敗する");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 42 &&
          dtc.entries[SENSOR_SPEED].status == DTC_HISTORY,
          "ファイル無し: dtcは変更されない");
}

/* 値の個数が足りない壊れたファイルの場合、失敗を返しdtcを変更しないことを確認する */
static void test_load_corrupted_too_few_values(void) {
    write_raw(TEST_PERSIST_FILENAME, "3\n2\n");   /* 11個必要な値が2個しかない */

    DtcRecord dtc;
    set_sentinel(&dtc);

    bool ok = persist_load_dtc(&dtc, TEST_PERSIST_FILENAME);

    TEST_ASSERT_FALSE_MESSAGE(ok, "値不足: 読み込みは失敗する");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 42 &&
          dtc.entries[SENSOR_SPEED].status == DTC_HISTORY,
          "値不足: dtcは変更されない");
}

/* 数値ではない文字列で壊れたファイルの場合、失敗を返しdtcを変更しないことを確認する */
static void test_load_corrupted_non_numeric(void) {
    write_raw(TEST_PERSIST_FILENAME, "abc\ndef\nghi\n");

    DtcRecord dtc;
    set_sentinel(&dtc);

    bool ok = persist_load_dtc(&dtc, TEST_PERSIST_FILENAME);

    TEST_ASSERT_FALSE_MESSAGE(ok, "数値以外: 読み込みは失敗する");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[SENSOR_SPEED].count == 42 &&
          dtc.entries[SENSOR_SPEED].status == DTC_HISTORY,
          "数値以外: dtcは変更されない");
}

/* /dev/full（Linux上で書き込みが必ず失敗する特殊デバイス）へ保存させ、
   fputs/fcloseの失敗を検出してfalseを返すことを確認する。WSL2 Ubuntu環境前提のテスト */
static void test_save_write_failure(void) {
    DtcRecord dtc;
    diag_init(&dtc);
    test_feed(&dtc, 50, 5001, 50);   /* 保存対象に適当な中身を持たせる */

    bool ok = persist_save_dtc(&dtc, "/dev/full");

    TEST_ASSERT_FALSE_MESSAGE(ok, "書き込み失敗(/dev/full): 保存は失敗を返す");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_round_trip);
    RUN_TEST(test_load_file_not_found);
    RUN_TEST(test_load_corrupted_too_few_values);
    RUN_TEST(test_load_corrupted_non_numeric);
    RUN_TEST(test_save_write_failure);

    int result = UNITY_END();
    remove(TEST_PERSIST_FILENAME);   /* テスト専用ファイルの後片付け */
    return result;
}
