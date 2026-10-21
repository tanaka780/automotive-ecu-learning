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
    dtc->entries[DTC_ID_SPEED].count  = 42;
    dtc->entries[DTC_ID_SPEED].status = DTC_HISTORY;
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

/* 保存→読み込みの往復で、4つの記録とfreeze_frameの内容が一致することを確認する。
   previous_badは保存対象外の設計のため、読み込み後は常に異常なしに戻ることも確認する */
static void test_round_trip(void) {
    DtcRecord dtc;
    diag_init(&dtc);

    test_feed(&dtc, 50, 2000, 50);    /* Sample1: 正常値 */
    test_feed(&dtc, 50, 5001, 50);    /* Sample2: RPMがCRITICALになる（フリーズフレーム記録） */
    test_feed(&dtc, 50, 2000, 50);    /* Sample3: RPMがNORMALに戻る → HISTORYへ遷移 */
    test_feed(&dtc, 101, 2000, 111);  /* Sample4: Speedが危険、水温がオーバーヒートになる */
    test_feed(&dtc, 50, 2000, 0);     /* Sample5: 水温センサの断線（値域外）。Speed・オーバーヒートはHISTORYへ */
    /* どれかの記録が0のまま保存・読み込みの一致だけが通ってしまわないよう、4つとも記録されている前提を確かめる */
    for (int i = 0; i < (int)DTC_ID_COUNT; i++) {
        TEST_ASSERT_TRUE_MESSAGE(dtc.entries[i].count >= 1, "前提: 4つの記録がどれも記録されている");
    }

    persist_save_dtc(&dtc, TEST_PERSIST_FILENAME);

    DtcRecord loaded;
    diag_init(&loaded);
    bool ok = persist_load_dtc(&loaded, TEST_PERSIST_FILENAME);

    TEST_ASSERT_TRUE_MESSAGE(ok, "往復一致: 読み込みが成功する");
    for (int i = 0; i < (int)DTC_ID_COUNT; i++) {
        TEST_ASSERT_TRUE_MESSAGE(loaded.entries[i].id == (DtcId)i, "往復一致: 記録の種類が並びどおり");
        TEST_ASSERT_TRUE_MESSAGE(loaded.entries[i].count == dtc.entries[i].count, "往復一致: 発生回数が一致");
        TEST_ASSERT_TRUE_MESSAGE(loaded.entries[i].status == dtc.entries[i].status, "往復一致: 状態区分が一致");
    }
    TEST_ASSERT_TRUE_MESSAGE(loaded.entries[DTC_ID_TEMP_SENSOR].status == DTC_ACTIVE &&
                             loaded.entries[DTC_ID_OVERHEAT].status == DTC_HISTORY,
          "往復一致: 水温の2つの記録の状態区分が入れ替わらない（TempSensorはACTIVE、OverheatはHISTORY）");
    TEST_ASSERT_TRUE_MESSAGE(loaded.freeze_frame.captured == dtc.freeze_frame.captured,
          "往復一致: フリーズフレームの記録有無が一致");
    TEST_ASSERT_TRUE_MESSAGE(loaded.freeze_frame.trigger == DTC_ID_RPM,
          "往復一致: フリーズフレームの原因が一致(RPM)");
    TEST_ASSERT_TRUE_MESSAGE(loaded.freeze_frame.data.rpm == dtc.freeze_frame.data.rpm,
          "往復一致: フリーズフレームのRPM値が一致");

    bool previous_reset = true;
    for (int i = 0; i < (int)DTC_ID_COUNT; i++) {
        if (loaded.previous_bad[i]) {
            previous_reset = false;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(previous_reset, "往復一致: previous_badは保存対象外のため異常なしに戻る");
}

/* ファイルが存在しない場合、失敗を返しdtcを変更しないことを確認する */
static void test_load_file_not_found(void) {
    remove(TEST_PERSIST_FILENAME);   /* ファイルが無い状態を保証する */

    DtcRecord dtc;
    set_sentinel(&dtc);

    bool ok = persist_load_dtc(&dtc, TEST_PERSIST_FILENAME);

    TEST_ASSERT_FALSE_MESSAGE(ok, "ファイル無し: 読み込みは失敗する");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 42 &&
          dtc.entries[DTC_ID_SPEED].status == DTC_HISTORY,
          "ファイル無し: dtcは変更されない");
}

/* 値の個数が足りない壊れたファイルの場合、失敗を返しdtcを変更しないことを確認する */
static void test_load_corrupted_too_few_values(void) {
    write_raw(TEST_PERSIST_FILENAME, "3\n2\n");   /* 13個必要な値が2個しかない */

    DtcRecord dtc;
    set_sentinel(&dtc);

    bool ok = persist_load_dtc(&dtc, TEST_PERSIST_FILENAME);

    TEST_ASSERT_FALSE_MESSAGE(ok, "値不足: 読み込みは失敗する");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 42 &&
          dtc.entries[DTC_ID_SPEED].status == DTC_HISTORY,
          "値不足: dtcは変更されない");
}

