/* -std=c11だけではnanosleep()が見えないため、POSIX機能テストマクロをヘッダより前に定義する
   （src/timer.cのclock_gettimeと理由は同じ） */
#define _POSIX_C_SOURCE 200809L

#include "unity.h"
#include "can.h"
#include <time.h>   /* nanosleep() */

void setUp(void) {}
void tearDown(void) {}

/* テスト内で指定ミリ秒だけ待つ。timestamp_msはミリ秒単位のため、送信を複数回に分けて
   「新しいフレームか」を区別させたいテスト（Timeout解除の確認等）で使う */
static void sleep_ms(long ms) {
    struct timespec duration = { .tv_sec = 0, .tv_nsec = ms * 1000000L };
    (void)nanosleep(&duration, NULL);
}

/* can_bus_init直後は、全メッセージがvalid=false（まだ一度も送信されていない）ことを確認する */
static void test_bus_init_frames_invalid(void) {
    CanBus bus;
    can_bus_init(&bus);

    for (int i = 0; i < (int)CAN_MSG_COUNT; i++) {
        TEST_ASSERT_FALSE_MESSAGE(bus.frames[i].valid, "init直後は全メッセージがvalid=false");
    }
}

/* can_monitor_init直後は、全メッセージがCAN_LINK_OK・カウンタ0であることを確認する */
static void test_monitor_init_default_state(void) {
    CanMonitor mon;
    can_monitor_init(&mon);

    for (int i = 0; i < (int)CAN_MSG_COUNT; i++) {
        TEST_ASSERT_TRUE_MESSAGE(mon.state[i] == CAN_LINK_OK, "init直後は全メッセージCAN_LINK_OK");
        TEST_ASSERT_EQUAL_MESSAGE(0, mon.bad_count[i], "init直後はbad_countが0");
        TEST_ASSERT_EQUAL_MESSAGE(0, mon.good_count[i], "init直後はgood_countが0");
    }
}

/* 送信した値がそのまま受信側で復元され、CAN_LINK_OKのままであることを確認する */
static void test_engine_status_roundtrip(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    VehicleSensorData data = { .speed = 90, .rpm = 4500, .temperature = 70 };
    can_send_engine_status(&bus, &data);

    CanEngineStatus status;
    CanLinkState state = can_receive_engine_status(&mon, &bus, &status);

    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "有効なフレームを受信すればCAN_LINK_OK");
    TEST_ASSERT_EQUAL_MESSAGE(data.speed, status.speed, "speedがそのまま復元される");
    TEST_ASSERT_EQUAL_MESSAGE(data.rpm, status.rpm, "rpm（2byte）がそのまま復元される");
    TEST_ASSERT_EQUAL_MESSAGE(data.temperature, status.temperature, "temperatureがそのまま復元される");
}

/* 一度も送信されないまま受信を3回連続で呼ぶと、Timeoutとして確定しCAN_LINK_LOSTになることを確認する */
static void test_engine_status_timeout_after_missed_receptions(void) {
    CanBus bus;
    can_bus_init(&bus);   /* 一度も送信しない = 常にvalid=false */
    CanMonitor mon;
    can_monitor_init(&mon);

    CanEngineStatus status;
    CanLinkState state;

    state = can_receive_engine_status(&mon, &bus, &status);
    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "1回目の未受信ではまだLOSTにならない");

    state = can_receive_engine_status(&mon, &bus, &status);
    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "2回目の未受信でもまだLOSTにならない");

    state = can_receive_engine_status(&mon, &bus, &status);
    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_LOST, "3回連続で未受信ならCAN_LINK_LOSTに確定する");
}

/* 値域外のデータ（Invalid Data）を3回連続で受信すると、CAN_LINK_LOSTになることを確認する */
static void test_engine_status_invalid_data_confirms_lost(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    /* speed=200はuint8_tには収まるが、validate.cの物理的な値域(0〜120)を外れる */
    VehicleSensorData invalid_data = { .speed = 200, .rpm = 3000, .temperature = 50 };
    CanEngineStatus status;
    CanLinkState state = CAN_LINK_OK;

    for (int i = 0; i < 3; i++) {
        sleep_ms(2);   /* timestamp_msを毎回変えるため、送信ごとに実時間を進める */
        can_send_engine_status(&bus, &invalid_data);
        state = can_receive_engine_status(&mon, &bus, &status);
    }

    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_LOST, "値域外データを3回連続受信するとCAN_LINK_LOSTに確定する");
}

