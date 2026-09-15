#include <stdio.h>
#include "unity.h"
#include "can_fault.h"

void setUp(void) {}
void tearDown(void) {}

/* 本番のcan_fault.txtを壊さないよう、テスト専用のファイル名を使う */
#define TEST_CAN_FAULT_FILENAME "test_can_fault.txt"

/* テキストをそのままファイルへ書き込む（can_fault_loadを経由せず、任意の内容のファイルを再現するため） */
static void write_raw(const char *filename, const char *text) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return;   /* テスト環境で書き込めない場合は何もしない（通常は起こらない） */
    }
    fputs(text, fp);
    fclose(fp);
}

/* can_fault_init直後はmode=NONEで、is_dropped/is_corruptedが常にfalseを返すことを確認する */
static void test_init_is_inactive(void) {
    CanFaultConfig cfg;
    can_fault_init(&cfg);

    TEST_ASSERT_TRUE_MESSAGE(cfg.mode == CAN_FAULT_MODE_NONE, "init直後はmode=NONE");
    TEST_ASSERT_FALSE_MESSAGE(can_fault_is_dropped(&cfg, CAN_MSG_ENGINE_STATUS, 0U), "inactiveならDropしない(t=0)");
    TEST_ASSERT_FALSE_MESSAGE(can_fault_is_dropped(&cfg, CAN_MSG_ENGINE_STATUS, 100000U), "inactiveならDropしない(t=100000)");
    TEST_ASSERT_FALSE_MESSAGE(can_fault_is_corrupted(&cfg, CAN_MSG_ENGINE_STATUS, 0U), "inactiveならCorruptしない(t=0)");
}

/* is_dropped: DROP中でも対象メッセージが違えばDropしないことを確認する */
static void test_is_dropped_ignores_other_target(void) {
    CanFaultConfig cfg = { .mode = CAN_FAULT_MODE_DROP, .target = CAN_MSG_ENGINE_STATUS, .start_ms = 0U, .end_ms = 1000U };

    TEST_ASSERT_FALSE_MESSAGE(can_fault_is_dropped(&cfg, CAN_MSG_FAULT_STATUS, 500U),
                               "targetと違うメッセージはDropしない");
    TEST_ASSERT_TRUE_MESSAGE(can_fault_is_dropped(&cfg, CAN_MSG_ENGINE_STATUS, 500U),
                              "targetと一致するメッセージは期間内ならDropする");
}

/* is_dropped: start_ms/end_msの境界（両端含む）を確認する */
static void test_is_dropped_boundary(void) {
    CanFaultConfig cfg = { .mode = CAN_FAULT_MODE_DROP, .target = CAN_MSG_ENGINE_STATUS, .start_ms = 100U, .end_ms = 200U };

    TEST_ASSERT_FALSE_MESSAGE(can_fault_is_dropped(&cfg, CAN_MSG_ENGINE_STATUS, 99U),  "start_msの直前はDropしない");
    TEST_ASSERT_TRUE_MESSAGE(can_fault_is_dropped(&cfg, CAN_MSG_ENGINE_STATUS, 100U),  "start_msちょうどはDropする");
    TEST_ASSERT_TRUE_MESSAGE(can_fault_is_dropped(&cfg, CAN_MSG_ENGINE_STATUS, 200U),  "end_msちょうどはDropする");
    TEST_ASSERT_FALSE_MESSAGE(can_fault_is_dropped(&cfg, CAN_MSG_ENGINE_STATUS, 201U), "end_msの直後はDropしない");
}

/* is_corrupted: DROPと同じ形の判定（対象メッセージ一致・期間の境界）を確認する（Phase22拡張バックログ） */
static void test_is_corrupted_ignores_other_target(void) {
    CanFaultConfig cfg = { .mode = CAN_FAULT_MODE_CORRUPT, .target = CAN_MSG_ENGINE_STATUS, .start_ms = 0U, .end_ms = 1000U };

    TEST_ASSERT_FALSE_MESSAGE(can_fault_is_corrupted(&cfg, CAN_MSG_FAULT_STATUS, 500U),
                               "targetと違うメッセージはCorruptしない");
    TEST_ASSERT_TRUE_MESSAGE(can_fault_is_corrupted(&cfg, CAN_MSG_ENGINE_STATUS, 500U),
                              "targetと一致するメッセージは期間内ならCorruptする");
    TEST_ASSERT_FALSE_MESSAGE(can_fault_is_dropped(&cfg, CAN_MSG_ENGINE_STATUS, 500U),
                               "CORRUPTモードのcfgはis_droppedではtrueにならない");
}

