#include "unity.h"
#include "can.h"
#include "can_diag.h"

void setUp(void) {}
void tearDown(void) {}

/* 初期化直後は全メッセージがNONE・発生回数0であることを確認する */
static void test_init_state(void) {
    CanDtcRecord rec;
    can_diag_init(&rec);

    TEST_ASSERT_TRUE_MESSAGE(rec.status[CAN_MSG_ENGINE_STATUS] == DTC_NONE, "初期化直後はEngineStatusもNONE");
    TEST_ASSERT_EQUAL_MESSAGE(0, rec.count[CAN_MSG_ENGINE_STATUS], "初期化直後はEngineStatusの発生回数も0");
    TEST_ASSERT_TRUE_MESSAGE(rec.status[CAN_MSG_FAULT_STATUS] == DTC_NONE, "初期化直後はFaultStatusもNONE");
    TEST_ASSERT_EQUAL_MESSAGE(0, rec.count[CAN_MSG_FAULT_STATUS], "初期化直後はFaultStatusの発生回数も0");
}

/* OK→LOSTへの遷移（エッジ）で発生回数が増え、ACTIVEになることを確認する */
static void test_counts_on_link_lost(void) {
    CanDtcRecord rec;
    can_diag_init(&rec);

    can_diag_check(&rec, CAN_MSG_ENGINE_STATUS, CAN_LINK_OK, CAN_LINK_LOST);

    TEST_ASSERT_EQUAL_MESSAGE(1, rec.count[CAN_MSG_ENGINE_STATUS], "Lost確定の瞬間に発生回数が1になる");
    TEST_ASSERT_TRUE_MESSAGE(rec.status[CAN_MSG_ENGINE_STATUS] == DTC_ACTIVE, "Lost確定の瞬間にACTIVEになる");
}

/* LOSTが継続する間は、発生回数を二重カウントしないことを確認する */
static void test_does_not_double_count_while_lost(void) {
    CanDtcRecord rec;
    can_diag_init(&rec);

    can_diag_check(&rec, CAN_MSG_ENGINE_STATUS, CAN_LINK_OK, CAN_LINK_LOST);
    can_diag_check(&rec, CAN_MSG_ENGINE_STATUS, CAN_LINK_LOST, CAN_LINK_LOST);

    TEST_ASSERT_EQUAL_MESSAGE(1, rec.count[CAN_MSG_ENGINE_STATUS], "Lost継続中は発生回数を増やさない");
}

/* LOST→OKへの復帰でHISTORYへ遷移することを確認する */
static void test_transitions_to_history_on_recovery(void) {
    CanDtcRecord rec;
    can_diag_init(&rec);

    can_diag_check(&rec, CAN_MSG_FAULT_STATUS, CAN_LINK_OK, CAN_LINK_LOST);
    can_diag_check(&rec, CAN_MSG_FAULT_STATUS, CAN_LINK_LOST, CAN_LINK_OK);

    TEST_ASSERT_TRUE_MESSAGE(rec.status[CAN_MSG_FAULT_STATUS] == DTC_HISTORY, "復帰の瞬間にHISTORYへ遷移する");
}

/* EngineStatusとFaultStatusが互いに影響せず独立して記録されることを確認する */
static void test_messages_are_independent(void) {
    CanDtcRecord rec;
    can_diag_init(&rec);

    can_diag_check(&rec, CAN_MSG_ENGINE_STATUS, CAN_LINK_OK, CAN_LINK_LOST);

    TEST_ASSERT_EQUAL_MESSAGE(1, rec.count[CAN_MSG_ENGINE_STATUS], "EngineStatusは発生回数1");
    TEST_ASSERT_EQUAL_MESSAGE(0, rec.count[CAN_MSG_FAULT_STATUS], "FaultStatusは影響を受けず0のまま");
    TEST_ASSERT_TRUE_MESSAGE(rec.status[CAN_MSG_FAULT_STATUS] == DTC_NONE, "FaultStatusは影響を受けずNONEのまま");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init_state);
    RUN_TEST(test_counts_on_link_lost);
    RUN_TEST(test_does_not_double_count_while_lost);
    RUN_TEST(test_transitions_to_history_on_recovery);
    RUN_TEST(test_messages_are_independent);

    return UNITY_END();
}