/* CAN_LINK_LOST確定後、有効なフレームを3回連続受信すればCAN_LINK_OKに復帰することを確認する */
static void test_engine_status_recovers_after_valid_receptions(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    CanEngineStatus status;
    CanLinkState state;

    /* 未送信を3回連続でCAN_LINK_LOSTに確定させる（前提づくり） */
    for (int i = 0; i < 3; i++) {
        state = can_receive_engine_status(&mon, &bus, &status);
    }
    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_LOST, "前提: LOSTに確定済み");

    VehicleSensorData valid_data = { .speed = 60, .rpm = 3000, .temperature = 60 };
    for (int i = 0; i < 3; i++) {
        sleep_ms(2);
        can_send_engine_status(&bus, &valid_data);
        state = can_receive_engine_status(&mon, &bus, &status);
    }

    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "有効なフレームを3回連続受信すればCAN_LINK_OKに復帰する");
}

/* can_monitor_restart（エンジンのかけ直し）は、途絶の連続回数と状態だけを戻し、送信時刻の記録と最後に受け取った値は
   残すことを確認する。送信時刻の記録が残るので、OFF前のフレームが残ったbusを読んでも新しいフレームとは見なさず、
   かけ直した後に受け取れない回が3回続いたときに、改めて途絶と確定する */
static void test_monitor_restart_keeps_timestamp(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    VehicleSensorData data = { .speed = 60, .rpm = 3000, .temperature = 60 };
    can_send_engine_status(&bus, &data);
    CanEngineStatus status;
    CanLinkState state = can_receive_engine_status(&mon, &bus, &status);   /* 受け取る */
    for (int i = 0; i < 2; i++) {
        state = can_receive_engine_status(&mon, &bus, &status);           /* 2回受け取れない（あと1回で途絶） */
    }
    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "前提: 受け取れない回が2回ではまだOK");
    uint32_t seen = mon.last_seen_timestamp_ms[CAN_MSG_ENGINE_STATUS];

    can_monitor_restart(&mon);
    TEST_ASSERT_EQUAL_MESSAGE(0, mon.bad_count[CAN_MSG_ENGINE_STATUS], "かけ直し後: 受け取れない回数は0に戻る");
    TEST_ASSERT_EQUAL_MESSAGE(seen, mon.last_seen_timestamp_ms[CAN_MSG_ENGINE_STATUS], "かけ直し後: 送信時刻の記録は残る");
    TEST_ASSERT_EQUAL_MESSAGE(60, mon.last_engine_status.speed, "かけ直し後: 最後に受け取った値は残る");

    for (int i = 0; i < 2; i++) {
        state = can_receive_engine_status(&mon, &bus, &status);           /* busにはOFF前のフレームが残ったまま */
    }
    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "かけ直し後の2回目まではOK（切る前の2回を足さない）");
    state = can_receive_engine_status(&mon, &bus, &status);
    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_LOST, "かけ直し後の3回目で途絶と確定する（古いフレームを新しいと見誤らない）");
}

/* 途絶と確定した後にかけ直すと、途絶の状態がOKに戻る（起動したときと同じ）ことを確認する */
static void test_monitor_restart_clears_lost(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    CanEngineStatus status;
    CanLinkState state = CAN_LINK_OK;
    for (int i = 0; i < 3; i++) {
        state = can_receive_engine_status(&mon, &bus, &status);
    }
    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_LOST, "前提: LOSTに確定済み");

    can_monitor_restart(&mon);
    TEST_ASSERT_TRUE_MESSAGE(mon.state[CAN_MSG_ENGINE_STATUS] == CAN_LINK_OK, "かけ直し後: 途絶の状態はOKに戻る");
    TEST_ASSERT_EQUAL_MESSAGE(0, mon.good_count[CAN_MSG_ENGINE_STATUS], "かけ直し後: 復帰の回数も0に戻る");
}

/* 送ったセンサ別の確定異常フラグが、警告灯データのビットフラグとして正しく復元されることを確認する */
static void test_fault_status_roundtrip(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    CanFaultStatus sent = { .fault_speed = false, .fault_rpm = true, .fault_temp = false };   /* RPMだけ確定異常 */
    can_send_fault_status(&bus, &sent);

    CanFaultStatus status;
    CanLinkState state = can_receive_fault_status(&mon, &bus, &status);

    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "有効なフレームを受信すればCAN_LINK_OK");
    TEST_ASSERT_FALSE_MESSAGE(status.fault_speed, "Speedは異常なしのまま復元される");
    TEST_ASSERT_TRUE_MESSAGE(status.fault_rpm, "RPMの確定異常フラグが復元される");
    TEST_ASSERT_FALSE_MESSAGE(status.fault_temp, "Tempは異常なしのまま復元される");
    TEST_ASSERT_FALSE_MESSAGE(status.overheat, "高温の通知は無しのまま復元される");
}

