#include "unity.h"
#include "fan_ctrl.h"

void setUp(void) {}
void tearDown(void) {}

/* 閾値の値そのものは書かず、FAN_ON_TEMP/FAN_OFF_TEMPとその±1で境界を確かめる
   （閾値を調整してもテストを書き直さずに済むようにする） */

/* OFFの状態からは、ON閾値に届くまでOFFのまま（閾値の間の下端・上端とも） */
static void test_off_stays_off_below_on_threshold(void) {
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, fan_ctrl_decide(FAN_OFF_TEMP + 1U, FAN_OFF), "閾値の間の下端ではOFFのまま");
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, fan_ctrl_decide(FAN_ON_TEMP - 1U, FAN_OFF), "ON閾値の1つ下ではOFFのまま");
}

/* OFFの状態から、ON閾値ちょうどでONになる */
static void test_off_turns_on_at_on_threshold(void) {
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, fan_ctrl_decide(FAN_ON_TEMP, FAN_OFF), "ON閾値ちょうどでONになる");
}

/* ONの状態からは、OFF閾値に届くまでONのまま（閾値の間の下端・上端とも） */
static void test_on_stays_on_above_off_threshold(void) {
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, fan_ctrl_decide(FAN_ON_TEMP - 1U, FAN_ON), "閾値の間の上端ではONのまま");
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, fan_ctrl_decide(FAN_OFF_TEMP + 1U, FAN_ON), "OFF閾値の1つ上ではONのまま");
}

/* ONの状態から、OFF閾値ちょうどでOFFになる */
static void test_on_turns_off_at_off_threshold(void) {
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, fan_ctrl_decide(FAN_OFF_TEMP, FAN_ON), "OFF閾値ちょうどでOFFになる");
}

/* 閾値の外側では、前回の要求に関係なく決まる（uint8_tの両端も含む） */
static void test_outside_thresholds_ignore_current(void) {
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, fan_ctrl_decide(FAN_OFF_TEMP, FAN_OFF), "OFF閾値ちょうどでOFFのまま");
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, fan_ctrl_decide(FAN_ON_TEMP, FAN_ON), "ON閾値ちょうどでONのまま");
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, fan_ctrl_decide(0U, FAN_ON), "0ではOFF");
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, fan_ctrl_decide(255U, FAN_OFF), "255ではON");
}

/* 水温が上がって下がる一巡で、OFF→ON→ON→ON→OFFになる（閾値付近でON/OFFを繰り返さない） */
static void test_full_cycle(void) {
    FanState state = FAN_OFF;

    state = fan_ctrl_decide(FAN_OFF_TEMP - 5U, state);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state, "低い水温ではOFF");
    state = fan_ctrl_decide(FAN_ON_TEMP, state);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state, "ON閾値でON");
    state = fan_ctrl_decide(FAN_ON_TEMP - 2U, state);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state, "閾値の間に下がってもON");
    state = fan_ctrl_decide(FAN_OFF_TEMP + 1U, state);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state, "OFF閾値の1つ上でもON");
    state = fan_ctrl_decide(FAN_OFF_TEMP, state);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state, "OFF閾値でOFF");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_off_stays_off_below_on_threshold);
    RUN_TEST(test_off_turns_on_at_on_threshold);
    RUN_TEST(test_on_stays_on_above_off_threshold);
    RUN_TEST(test_on_turns_off_at_off_threshold);
    RUN_TEST(test_outside_thresholds_ignore_current);
    RUN_TEST(test_full_cycle);

    return UNITY_END();
}
