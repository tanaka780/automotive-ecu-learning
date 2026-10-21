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

    return UNITY_END();
}