/* 高温の通知（Phase27）は、センサの確定異常とは別のビットで往復することを確認する */
static void test_fault_status_overheat_roundtrip(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    CanFaultStatus sent = { .fault_speed = false, .fault_rpm = false, .fault_temp = false, .overheat = true };
    can_send_fault_status(&bus, &sent);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(CAN_FAULT_BIT_OVERHEAT, bus.frames[CAN_MSG_FAULT_STATUS].data[0],
          "高温の通知だけなら、高温のビットだけが立つ");

    CanFaultStatus status;
    (void)can_receive_fault_status(&mon, &bus, &status);
    TEST_ASSERT_TRUE_MESSAGE(status.overheat, "高温の通知が復元される");
    TEST_ASSERT_FALSE_MESSAGE(status.fault_temp, "水温センサの確定異常とは別に扱われる");
}

/* 警告灯データも、一度も送信されないまま受信を3回連続で呼ぶとCAN_LINK_LOSTになることを確認する */
static void test_fault_status_timeout_after_missed_receptions(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    CanFaultStatus status;
    CanLinkState state;

    for (int i = 0; i < 3; i++) {
        state = can_receive_fault_status(&mon, &bus, &status);
    }

    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_LOST, "3回連続で未受信ならCAN_LINK_LOSTに確定する");
}

/* ファン出力ONが、ファン状態のbit0として正しく復元されることを確認する（Phase26） */
static void test_fan_status_roundtrip_on(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    can_send_fan_status(&bus, true);

    CanFanStatus status;
    CanLinkState state = can_receive_fan_status(&mon, &bus, &status);

    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "有効なフレームを受信すればCAN_LINK_OK");
    TEST_ASSERT_TRUE_MESSAGE(status.fan_on, "ファンONが復元される");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(CAN_FAN_BIT_ON, bus.frames[CAN_MSG_FAN_STATUS].data[0], "ONはbit0だけが立つ");
}

/* ファン出力OFFが復元されることを確認する。初期値(false)と区別するため、一度ONを受信してからOFFを送る */
static void test_fan_status_roundtrip_off(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    CanFanStatus status;
    can_send_fan_status(&bus, true);
    (void)can_receive_fan_status(&mon, &bus, &status);

    sleep_ms(5);   /* 前回と送信時刻(timestamp_ms)を変え、新しいフレームとして受信させる */
    can_send_fan_status(&bus, false);
    CanLinkState state = can_receive_fan_status(&mon, &bus, &status);

    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "有効なフレームを受信すればCAN_LINK_OK");
    TEST_ASSERT_FALSE_MESSAGE(status.fan_on, "ON受信後にOFFを受信すればOFFになる");
}

/* ファン状態も、一度も送信されないまま受信を3回連続で呼ぶとCAN_LINK_LOSTになることを確認する */
static void test_fan_status_timeout_after_missed_receptions(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    CanFanStatus status;
    CanLinkState state;

    for (int i = 0; i < 2; i++) {
        state = can_receive_fan_status(&mon, &bus, &status);
    }
    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "2回連続の未受信ではまだCAN_LINK_OK");

    state = can_receive_fan_status(&mon, &bus, &status);
    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_LOST, "3回連続で未受信ならCAN_LINK_LOSTに確定する");
}

/* can_bus_init直後は、診断の要求・応答もvalid=falseで、読んでも何も受け取らないことを確認する（Phase30） */
static void test_diag_init_nothing_to_receive(void) {
    CanBus bus;
    can_bus_init(&bus);
    uint32_t last_seen = 0U;
    uint8_t  payload[CAN_DIAG_SF_MAX_LEN];
    uint8_t  len = 0U;

    TEST_ASSERT_FALSE_MESSAGE(bus.diag_request.valid, "init直後は要求がvalid=false");
    TEST_ASSERT_FALSE_MESSAGE(bus.diag_response.valid, "init直後は応答がvalid=false");
    TEST_ASSERT_FALSE_MESSAGE(can_receive_diag_request(&bus, &last_seen, payload, &len), "送られていなければ受け取らない");
}

