/* -std=c11だけではnanosleep()が見えないため、POSIX機能テストマクロをヘッダより前に定義する
   （src/timer.cのclock_gettimeと理由は同じ） */
#define _POSIX_C_SOURCE 200809L

#include "unity.h"
#include "can.h"
#include "faultmgr.h"
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

/* FaultManagerの確定状態が、警告灯データのビットフラグとして正しく復元されることを確認する */
static void test_fault_status_roundtrip(void) {
    CanBus bus;
    can_bus_init(&bus);
    CanMonitor mon;
    can_monitor_init(&mon);

    FaultManager fm;
    faultmgr_init(&fm);
    fm.state[SENSOR_RPM] = FAULT_DEGRADED;   /* RPMだけ確定異常にする（直接組み立て、faultmgr_checkは経由しない） */

    can_send_fault_status(&bus, &fm);

    CanFaultStatus status;
    CanLinkState state = can_receive_fault_status(&mon, &bus, &status);

    TEST_ASSERT_TRUE_MESSAGE(state == CAN_LINK_OK, "有効なフレームを受信すればCAN_LINK_OK");
    TEST_ASSERT_FALSE_MESSAGE(status.fault_speed, "Speedは異常なしのまま復元される");
    TEST_ASSERT_TRUE_MESSAGE(status.fault_rpm, "RPMの確定異常フラグが復元される");
    TEST_ASSERT_FALSE_MESSAGE(status.fault_temp, "Tempは異常なしのまま復元される");
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

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_bus_init_frames_invalid);
    RUN_TEST(test_monitor_init_default_state);
    RUN_TEST(test_engine_status_roundtrip);
    RUN_TEST(test_engine_status_timeout_after_missed_receptions);
    RUN_TEST(test_engine_status_invalid_data_confirms_lost);
    RUN_TEST(test_engine_status_recovers_after_valid_receptions);
    RUN_TEST(test_fault_status_roundtrip);
    RUN_TEST(test_fault_status_timeout_after_missed_receptions);

    return UNITY_END();
}