/* is_corrupted: start_ms/end_msの境界（両端含む）を確認する */
static void test_is_corrupted_boundary(void) {
    CanFaultConfig cfg = { .mode = CAN_FAULT_MODE_CORRUPT, .target = CAN_MSG_ENGINE_STATUS, .start_ms = 100U, .end_ms = 200U };

    TEST_ASSERT_FALSE_MESSAGE(can_fault_is_corrupted(&cfg, CAN_MSG_ENGINE_STATUS, 99U),  "start_msの直前はCorruptしない");
    TEST_ASSERT_TRUE_MESSAGE(can_fault_is_corrupted(&cfg, CAN_MSG_ENGINE_STATUS, 100U),  "start_msちょうどはCorruptする");
    TEST_ASSERT_TRUE_MESSAGE(can_fault_is_corrupted(&cfg, CAN_MSG_ENGINE_STATUS, 200U),  "end_msちょうどはCorruptする");
    TEST_ASSERT_FALSE_MESSAGE(can_fault_is_corrupted(&cfg, CAN_MSG_ENGINE_STATUS, 201U), "end_msの直後はCorruptしない");
}

/* can_fault_load: MODE=DROP・TARGET・START_MS・END_MSが揃っていればcfgに反映されることを確認する */
static void test_load_all_keys(void) {
    write_raw(TEST_CAN_FAULT_FILENAME,
        "MODE=DROP\n"
        "TARGET=ENGINE_STATUS\n"
        "START_MS=5000\n"
        "END_MS=10000\n");

    CanFaultConfig cfg;
    can_fault_init(&cfg);
    bool ok = can_fault_load(&cfg, TEST_CAN_FAULT_FILENAME);

    TEST_ASSERT_TRUE_MESSAGE(ok, "全キー反映: 戻り値はtrue(注入が有効)");
    TEST_ASSERT_TRUE_MESSAGE(cfg.mode == CAN_FAULT_MODE_DROP, "全キー反映: modeがDROPになる");
    TEST_ASSERT_TRUE_MESSAGE(cfg.target == CAN_MSG_ENGINE_STATUS, "全キー反映: targetがENGINE_STATUSになる");
    TEST_ASSERT_EQUAL_MESSAGE(5000, cfg.start_ms, "全キー反映: start_msが反映される");
    TEST_ASSERT_EQUAL_MESSAGE(10000, cfg.end_ms, "全キー反映: end_msが反映される");
}

/* can_fault_load: MODE=CORRUPT・TARGET=ENGINE_STATUSが反映されることを確認する（Phase22拡張バックログ） */
static void test_load_mode_corrupt_engine_status(void) {
    write_raw(TEST_CAN_FAULT_FILENAME,
        "MODE=CORRUPT\n"
        "TARGET=ENGINE_STATUS\n"
        "START_MS=1000\n"
        "END_MS=2000\n");

    CanFaultConfig cfg;
    can_fault_init(&cfg);
    bool ok = can_fault_load(&cfg, TEST_CAN_FAULT_FILENAME);

    TEST_ASSERT_TRUE_MESSAGE(ok, "MODE=CORRUPT+ENGINE_STATUS: 戻り値はtrue(注入が有効)");
    TEST_ASSERT_TRUE_MESSAGE(cfg.mode == CAN_FAULT_MODE_CORRUPT, "MODE=CORRUPT+ENGINE_STATUS: modeがCORRUPTになる");
}

/* can_fault_load: MODE=CORRUPT・TARGET=FAULT_STATUSは無効な組み合わせのため注入なし扱いになることを確認する
   （FAULT_STATUSはビットフラグのみでcan.cにInvalid Data判定ロジックが無いため、Phase22拡張バックログで決定） */
static void test_load_mode_corrupt_fault_status_is_invalid(void) {
    write_raw(TEST_CAN_FAULT_FILENAME,
        "MODE=CORRUPT\n"
        "TARGET=FAULT_STATUS\n"
        "START_MS=0\n"
        "END_MS=1000\n");

    CanFaultConfig cfg;
    can_fault_init(&cfg);
    bool ok = can_fault_load(&cfg, TEST_CAN_FAULT_FILENAME);

    TEST_ASSERT_FALSE_MESSAGE(ok, "MODE=CORRUPT+FAULT_STATUS: 無効な組み合わせのため戻り値はfalse");
    TEST_ASSERT_TRUE_MESSAGE(cfg.mode == CAN_FAULT_MODE_NONE, "MODE=CORRUPT+FAULT_STATUS: modeはNONEのまま");
}

/* can_fault_load: TARGET=FAULT_STATUSも正しく反映されることを確認する */
static void test_load_target_fault_status(void) {
    write_raw(TEST_CAN_FAULT_FILENAME,
        "MODE=DROP\n"
        "TARGET=FAULT_STATUS\n"
        "START_MS=0\n"
        "END_MS=1000\n");

    CanFaultConfig cfg;
    can_fault_init(&cfg);
    bool ok = can_fault_load(&cfg, TEST_CAN_FAULT_FILENAME);

    TEST_ASSERT_TRUE_MESSAGE(ok, "TARGET=FAULT_STATUS: 戻り値はtrue");
    TEST_ASSERT_TRUE_MESSAGE(cfg.target == CAN_MSG_FAULT_STATUS, "TARGET=FAULT_STATUS: targetが反映される");
}