/* 要求を1フレームで送ると、ID・DLC・data[0]（長さ）が決めた形になり、中身がそのまま読めることを確認する。
   周期のメッセージの枠と応答の枠には書かれないことも確認する（Phase30） */
static void test_diag_request_roundtrip(void) {
    CanBus bus;
    can_bus_init(&bus);
    const uint8_t request[] = { 0x19U, 0x01U, 0x21U };

    TEST_ASSERT_TRUE_MESSAGE(can_send_diag_request(&bus, request, 3U), "3バイトは1フレームで送れる");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(CAN_ID_DIAG_REQUEST, bus.diag_request.id, "要求のIDは0x7E0");
    TEST_ASSERT_EQUAL_MESSAGE(4, bus.diag_request.dlc, "DLCは長さの1バイト＋中身3バイト");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x03U, bus.diag_request.data[0], "data[0]はSingle Frame（上位4ビット0）と長さ3");
    TEST_ASSERT_FALSE_MESSAGE(bus.diag_response.valid, "応答の枠には書かれない");
    for (int i = 0; i < (int)CAN_MSG_COUNT; i++) {
        TEST_ASSERT_FALSE_MESSAGE(bus.frames[i].valid, "周期のメッセージの枠には書かれない");
    }

    uint32_t last_seen = 0U;
    uint8_t  payload[CAN_DIAG_SF_MAX_LEN];
    uint8_t  len = 0U;
    TEST_ASSERT_TRUE_MESSAGE(can_receive_diag_request(&bus, &last_seen, payload, &len), "新しい要求を受け取る");
    TEST_ASSERT_EQUAL_MESSAGE(3, len, "受け取ったバイト数は3");
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(request, payload, 3, "中身がそのまま読める");
}

/* 応答は0x7E8で送られ、要求の枠には書かれないことを確認する（Phase30） */
static void test_diag_response_roundtrip(void) {
    CanBus bus;
    can_bus_init(&bus);
    const uint8_t response[] = { 0x7FU, 0x19U, 0x12U };

    TEST_ASSERT_TRUE_MESSAGE(can_send_diag_response(&bus, response, 3U), "3バイトは1フレームで送れる");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(CAN_ID_DIAG_RESPONSE, bus.diag_response.id, "応答のIDは0x7E8");
    TEST_ASSERT_FALSE_MESSAGE(bus.diag_request.valid, "要求の枠には書かれない");

    uint32_t last_seen = 0U;
    uint8_t  payload[CAN_DIAG_SF_MAX_LEN];
    uint8_t  len = 0U;
    TEST_ASSERT_TRUE_MESSAGE(can_receive_diag_response(&bus, &last_seen, payload, &len), "新しい応答を受け取る");
    TEST_ASSERT_EQUAL_MESSAGE(3, len, "受け取ったバイト数は3");
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(response, payload, 3, "中身がそのまま読める");
}

/* 同じフレームは1回しか受け取らず、新しく送られたら再び受け取ることを確認する（Phase30） */
static void test_diag_request_received_once(void) {
    CanBus bus;
    can_bus_init(&bus);
    const uint8_t request[] = { 0x14U, 0xFFU, 0xFFU, 0xFFU };
    uint32_t last_seen = 0U;
    uint8_t  payload[CAN_DIAG_SF_MAX_LEN];
    uint8_t  len = 0U;

    (void)can_send_diag_request(&bus, request, 4U);
    TEST_ASSERT_TRUE_MESSAGE(can_receive_diag_request(&bus, &last_seen, payload, &len), "1回目は受け取る");
    TEST_ASSERT_FALSE_MESSAGE(can_receive_diag_request(&bus, &last_seen, payload, &len), "同じフレームは2回受け取らない");

    sleep_ms(5);   /* 前回と送信時刻(timestamp_ms)を変え、新しいフレームとして受け取らせる */
    (void)can_send_diag_request(&bus, request, 4U);
    TEST_ASSERT_TRUE_MESSAGE(can_receive_diag_request(&bus, &last_seen, payload, &len), "新しく送られたら受け取る");
}

/* 長さ0と、1フレームに入らない8バイトは送らず、枠も書き換えないことを確認する（Phase30） */
static void test_diag_send_rejects_bad_length(void) {
    CanBus bus;
    can_bus_init(&bus);
    const uint8_t request[8] = { 0x19U, 0x02U, 0x09U, 0U, 0U, 0U, 0U, 0U };

    TEST_ASSERT_FALSE_MESSAGE(can_send_diag_request(&bus, request, 0U), "長さ0は送らない");
    TEST_ASSERT_FALSE_MESSAGE(can_send_diag_request(&bus, request, 8U), "8バイトは1フレームに入らないので送らない");
    TEST_ASSERT_FALSE_MESSAGE(bus.diag_request.valid, "送らなかったときは枠を書き換えない");
}

