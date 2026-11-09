#include <stdio.h>
#include "unity.h"
#include "task_stall.h"

void setUp(void) {}
void tearDown(void) {}

/* 本番のtask_stall.txtを壊さないよう、テスト専用のファイル名を使う */
#define TEST_TASK_STALL_FILENAME "test_task_stall.txt"

/* テキストをそのままファイルへ書き込む（task_stall_loadに任意の内容のファイルを読ませるため） */
static void write_raw(const char *filename, const char *text) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return;   /* テスト環境で書き込めない場合は何もしない（通常は起こらない） */
    }
    fputs(text, fp);
    fclose(fp);
}

/* init直後は、どのタスクも止めない */
static void test_init_stops_nothing(void) {
    TaskStallConfig cfg;
    task_stall_init(&cfg);

    TEST_ASSERT_FALSE_MESSAGE(task_stall_is_stopped(&cfg, TASK_STALL_FAN_RX_FAULT_STATUS, 0U), "警告灯データの受信は止めない");
    TEST_ASSERT_FALSE_MESSAGE(task_stall_is_stopped(&cfg, TASK_STALL_FAN_RX_ENGINE_STATUS, 0U), "ゲージデータの受信は止めない");
}

/* 期間の両端を含み、対象ではないタスクは止めない */
static void test_is_stopped_boundary_and_target(void) {
    TaskStallConfig cfg;
    task_stall_init(&cfg);
    cfg.target   = TASK_STALL_FAN_RX_FAULT_STATUS;
    cfg.start_ms = 1000U;
    cfg.end_ms   = 2000U;

    TEST_ASSERT_FALSE_MESSAGE(task_stall_is_stopped(&cfg, TASK_STALL_FAN_RX_FAULT_STATUS, 999U),  "start_msの直前は止めない");
    TEST_ASSERT_TRUE_MESSAGE(task_stall_is_stopped(&cfg, TASK_STALL_FAN_RX_FAULT_STATUS, 1000U),  "start_msちょうどは止める");
    TEST_ASSERT_TRUE_MESSAGE(task_stall_is_stopped(&cfg, TASK_STALL_FAN_RX_FAULT_STATUS, 2000U),  "end_msちょうどは止める");
    TEST_ASSERT_FALSE_MESSAGE(task_stall_is_stopped(&cfg, TASK_STALL_FAN_RX_FAULT_STATUS, 2001U), "end_msの直後は止めない");
    TEST_ASSERT_FALSE_MESSAGE(task_stall_is_stopped(&cfg, TASK_STALL_FAN_RX_ENGINE_STATUS, 1500U), "対象ではないタスクは止めない");
}

/* ファイルが無ければfalseで、止めない */
static void test_load_missing_file(void) {
    TaskStallConfig cfg;
    task_stall_init(&cfg);
    remove(TEST_TASK_STALL_FILENAME);

    TEST_ASSERT_FALSE(task_stall_load(&cfg, TEST_TASK_STALL_FILENAME));
    TEST_ASSERT_TRUE(cfg.target == TASK_STALL_NONE);
}

/* 2つのTARGETと期間を読み込む。未知のキー・数値以外・負の値の行は無視する */
static void test_load_valid_file(void) {
    TaskStallConfig cfg;
    task_stall_init(&cfg);
    write_raw(TEST_TASK_STALL_FILENAME, "TARGET=FAN_RX_ENGINE_STATUS\nSTART_MS=3000\nEND_MS=abc\nEND_MS=8000\nSTART_MS=-1\nFOO=1\n");

    TEST_ASSERT_TRUE(task_stall_load(&cfg, TEST_TASK_STALL_FILENAME));
    TEST_ASSERT_TRUE(cfg.target == TASK_STALL_FAN_RX_ENGINE_STATUS);
    TEST_ASSERT_EQUAL_UINT32(3000U, cfg.start_ms);
    TEST_ASSERT_EQUAL_UINT32(8000U, cfg.end_ms);

    task_stall_init(&cfg);
    write_raw(TEST_TASK_STALL_FILENAME, "TARGET=FAN_RX_FAULT_STATUS\nSTART_MS=0\nEND_MS=500\n");
    TEST_ASSERT_TRUE(task_stall_load(&cfg, TEST_TASK_STALL_FILENAME));
    TEST_ASSERT_TRUE(cfg.target == TASK_STALL_FAN_RX_FAULT_STATUS);
    remove(TEST_TASK_STALL_FILENAME);
}

/* TARGETが無い・不正なら止めない（メッセージの名前のFAULT_STATUSもタスクの名前ではないので不正） */
static void test_load_invalid_target(void) {
    TaskStallConfig cfg;
    task_stall_init(&cfg);
    write_raw(TEST_TASK_STALL_FILENAME, "TARGET=FAULT_STATUS\nSTART_MS=0\nEND_MS=5000\n");
    TEST_ASSERT_FALSE(task_stall_load(&cfg, TEST_TASK_STALL_FILENAME));
    TEST_ASSERT_TRUE(cfg.target == TASK_STALL_NONE);

    write_raw(TEST_TASK_STALL_FILENAME, "START_MS=0\nEND_MS=5000\n");
    TEST_ASSERT_FALSE(task_stall_load(&cfg, TEST_TASK_STALL_FILENAME));
    TEST_ASSERT_TRUE(cfg.target == TASK_STALL_NONE);
    remove(TEST_TASK_STALL_FILENAME);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_stops_nothing);
    RUN_TEST(test_is_stopped_boundary_and_target);
    RUN_TEST(test_load_missing_file);
    RUN_TEST(test_load_valid_file);
    RUN_TEST(test_load_invalid_target);
    return UNITY_END();
}
