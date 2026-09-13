#include "unity.h"
#include "debounce.h"

void setUp(void) {}
void tearDown(void) {}

/* 確定回数(3)に届く前はfalse(未確定)のままであることを確認する */
static void test_not_confirmed_before_threshold(void) {
    uint8_t bad_count = 0U, good_count = 0U;
    bool confirmed = false;

    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    TEST_ASSERT_FALSE_MESSAGE(confirmed, "bad1回目ではまだ確定しない");

    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    TEST_ASSERT_FALSE_MESSAGE(confirmed, "bad2回目でもまだ確定しない");
}

/* 確定回数(3)に達した瞬間にtrue(確定)になることを確認する */
static void test_confirms_at_threshold(void) {
    uint8_t bad_count = 0U, good_count = 0U;
    bool confirmed = false;

    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    TEST_ASSERT_FALSE_MESSAGE(confirmed, "3回目の直前まではfalse");

    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    TEST_ASSERT_TRUE_MESSAGE(confirmed, "bad3回連続でtrueに確定する");
}

/* is_badが連続せず途中で途切れると、連続回数が数え直しになることを確認する */
static void test_resets_on_interruption(void) {
    uint8_t bad_count = 0U, good_count = 0U;
    bool confirmed = false;

    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);   /* bad 1回目 */
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);   /* bad 2回目 */
    confirmed = debounce_update(confirmed, false, false, &bad_count, &good_count, 3U, 3U);  /* 途中で途切れる */
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);   /* 途切れた後の1回目 */
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);   /* 途切れた後の2回目 */

    TEST_ASSERT_FALSE_MESSAGE(confirmed, "通算4回でも連続が途切れていればまだ確定しない");
}

/* 復帰回数(3)に届く前はtrue(確定)のままであることを確認する */
static void test_recovery_not_confirmed_before_threshold(void) {
    uint8_t bad_count = 0U, good_count = 0U;
    bool confirmed = false;

    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    TEST_ASSERT_TRUE_MESSAGE(confirmed, "前提: 確定済み");

    confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 3U, 3U);   /* good 1回目 */
    confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 3U, 3U);   /* good 2回目 */
    TEST_ASSERT_TRUE_MESSAGE(confirmed, "good2回ではまだ復帰しない");
}

/* 復帰回数(3)に達した瞬間にfalse(復帰)になり、bad_countも0に戻ることを確認する */
static void test_recovery_confirms_at_threshold(void) {
    uint8_t bad_count = 0U, good_count = 0U;
    bool confirmed = false;

    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);

    confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 3U, 3U);
    confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 3U, 3U);
    confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 3U, 3U);

    TEST_ASSERT_FALSE_MESSAGE(confirmed, "good3回連続で復帰する");
    TEST_ASSERT_EQUAL_MESSAGE(0, bad_count, "復帰時にbad_countも0に戻る");
}

/* 確定中にis_goodがfalse（かつis_badもfalse、例:中間状態）だと、good_countが数え直しになることを確認する */
static void test_recovery_resets_on_non_good(void) {
    uint8_t bad_count = 0U, good_count = 0U;
    bool confirmed = false;

    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 3U, 3U);

    confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 3U, 3U);   /* good 1回目 */
    confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 3U, 3U);   /* good 2回目 */
    confirmed = debounce_update(confirmed, false, false, &bad_count, &good_count, 3U, 3U);  /* 中間状態(is_good=false)で途切れる */
    confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 3U, 3U);   /* 途切れた後の1回目 */
    confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 3U, 3U);   /* 途切れた後の2回目 */

    TEST_ASSERT_TRUE_MESSAGE(confirmed, "is_good=falseで連続が途切れると復帰しない");
}

/* confirm_threshold/recover_thresholdが異なる値でも独立して動作することを確認する
   （faultmgr.cとcan.cで異なる回数を使いたくなった場合にも対応できることの確認） */
static void test_independent_thresholds(void) {
    uint8_t bad_count = 0U, good_count = 0U;
    bool confirmed = false;

    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 2U, 5U);
    TEST_ASSERT_FALSE_MESSAGE(confirmed, "confirm_threshold=2の1回目ではまだ確定しない");
    confirmed = debounce_update(confirmed, true, false, &bad_count, &good_count, 2U, 5U);
    TEST_ASSERT_TRUE_MESSAGE(confirmed, "confirm_threshold=2の2回目で確定する");

    for (int i = 0; i < 4; i++) {
        confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 2U, 5U);
    }
    TEST_ASSERT_TRUE_MESSAGE(confirmed, "recover_threshold=5の4回目ではまだ復帰しない");

    confirmed = debounce_update(confirmed, false, true, &bad_count, &good_count, 2U, 5U);
    TEST_ASSERT_FALSE_MESSAGE(confirmed, "recover_threshold=5の5回目で復帰する");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_not_confirmed_before_threshold);
    RUN_TEST(test_confirms_at_threshold);
    RUN_TEST(test_resets_on_interruption);
    RUN_TEST(test_recovery_not_confirmed_before_threshold);
    RUN_TEST(test_recovery_confirms_at_threshold);
    RUN_TEST(test_recovery_resets_on_non_good);
    RUN_TEST(test_independent_thresholds);

    return UNITY_END();
}