/* 数値ではない文字列で壊れたファイルの場合、失敗を返しdtcを変更しないことを確認する */
static void test_load_corrupted_non_numeric(void) {
    write_raw(TEST_PERSIST_FILENAME, "abc\ndef\nghi\n");

    DtcRecord dtc;
    set_sentinel(&dtc);

    bool ok = persist_load_dtc(&dtc, TEST_PERSIST_FILENAME);

    TEST_ASSERT_FALSE_MESSAGE(ok, "数値以外: 読み込みは失敗する");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_SPEED].count == 42 &&
          dtc.entries[DTC_ID_SPEED].status == DTC_HISTORY,
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

/* 通信の途絶の記録（Phase29）も、本番のmeter_dtc.txt・fan_dtc.txtとは別のテスト専用ファイル名を使う */
#define TEST_CAN_PERSIST_FILENAME "test_can_dtc_data.txt"

/* 保存→読み込みの往復で、メッセージ3種の回数・状態区分が入れ替わらずに一致することを確認する（Phase29）。
   3種とも違う値にし、回数と状態区分の値（0〜2）も重ならないようにして、並びの取り違えを見分ける */
static void test_can_round_trip(void) {
    CanDtcRecord rec = {
        .count  = { 5, 6, 7 },
        .status = { DTC_ACTIVE, DTC_HISTORY, DTC_NONE },
    };
    TEST_ASSERT_TRUE_MESSAGE(persist_save_can_dtc(&rec, TEST_CAN_PERSIST_FILENAME, "meter"), "保存は成功する");

    CanDtcRecord loaded = { .count = { 0, 0, 0 }, .status = { DTC_NONE, DTC_NONE, DTC_NONE } };
    TEST_ASSERT_TRUE_MESSAGE(persist_load_can_dtc(&loaded, TEST_CAN_PERSIST_FILENAME, "meter"), "読み込みは成功する");
    for (int i = 0; i < (int)CAN_MSG_COUNT; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(rec.count[i], loaded.count[i], "往復一致: 回数");
        TEST_ASSERT_TRUE_MESSAGE(rec.status[i] == loaded.status[i], "往復一致: 状態区分");
    }
}

/* 通信の途絶の記録のファイルが無い・値が足りない場合、失敗を返し記録を変更しないことを確認する（Phase29） */
static void test_can_load_missing_or_corrupted(void) {
    CanDtcRecord rec = { .count = { 7, 0, 0 }, .status = { DTC_HISTORY, DTC_NONE, DTC_NONE } };

    remove(TEST_CAN_PERSIST_FILENAME);
    TEST_ASSERT_FALSE_MESSAGE(persist_load_can_dtc(&rec, TEST_CAN_PERSIST_FILENAME, "fan"), "ファイル無し: 読み込みは失敗する");
    TEST_ASSERT_EQUAL_MESSAGE(7, rec.count[0], "ファイル無し: 記録は変更されない");

    write_raw(TEST_CAN_PERSIST_FILENAME, "1\n2\n3\n4\n5\n");   /* 6個必要な値が5個しかない */
    TEST_ASSERT_FALSE_MESSAGE(persist_load_can_dtc(&rec, TEST_CAN_PERSIST_FILENAME, "fan"), "値不足: 読み込みは失敗する");
    TEST_ASSERT_EQUAL_MESSAGE(7, rec.count[0], "値不足: 記録は変更されない");
}

/* 書き込みが失敗するとき（/dev/full）、通信の途絶の記録の保存もfalseを返すことを確認する（Phase29） */
static void test_can_save_write_failure(void) {
    CanDtcRecord rec = { .count = { 1, 0, 0 }, .status = { DTC_ACTIVE, DTC_NONE, DTC_NONE } };
    TEST_ASSERT_FALSE_MESSAGE(persist_save_can_dtc(&rec, "/dev/full", "meter"), "書き込み失敗(/dev/full): 保存は失敗を返す");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_round_trip);
    RUN_TEST(test_load_file_not_found);
    RUN_TEST(test_load_corrupted_too_few_values);
    RUN_TEST(test_load_corrupted_non_numeric);
    RUN_TEST(test_save_write_failure);
    RUN_TEST(test_can_round_trip);
    RUN_TEST(test_can_load_missing_or_corrupted);
    RUN_TEST(test_can_save_write_failure);

    int result = UNITY_END();
    remove(TEST_PERSIST_FILENAME);   /* テスト専用ファイルの後片付け */
    remove(TEST_CAN_PERSIST_FILENAME);
    return result;
}