/* 短い要求で上書きしたとき、前の長い要求の残りが後ろのバイトに残らないことを確認する（Phase30） */
static void test_diag_send_clears_unused_bytes(void) {
    CanBus bus;
    can_bus_init(&bus);
    const uint8_t long_request[]  = { 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U };
    const uint8_t short_request[] = { 0x3EU };

    (void)can_send_diag_request(&bus, long_request, 7U);
    TEST_ASSERT_EQUAL_MESSAGE(8, bus.diag_request.dlc, "7バイトならDLCは8");
    (void)can_send_diag_request(&bus, short_request, 1U);

    TEST_ASSERT_EQUAL_MESSAGE(2, bus.diag_request.dlc, "1バイトならDLCは2");
    for (int i = 2; i < 8; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00U, bus.diag_request.data[i], "使わない後ろのバイトは0");
    }
}

/* 1フレームとして正しくない形（Single Frame以外・長さ0・DLCに収まらない長さ）は受け取らず、
   同じフレームを読み直さないよう受け取り済みにすることを確認する（Phase30） */
static void test_diag_receive_rejects_bad_frame(void) {
    CanBus bus;
    can_bus_init(&bus);
    uint8_t  payload[CAN_DIAG_SF_MAX_LEN];
    uint8_t  len = 0U;

    /* 上位4ビットが2（Consecutive Frame：分割送信の2つ目以降のフレーム）。この段階では扱わない。
       下位4ビットは連番の1で、長さとして読むと1になり、長さの確認だけでは見分けられない */
    bus.diag_request.id           = CAN_ID_DIAG_REQUEST;
    bus.diag_request.dlc          = 8U;
    bus.diag_request.data[0]      = 0x21U;
    bus.diag_request.timestamp_ms = 100U;
    bus.diag_request.valid        = true;
    uint32_t last_seen = 0U;
    TEST_ASSERT_FALSE_MESSAGE(can_receive_diag_request(&bus, &last_seen, payload, &len), "Single Frame以外は受け取らない");
    TEST_ASSERT_EQUAL_MESSAGE(100, last_seen, "受け取らなかったフレームも受け取り済みとして記録する");

    /* 長さ0 */
    bus.diag_request.data[0]      = 0x00U;
    bus.diag_request.timestamp_ms = 200U;
    TEST_ASSERT_FALSE_MESSAGE(can_receive_diag_request(&bus, &last_seen, payload, &len), "長さ0は受け取らない");

    /* 長さ5なのにDLCが3（長さの1バイト＋中身2バイト分しか無い） */
    bus.diag_request.dlc          = 3U;
    bus.diag_request.data[0]      = 0x05U;
    bus.diag_request.timestamp_ms = 300U;
    TEST_ASSERT_FALSE_MESSAGE(can_receive_diag_request(&bus, &last_seen, payload, &len), "DLCに収まらない長さは受け取らない");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_bus_init_frames_invalid);
    RUN_TEST(test_monitor_init_default_state);
    RUN_TEST(test_engine_status_roundtrip);
    RUN_TEST(test_engine_status_timeout_after_missed_receptions);
    RUN_TEST(test_engine_status_invalid_data_confirms_lost);
    RUN_TEST(test_engine_status_recovers_after_valid_receptions);
    RUN_TEST(test_monitor_restart_keeps_timestamp);
    RUN_TEST(test_monitor_restart_clears_lost);
    RUN_TEST(test_fault_status_roundtrip);
    RUN_TEST(test_fault_status_overheat_roundtrip);
    RUN_TEST(test_fault_status_timeout_after_missed_receptions);
    RUN_TEST(test_fan_status_roundtrip_on);
    RUN_TEST(test_fan_status_roundtrip_off);
    RUN_TEST(test_fan_status_timeout_after_missed_receptions);
    RUN_TEST(test_diag_init_nothing_to_receive);
    RUN_TEST(test_diag_request_roundtrip);
    RUN_TEST(test_diag_response_roundtrip);
    RUN_TEST(test_diag_request_received_once);
    RUN_TEST(test_diag_send_rejects_bad_length);
    RUN_TEST(test_diag_send_clears_unused_bytes);
    RUN_TEST(test_diag_receive_rejects_bad_frame);

    return UNITY_END();
}
