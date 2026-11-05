#include "unity.h"
#include "uds.h"

/* 記録は状態区分を直接書いて用意する。uds.cが読むのは状態区分だけで、異常の判定（diag_check）は
   test_diag.cで確かめているため */
static DtcRecord dtc;
static uint8_t   response[UDS_RESPONSE_MAX_LEN];

void setUp(void) {
    diag_init(&dtc);
    for (int i = 0; i < (int)UDS_RESPONSE_MAX_LEN; i++) {
        response[i] = 0xAAU;
    }
}
void tearDown(void) {}

/* 否定応答（7F <サービスID> <NRC>）が返ったことを確かめる */
static void assert_negative(uint8_t len, uint8_t sid, uint8_t nrc) {
    TEST_ASSERT_EQUAL_MESSAGE(3, len, "否定応答は3バイト");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x7FU, response[0], "否定応答は0x7Fから始まる");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(sid, response[1], "2バイト目は要求のサービスID");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(nrc, response[2], "3バイト目は拒否の理由（NRC）");
}

/* 0x19 01に、マスクと重なる記録の件数を返す */
static uint16_t read_count(uint8_t mask) {
    const uint8_t request[] = { 0x19U, 0x01U, mask };
    uint8_t len = uds_handle_request(&dtc, request, 3U, response);
    TEST_ASSERT_EQUAL_MESSAGE(6, len, "0x19 01の肯定応答は6バイト");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x59U, response[0], "肯定応答は0x19+0x40");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x01U, response[1], "サブファンクションをそのまま返す");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x21U, response[2], "対応しているビットはbit0とbit5");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00U, response[3], "DTCの形式はSAE J2012");
    return (uint16_t)(((uint16_t)response[4] << 8) | response[5]);
}

/* 記録が1件も無ければ、どのマスクでも件数は0 */
static void test_read_count_no_record(void) {
    TEST_ASSERT_EQUAL_MESSAGE(0, read_count(0xFFU), "記録なしなら0件");
}

/* 今も異常（ACTIVE）はbit0とbit5、過去の異常（HISTORY）はbit5だけが立つ。
   confirmedDTC（bit3）は使わないので、bit3だけのマスクでは0件になる */
static void test_read_count_by_mask(void) {
    dtc.entries[DTC_ID_SPEED].count        = 1U;
    dtc.entries[DTC_ID_SPEED].status       = DTC_ACTIVE;
    dtc.entries[DTC_ID_TEMP_SENSOR].count  = 2U;
    dtc.entries[DTC_ID_TEMP_SENSOR].status = DTC_HISTORY;

    TEST_ASSERT_EQUAL_MESSAGE(1, read_count(0x01U), "bit0（今も異常）はACTIVEの1件");
    TEST_ASSERT_EQUAL_MESSAGE(2, read_count(0x20U), "bit5（消してから異常になった）はACTIVEとHISTORYの2件");
    TEST_ASSERT_EQUAL_MESSAGE(2, read_count(0xFFU), "全ビットのマスクなら2件");
    TEST_ASSERT_EQUAL_MESSAGE(0, read_count(0x08U), "bit3（confirmedDTC）は使わないので0件");
}

/* bit5は回数ではなく状態区分から作るので、回数が256回目に0へ戻っても数える */
static void test_read_count_uses_status_not_count(void) {
    dtc.entries[DTC_ID_RPM].count  = 0U;   /* 255の次に0へ戻った状態 */
    dtc.entries[DTC_ID_RPM].status = DTC_ACTIVE;

    TEST_ASSERT_EQUAL_MESSAGE(1, read_count(0x20U), "回数が0でもACTIVEならbit5に数える");
}

/* 0x14 FFFFFFで記録を全部消し、0x54を返す */
static void test_clear_all(void) {
    dtc.entries[DTC_ID_OVERHEAT].count  = 3U;
    dtc.entries[DTC_ID_OVERHEAT].status = DTC_HISTORY;
    dtc.freeze_frame.captured           = true;
    const uint8_t request[] = { 0x14U, 0xFFU, 0xFFU, 0xFFU };

    uint8_t len = uds_handle_request(&dtc, request, 4U, response);

    TEST_ASSERT_EQUAL_MESSAGE(1, len, "0x14の肯定応答は1バイト");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x54U, response[0], "肯定応答は0x14+0x40");
    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.entries[DTC_ID_OVERHEAT].count, "回数が0に戻る");
    TEST_ASSERT_TRUE_MESSAGE(dtc.entries[DTC_ID_OVERHEAT].status == DTC_NONE, "状態区分がNONEに戻る");
    TEST_ASSERT_FALSE_MESSAGE(dtc.freeze_frame.captured, "フリーズフレームも消える");
}

/* FFFFFF以外の消去（記録の種類の指定）は受け付けず、記録も消さない */
static void test_clear_other_group_rejected(void) {
    dtc.entries[DTC_ID_SPEED].count  = 1U;
    dtc.entries[DTC_ID_SPEED].status = DTC_HISTORY;
    const uint8_t request[] = { 0x14U, 0x00U, 0x05U, 0x00U };

    assert_negative(uds_handle_request(&dtc, request, 4U, response), 0x14U, 0x31U);
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_SPEED].count, "拒否したときは記録を消さない");
}

