/* -std=c11だけではnanosleep()が見えないため、POSIX機能テストマクロをヘッダより前に定義する
   （test_can.cと理由は同じ） */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <time.h>   /* nanosleep() */
#include "unity.h"
#include "uds_tester.h"

#define TEST_FILENAME "test_uds_request.txt"

void setUp(void) {}
void tearDown(void) {
    (void)remove(TEST_FILENAME);
}

/* P2（50ms）を過ぎたことを確かめるため、実際に待つ */
static void sleep_ms(long ms) {
    struct timespec duration = { .tv_sec = 0, .tv_nsec = ms * 1000000L };
    (void)nanosleep(&duration, NULL);
}

static void write_file(const char *text) {
    FILE *fp = fopen(TEST_FILENAME, "w");
    TEST_ASSERT_NOT_NULL(fp);
    (void)fputs(text, fp);
    (void)fclose(fp);
}

/* 正しい行だけを書いた順に読み、正しくない行は無視する */
static void test_load_valid_and_invalid_lines(void) {
    write_file("REQ@5=19 01 FF\n"
               "REQ@0=19 01 FF\n"                 /* Nが1未満 */
               "REQ@3x=19 01 FF\n"                /* Nの後ろに余計な文字 */
               "REQ@6=19 GG\n"                    /* 16進数でない */
               "REQ@6=19 100\n"                   /* 0xFFを超える */
               "REQ@6=01 02 03 04 05 06 07 08\n"  /* 1フレームに入らない8バイト */
               "REQ@6=\n"                         /* バイトが無い */
               "MODE=FIXED\n"                     /* REQ@でない行 */
               "REQ@12=14 FF FF FF\r\n");          /* 改行がCRLFでも読む */
    UdsTesterScript script;

    TEST_ASSERT_TRUE_MESSAGE(uds_tester_load(&script, TEST_FILENAME), "正しい行があればtrue");
    TEST_ASSERT_EQUAL_MESSAGE(2, script.count, "正しい2行だけを読む");
    TEST_ASSERT_EQUAL_MESSAGE(5, script.at_sample[0], "1行目のN");
    TEST_ASSERT_EQUAL_MESSAGE(3, script.len[0], "1行目は3バイト");
    const uint8_t first[]  = { 0x19U, 0x01U, 0xFFU };
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(first, script.data[0], 3, "1行目のバイト列");
    TEST_ASSERT_EQUAL_MESSAGE(12, script.at_sample[1], "2行目のN");
    TEST_ASSERT_EQUAL_MESSAGE(4, script.len[1], "2行目は4バイト");
    const uint8_t second[] = { 0x14U, 0xFFU, 0xFFU, 0xFFU };
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(second, script.data[1], 4, "2行目のバイト列");
}

/* ファイルが無ければ、何も読まずにfalse */
static void test_load_no_file(void) {
    UdsTesterScript script;
    script.count = 3U;

    TEST_ASSERT_FALSE_MESSAGE(uds_tester_load(&script, "no_such_uds_request.txt"), "ファイルが無ければfalse");
    TEST_ASSERT_EQUAL_MESSAGE(0, script.count, "件数は0");
}

/* ファイルはあっても正しい行が1つも無ければ、false（件数のログも出さない） */
static void test_load_no_valid_line(void) {
    write_file("REQ@0=19 01 FF\n"
               "MODE=FIXED\n");
    UdsTesterScript script;

    TEST_ASSERT_FALSE_MESSAGE(uds_tester_load(&script, TEST_FILENAME), "正しい行が無ければfalse");
    TEST_ASSERT_EQUAL_MESSAGE(0, script.count, "件数は0");
}

/* UDS_TESTER_REQUEST_MAXを超えた分は読まない */
static void test_load_ignores_over_max(void) {
    FILE *fp = fopen(TEST_FILENAME, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < (int)UDS_TESTER_REQUEST_MAX + 2; i++) {
        (void)fprintf(fp, "REQ@%d=19 01 FF\n", i + 1);
    }
    (void)fclose(fp);
    UdsTesterScript script;

    (void)uds_tester_load(&script, TEST_FILENAME);
    TEST_ASSERT_EQUAL_MESSAGE(UDS_TESTER_REQUEST_MAX, script.count, "上限までしか読まない");
}

/* Nのサンプルになるまでは送らず、Nのサンプルで送る */
static void test_sends_at_sample(void) {
    write_file("REQ@3=19 01 FF\n");
    UdsTesterScript script;
    (void)uds_tester_load(&script, TEST_FILENAME);
    CanBus bus;
    can_bus_init(&bus);
    int sample_index = 2;
    UdsTesterContext ctx;
    uds_tester_init(&ctx, &script, &sample_index, &bus);

    uds_tester_run(&ctx);
    TEST_ASSERT_FALSE_MESSAGE(bus.diag_request.valid, "Nより前のサンプルでは送らない");

    sample_index = 3;
    uds_tester_run(&ctx);
    TEST_ASSERT_TRUE_MESSAGE(bus.diag_request.valid, "Nのサンプルで送る");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x03U, bus.diag_request.data[0], "長さ3の1フレーム");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x19U, bus.diag_request.data[1], "サービスID");
    TEST_ASSERT_TRUE_MESSAGE(ctx.waiting, "送ったら応答を待つ");
}

/* 同じNの要求が2つあっても、1つ目の応答を受け取るまで2つ目を送らない（busの上書きを防ぐ） */
static void test_waits_for_response_before_next(void) {
    write_file("REQ@1=14 FF FF FF\n"
               "REQ@1=19 01 FF\n");
    UdsTesterScript script;
    (void)uds_tester_load(&script, TEST_FILENAME);
    CanBus bus;
    can_bus_init(&bus);
    int sample_index = 1;
    UdsTesterContext ctx;
    uds_tester_init(&ctx, &script, &sample_index, &bus);

    uds_tester_run(&ctx);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x14U, bus.diag_request.data[1], "1つ目を送る");
    uds_tester_run(&ctx);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x14U, bus.diag_request.data[1], "応答が来るまで2つ目を送らない");

    const uint8_t response[] = { 0x54U };
    (void)can_send_diag_response(&bus, response, 1U);
    uds_tester_run(&ctx);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x19U, bus.diag_request.data[1], "応答を受け取ったら2つ目を送る");
    TEST_ASSERT_EQUAL_MESSAGE(2, ctx.next, "2つとも送った");
}

/* 応答が来なくても、P2（50ms）を過ぎたら「応答なし」として次を送る */
static void test_gives_up_after_p2(void) {
    write_file("REQ@1=19 01 FF\n"
               "REQ@1=14 FF FF FF\n");
    UdsTesterScript script;
    (void)uds_tester_load(&script, TEST_FILENAME);
    CanBus bus;
    can_bus_init(&bus);
    int sample_index = 1;
    UdsTesterContext ctx;
    uds_tester_init(&ctx, &script, &sample_index, &bus);

    uds_tester_run(&ctx);
    uds_tester_run(&ctx);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x19U, bus.diag_request.data[1], "P2の前は待ち続ける");

    sleep_ms((long)UDS_TESTER_P2_MS + 10L);
    uds_tester_run(&ctx);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x14U, bus.diag_request.data[1], "P2を過ぎたら次を送る");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_load_valid_and_invalid_lines);
    RUN_TEST(test_load_no_file);
    RUN_TEST(test_load_no_valid_line);
    RUN_TEST(test_load_ignores_over_max);
    RUN_TEST(test_sends_at_sample);
    RUN_TEST(test_waits_for_response_before_next);
    RUN_TEST(test_gives_up_after_p2);

    return UNITY_END();
}