/* can_fault_load: ファイルが無い場合はfalseを返し、cfgを変更しないことを確認する */
static void test_load_file_missing(void) {
    CanFaultConfig cfg;
    can_fault_init(&cfg);
    bool ok = can_fault_load(&cfg, "no_such_can_fault_file.txt");

    TEST_ASSERT_FALSE_MESSAGE(ok, "ファイル無し: 戻り値はfalse");
    TEST_ASSERT_TRUE_MESSAGE(cfg.mode == CAN_FAULT_MODE_NONE, "ファイル無し: modeはinit直後のNONEのまま");
}

/* can_fault_load: MODE=NORMALの場合はfalseを返し、注入なしのままであることを確認する */
static void test_load_mode_normal(void) {
    write_raw(TEST_CAN_FAULT_FILENAME,
        "MODE=NORMAL\n"
        "TARGET=ENGINE_STATUS\n"
        "START_MS=0\n"
        "END_MS=1000\n");

    CanFaultConfig cfg;
    can_fault_init(&cfg);
    bool ok = can_fault_load(&cfg, TEST_CAN_FAULT_FILENAME);

    TEST_ASSERT_FALSE_MESSAGE(ok, "MODE=NORMAL: 戻り値はfalse");
    TEST_ASSERT_TRUE_MESSAGE(cfg.mode == CAN_FAULT_MODE_NONE, "MODE=NORMAL: modeはNONEのまま");
}

/* can_fault_load: MODE=DROPでもTARGETが無ければDrop対象を特定できないため、注入なし扱いになることを確認する */
static void test_load_missing_target_is_inactive(void) {
    write_raw(TEST_CAN_FAULT_FILENAME,
        "MODE=DROP\n"
        "START_MS=0\n"
        "END_MS=1000\n");

    CanFaultConfig cfg;
    can_fault_init(&cfg);
    bool ok = can_fault_load(&cfg, TEST_CAN_FAULT_FILENAME);

    TEST_ASSERT_FALSE_MESSAGE(ok, "TARGET無し: 戻り値はfalse");
    TEST_ASSERT_TRUE_MESSAGE(cfg.mode == CAN_FAULT_MODE_NONE, "TARGET無し: modeはNONEのまま");
}

/* can_fault_load: 不正なTARGET値・未知のキーは無視され、他の正常な行だけが反映されることを確認する */
static void test_load_ignores_invalid_lines(void) {
    write_raw(TEST_CAN_FAULT_FILENAME,
        "MODE=DROP\n"
        "TARGET=UNKNOWN_MSG\n"
        "UNKNOWN_KEY=123\n"
        "START_MS=2000\n"
        "END_MS=3000\n");

    CanFaultConfig cfg;
    can_fault_init(&cfg);
    bool ok = can_fault_load(&cfg, TEST_CAN_FAULT_FILENAME);

    TEST_ASSERT_FALSE_MESSAGE(ok, "不正なTARGET: target_seenがfalseのままなので戻り値はfalse");
    TEST_ASSERT_TRUE_MESSAGE(cfg.mode == CAN_FAULT_MODE_NONE, "不正なTARGET: modeはNONEのまま");
}

/* can_fault_load: 同じキーが複数回書かれた場合、後に書かれた値が採用される（後勝ち）ことを確認する */
static void test_load_duplicate_key_last_wins(void) {
    write_raw(TEST_CAN_FAULT_FILENAME,
        "MODE=DROP\n"
        "TARGET=ENGINE_STATUS\n"
        "START_MS=1000\n"
        "START_MS=5000\n"
        "END_MS=10000\n");

    CanFaultConfig cfg;
    can_fault_init(&cfg);
    bool ok = can_fault_load(&cfg, TEST_CAN_FAULT_FILENAME);

    TEST_ASSERT_TRUE_MESSAGE(ok, "キー重複: 戻り値はtrue");
    TEST_ASSERT_EQUAL_MESSAGE(5000, cfg.start_ms, "キー重複: 後に書かれた値が採用される(後勝ち)");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init_is_inactive);
    RUN_TEST(test_is_dropped_ignores_other_target);
    RUN_TEST(test_is_dropped_boundary);
    RUN_TEST(test_is_corrupted_ignores_other_target);
    RUN_TEST(test_is_corrupted_boundary);
    RUN_TEST(test_load_all_keys);
    RUN_TEST(test_load_mode_corrupt_engine_status);
    RUN_TEST(test_load_mode_corrupt_fault_status_is_invalid);
    RUN_TEST(test_load_target_fault_status);
    RUN_TEST(test_load_file_missing);
    RUN_TEST(test_load_mode_normal);
    RUN_TEST(test_load_missing_target_is_inactive);
    RUN_TEST(test_load_ignores_invalid_lines);
    RUN_TEST(test_load_duplicate_key_last_wins);

    int result = UNITY_END();
    remove(TEST_CAN_FAULT_FILENAME);   /* テスト専用ファイルの後片付け */
    return result;
}