/* 長さが合わない要求は0x13で拒否する。0x14は、長さが合わなければ記録を消さない */
static void test_incorrect_length(void) {
    const uint8_t read_sid_only[]  = { 0x19U };
    const uint8_t read_no_mask[]   = { 0x19U, 0x01U };
    const uint8_t read_too_long[]  = { 0x19U, 0x01U, 0x21U, 0x00U };
    const uint8_t clear_short[]    = { 0x14U, 0xFFU, 0xFFU };

    assert_negative(uds_handle_request(&dtc, read_sid_only, 1U, response), 0x19U, 0x13U);
    assert_negative(uds_handle_request(&dtc, read_no_mask, 2U, response), 0x19U, 0x13U);
    assert_negative(uds_handle_request(&dtc, read_too_long, 4U, response), 0x19U, 0x13U);

    dtc.entries[DTC_ID_SPEED].count  = 1U;
    dtc.entries[DTC_ID_SPEED].status = DTC_HISTORY;
    assert_negative(uds_handle_request(&dtc, clear_short, 3U, response), 0x14U, 0x13U);
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_SPEED].count, "長さが合わない消去では記録を消さない");
}

/* 0x19の対応していないサブファンクション（0x02：DTCの一覧）は0x12で拒否する。
   サブファンクションまで読める長さ（2バイト）があれば、0x01用の長さ（3バイト）の確認より先に
   サブファンクションを見るので、2バイトの要求でも0x13ではなく0x12になる */
static void test_subfunction_not_supported(void) {
    const uint8_t request[] = { 0x19U, 0x02U, 0x21U };
    assert_negative(uds_handle_request(&dtc, request, 3U, response), 0x19U, 0x12U);
    assert_negative(uds_handle_request(&dtc, request, 2U, response), 0x19U, 0x12U);
}

/* 対応していないサービス（0x22：データの読み出し）は0x11で拒否する。長さ0なら応答しない */
static void test_service_not_supported_and_empty(void) {
    const uint8_t request[] = { 0x22U, 0xF1U, 0x90U };
    assert_negative(uds_handle_request(&dtc, request, 3U, response), 0x22U, 0x11U);

    TEST_ASSERT_EQUAL_MESSAGE(0, uds_handle_request(&dtc, request, 0U, response), "長さ0なら応答しない");
}

/* busを通した応答：ONなら要求に応答をbusへ書く */
static void test_server_responds_when_on(void) {
    CanBus bus;
    can_bus_init(&bus);
    Ignition ignition = { .current = IGNITION_ON, .previous = IGNITION_ON };
    UdsContext ctx = { .ignition = &ignition, .can_bus = &bus, .dtc = &dtc, .last_request_ms = 0U };
    const uint8_t request[] = { 0x19U, 0x01U, 0xFFU };

    uds_server_run(&ctx);
    TEST_ASSERT_FALSE_MESSAGE(bus.diag_response.valid, "要求が無ければ応答しない");

    (void)can_send_diag_request(&bus, request, 3U);
    uds_server_run(&ctx);

    uint32_t last_seen = 0U;
    uint8_t  payload[CAN_DIAG_SF_MAX_LEN];
    uint8_t  len = 0U;
    TEST_ASSERT_TRUE_MESSAGE(can_receive_diag_response(&bus, &last_seen, payload, &len), "応答がbusに書かれる");
    TEST_ASSERT_EQUAL_MESSAGE(6, len, "0x19 01の肯定応答は6バイト");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x59U, payload[0], "肯定応答");
}

/* OFFの間に届いた要求は捨て、ONになってからもその古い要求には答えない */
static void test_server_drops_request_while_off(void) {
    CanBus bus;
    can_bus_init(&bus);
    Ignition ignition = { .current = IGNITION_OFF, .previous = IGNITION_OFF };
    UdsContext ctx = { .ignition = &ignition, .can_bus = &bus, .dtc = &dtc, .last_request_ms = 0U };
    const uint8_t request[] = { 0x14U, 0xFFU, 0xFFU, 0xFFU };
    dtc.entries[DTC_ID_SPEED].count  = 1U;
    dtc.entries[DTC_ID_SPEED].status = DTC_HISTORY;

    (void)can_send_diag_request(&bus, request, 4U);
    uds_server_run(&ctx);
    TEST_ASSERT_FALSE_MESSAGE(bus.diag_response.valid, "OFFの間は応答しない");
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_SPEED].count, "OFFの間の消去の要求では消さない");

    ignition.previous = IGNITION_OFF;
    ignition.current  = IGNITION_ON;
    uds_server_run(&ctx);
    TEST_ASSERT_FALSE_MESSAGE(bus.diag_response.valid, "ONになっても、OFFの間に届いた要求には答えない");
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.entries[DTC_ID_SPEED].count, "記録は残ったまま");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_read_count_no_record);
    RUN_TEST(test_read_count_by_mask);
    RUN_TEST(test_read_count_uses_status_not_count);
    RUN_TEST(test_clear_all);
    RUN_TEST(test_clear_other_group_rejected);
    RUN_TEST(test_incorrect_length);
    RUN_TEST(test_subfunction_not_supported);
    RUN_TEST(test_service_not_supported_and_empty);
    RUN_TEST(test_server_responds_when_on);
    RUN_TEST(test_server_drops_request_while_off);

    return UNITY_END();
}
