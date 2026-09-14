#include "unity.h"
#include "dtc_status.h"

void setUp(void) {}
void tearDown(void) {}

/* まだ一度も異常になっていなければNONEのまま、発生回数も0のままであることを確認する */
static void test_stays_none_without_bad(void) {
    uint8_t count = 0U;
    DtcStatus status = DTC_NONE;

    dtc_status_update(&count, &status, false, false);

    TEST_ASSERT_TRUE_MESSAGE(status == DTC_NONE, "異常が一度も無ければNONEのまま");
    TEST_ASSERT_EQUAL_MESSAGE(0, count, "異常が一度も無ければ発生回数は0のまま");
}

/* 異常に入った瞬間（エッジ）だけ発生回数が増え、ACTIVEになることを確認する */
static void test_counts_on_edge_into_bad(void) {
    uint8_t count = 0U;
    DtcStatus status = DTC_NONE;

    dtc_status_update(&count, &status, false, true);   /* NONE→異常発生（エッジ） */

    TEST_ASSERT_EQUAL_MESSAGE(1, count, "異常発生の瞬間に発生回数が1になる");
    TEST_ASSERT_TRUE_MESSAGE(status == DTC_ACTIVE, "異常発生の瞬間にACTIVEになる");
}

/* 異常が継続している間は、発生回数を二重カウントしないことを確認する */
static void test_does_not_double_count_while_continuing(void) {
    uint8_t count = 0U;
    DtcStatus status = DTC_NONE;

    dtc_status_update(&count, &status, false, true);   /* 1回目（エッジ） */
    dtc_status_update(&count, &status, true, true);    /* 継続中 */
    dtc_status_update(&count, &status, true, true);    /* 継続中 */

    TEST_ASSERT_EQUAL_MESSAGE(1, count, "異常継続中は発生回数を増やさない");
    TEST_ASSERT_TRUE_MESSAGE(status == DTC_ACTIVE, "異常継続中はACTIVEのまま");
}

/* 異常から解消された瞬間にHISTORYへ遷移することを確認する */
static void test_transitions_to_history_on_recovery(void) {
    uint8_t count = 0U;
    DtcStatus status = DTC_NONE;

    dtc_status_update(&count, &status, false, true);   /* 発生 */
    dtc_status_update(&count, &status, true, false);   /* 解消 */

    TEST_ASSERT_TRUE_MESSAGE(status == DTC_HISTORY, "解消の瞬間にHISTORYへ遷移する");
    TEST_ASSERT_EQUAL_MESSAGE(1, count, "解消してもそれまでの発生回数は変わらない");
}

/* HISTORY中は異常が解消されたままでも状態が変わらないことを確認する */
static void test_history_persists_while_not_bad(void) {
    uint8_t count = 0U;
    DtcStatus status = DTC_NONE;

    dtc_status_update(&count, &status, false, true);   /* 発生 */
    dtc_status_update(&count, &status, true, false);   /* 解消→HISTORY */
    dtc_status_update(&count, &status, false, false);  /* 解消継続 */

    TEST_ASSERT_TRUE_MESSAGE(status == DTC_HISTORY, "解消が続く間はHISTORYのまま");
    TEST_ASSERT_EQUAL_MESSAGE(1, count, "解消が続く間は発生回数も変わらない");
}

/* HISTORY後に再発すると、発生回数が増えて再びACTIVEになることを確認する */
static void test_counts_again_on_reoccurrence(void) {
    uint8_t count = 0U;
    DtcStatus status = DTC_NONE;

    dtc_status_update(&count, &status, false, true);   /* 1回目発生 */
    dtc_status_update(&count, &status, true, false);   /* 解消→HISTORY */
    dtc_status_update(&count, &status, false, true);   /* 再発生（エッジ） */

    TEST_ASSERT_EQUAL_MESSAGE(2, count, "再発生で発生回数が2に増える");
    TEST_ASSERT_TRUE_MESSAGE(status == DTC_ACTIVE, "再発生で再びACTIVEになる");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_stays_none_without_bad);
    RUN_TEST(test_counts_on_edge_into_bad);
    RUN_TEST(test_does_not_double_count_while_continuing);
    RUN_TEST(test_transitions_to_history_on_recovery);
    RUN_TEST(test_history_persists_while_not_bad);
    RUN_TEST(test_counts_again_on_reoccurrence);

    return UNITY_END();
}
