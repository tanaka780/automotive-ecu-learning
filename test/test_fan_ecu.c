#include "unity.h"
#include "fan_ecu.h"

/* busにフレームを直接書き、ファン制御ECU役の受信関数を呼んで確かめる。送信関数（can_send_*）は
   実時刻を送信時刻に使うため、続けて送ると同じ時刻になり新しいフレームと判定されない。
   ここでは送信時刻を自分で1ずつ進めて書くので、実際の待機は要らない */

static Ignition      ignition;
static CanBus        bus;
static CanMonitor    monitor;
static CanDtcRecord  dtc;
static FanEcuState   state;
static FanEcuContext ctx;
static uint32_t      now_ms;   /* 次に書くフレームの送信時刻。0は受信側の初期値と同じなので1から始める */

void setUp(void) {
    ignition.current  = IGNITION_ON;
    ignition.previous = IGNITION_ON;
    can_bus_init(&bus);
    can_monitor_init(&monitor);
    can_diag_init(&dtc);
    fan_ecu_init(&state);
    ctx.ignition    = &ignition;
    ctx.can_bus     = &bus;
    ctx.can_monitor = &monitor;
    ctx.can_dtc     = &dtc;
    ctx.state       = &state;
    now_ms = 1U;
}

void tearDown(void) {}

/* 水温tempのゲージデータ（車速・回転数は値域内の固定値）を新しいフレームとして書き、受信させる */
static void receive_engine(uint8_t temp) {
    CanFrame *f = &bus.frames[CAN_MSG_ENGINE_STATUS];
    f->id      = CAN_ID_ENGINE_STATUS;
    f->dlc     = 4U;
    f->data[0] = 50U;
    f->data[1] = (uint8_t)(2000U >> 8);
    f->data[2] = (uint8_t)(2000U & 0xFFU);
    f->data[3] = temp;
    f->timestamp_ms = now_ms++;
    f->valid   = true;
    fan_ecu_receive_engine_status(&ctx);
}

/* 警告灯データ（bitsはCAN_FAULT_BIT_*の組み合わせ）を新しいフレームとして書き、受信させる。
   ファン出力はこのときに決まる */
static void receive_fault(uint8_t bits) {
    CanFrame *f = &bus.frames[CAN_MSG_FAULT_STATUS];
    f->id      = CAN_ID_FAULT_STATUS;
    f->dlc     = 1U;
    f->data[0] = bits;
    f->timestamp_ms = now_ms++;
    f->valid   = true;
    fan_ecu_receive_fault_status(&ctx);
}

/* 水温を一度も受け取っていない間は、ON閾値を超える値が来る前でもOFFのまま（判断しない） */
static void test_off_until_first_temp(void) {
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "水温を受け取る前はOFF");
    TEST_ASSERT_FALSE_MESSAGE(state.has_valid_temp, "まだ水温を受け取っていない");
}

/* 水温による判断：ON閾値でON、閾値の間はONを保ち、OFF閾値でOFF */
static void test_temp_hysteresis(void) {
    receive_engine(FAN_ON_TEMP);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "ON閾値でON");
    receive_engine(FAN_OFF_TEMP + 1U);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "閾値の間ではONのまま");
    receive_engine(FAN_OFF_TEMP);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "OFF閾値でOFF");
}

/* 水温センサの確定異常の間は、差し替えられた水温（25℃）が来てもONにする */
static void test_temp_fault_forces_on(void) {
    receive_engine(25U);
    receive_fault(CAN_FAULT_BIT_TEMP);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "水温センサの確定異常でON");
}

/* 確定異常の間は水温による要求を更新しない。ON中に異常→25℃→解除で閾値の間の水温なら、
   異常の前の要求（ON）が残る。異常の間に25℃で更新していればOFFになる */
static void test_temp_request_kept_during_fault(void) {
    receive_engine(FAN_ON_TEMP);
    receive_fault(0U);
    receive_engine(25U);
    receive_fault(CAN_FAULT_BIT_TEMP);
    receive_engine(FAN_OFF_TEMP + 1U);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.temp_request, "異常の前の要求(ON)が残る");
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "解除後も閾値の間ではONのまま");
}

/* 確定異常が解除されたら、受け取った水温による通常の判断に戻る */
static void test_returns_to_temp_after_fault(void) {
    receive_engine(25U);
    receive_fault(CAN_FAULT_BIT_TEMP);
    receive_engine(FAN_OFF_TEMP);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "解除後はOFF閾値の水温でOFF");
}

/* 確定異常の前の要求がOFFで、解除の通知が解除後の水温より先に届いても、使える水温が決まるまではONを保つ。
   保たなければ、前の要求(OFF)で一瞬止まる（最初から断線していて、高温で直る場合、Phase27） */
static void test_hold_on_after_temp_fault_until_usable_temp(void) {
    receive_engine(25U);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "前提: 異常の前の要求はOFF");
    receive_engine(25U);
    receive_fault(CAN_FAULT_BIT_TEMP);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "確定異常でON");
    receive_fault(0U);   /* 解除の通知だけが先に届く。手元の水温(25)は確定中に送られたもので使えない */
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "使える水温が決まるまではONを保つ");
    receive_engine(100U);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "解除後の100℃でONのまま");
    TEST_ASSERT_FALSE_MESSAGE(state.hold_on, "使える水温が決まったら、保つのをやめる");
}

/* ONを保つのは使える水温が決まるまでで、決まった水温がOFF閾値以下ならOFFにする（Phase27） */
static void test_hold_on_ends_with_low_usable_temp(void) {
    receive_engine(25U);
    receive_fault(CAN_FAULT_BIT_TEMP);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "前提: 解除直後はONを保つ");
    receive_engine(FAN_OFF_TEMP);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "使える水温がOFF閾値ならOFF");
}

/* 警告灯データの途絶が確定するとONにし、復帰すると水温による判断に戻る */
static void test_fault_status_lost_forces_on(void) {
    receive_engine(FAN_OFF_TEMP);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "途絶の前はOFF");
    fan_ecu_receive_fault_status(&ctx);   /* 新しいフレームを書かずに受信させる（未受信1回目） */
    fan_ecu_receive_fault_status(&ctx);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "未受信2回ではまだOFF");
    fan_ecu_receive_fault_status(&ctx);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "未受信3回で途絶が確定しON");
    receive_fault(0U);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "正常受信2回ではまだON");
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "正常受信3回で復帰し、水温による判断(OFF)に戻る");
}

/* ゲージデータの途絶が確定すると、警告灯データを受けたときにONにする */
static void test_engine_status_lost_forces_on(void) {
    receive_engine(FAN_OFF_TEMP);
    receive_fault(0U);
    fan_ecu_receive_engine_status(&ctx);
    fan_ecu_receive_engine_status(&ctx);
    fan_ecu_receive_engine_status(&ctx);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "ゲージデータの途絶でON");
}

/* イグニッションOFF→ONで判断の状態を戻し、OFF前の水温では判断しない。busに残ったOFF前の
   ゲージデータは新しいフレームとして扱われない。新しく届いたら判断を再開する */
static void test_ignition_cycle_discards_old_temp(void) {
    receive_engine(FAN_ON_TEMP);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "OFF前はON");

    ignition.current = IGNITION_OFF;
    fan_ecu_receive_fault_status(&ctx);
    fan_ecu_receive_engine_status(&ctx);
    ignition.current = IGNITION_ON;

    fan_ecu_receive_engine_status(&ctx);   /* busにはOFF前のゲージデータが残っている */
    receive_fault(0U);
    TEST_ASSERT_FALSE_MESSAGE(state.has_valid_temp, "OFF前の水温は使わない");
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "ON直後は水温が届くまでOFF");

    receive_engine(FAN_ON_TEMP);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "新しく届いた水温で判断を再開する");
}

/* イグニッションOFF→ONで、受信監視状態の途絶の連続回数も戻す。切る前に受け取れなかった回数を足さず、
   かけ直した後に3回続けて受け取れなかったときに途絶とし、記録する（Phase28の完了後） */
static void test_ignition_cycle_restarts_link_count(void) {
    receive_engine(FAN_ON_TEMP);
    fan_ecu_receive_engine_status(&ctx);   /* 新しいゲージデータが届かない（1回目） */
    fan_ecu_receive_engine_status(&ctx);   /* 2回目。あと1回で途絶 */
    TEST_ASSERT_EQUAL_MESSAGE(CAN_LINK_OK, monitor.state[CAN_MSG_ENGINE_STATUS], "前提: 2回ではまだ途絶でない");

    ignition.current = IGNITION_OFF;
    fan_ecu_receive_fault_status(&ctx);
    ignition.current = IGNITION_ON;

    fan_ecu_receive_engine_status(&ctx);
    fan_ecu_receive_engine_status(&ctx);
    TEST_ASSERT_EQUAL_MESSAGE(CAN_LINK_OK, monitor.state[CAN_MSG_ENGINE_STATUS], "かけ直し後の2回目まではOK（切る前の2回を足さない）");
    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.count[CAN_MSG_ENGINE_STATUS], "まだ記録しない");

    fan_ecu_receive_engine_status(&ctx);
    TEST_ASSERT_EQUAL_MESSAGE(CAN_LINK_LOST, monitor.state[CAN_MSG_ENGINE_STATUS], "かけ直し後の3回目で途絶");
    TEST_ASSERT_EQUAL_MESSAGE(1, dtc.count[CAN_MSG_ENGINE_STATUS], "途絶を1回記録する");
}

/* イグニッションOFF中は受信も判断もしない（未受信を数えず、途絶も記録しない） */
static void test_nothing_while_ignition_off(void) {
    ignition.current = IGNITION_OFF;
    for (int i = 0; i < 5; i++) {
        fan_ecu_receive_fault_status(&ctx);
        fan_ecu_receive_engine_status(&ctx);
    }
    TEST_ASSERT_EQUAL_MESSAGE(CAN_LINK_OK, monitor.state[CAN_MSG_FAULT_STATUS], "OFF中は途絶を数えない");
    TEST_ASSERT_EQUAL_MESSAGE(0, dtc.count[CAN_MSG_FAULT_STATUS], "OFF中は記録しない");
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "OFF中はOFF");
}

/* 送る値はまとめた後のファン出力で、can_send_fan_statusでbusに書くとbit0に入る */
static void test_send_value_follows_output(void) {
    bool fan_on = true;
    TEST_ASSERT_TRUE_MESSAGE(fan_ecu_get_fan_status(&ctx, &fan_on), "ON中は送る");
    TEST_ASSERT_FALSE_MESSAGE(fan_on, "判断の前はOFFを送る");

    receive_engine(FAN_ON_TEMP);
    receive_fault(0U);
    TEST_ASSERT_TRUE(fan_ecu_get_fan_status(&ctx, &fan_on));
    TEST_ASSERT_TRUE_MESSAGE(fan_on, "ファン出力ONならONを送る");
    can_send_fan_status(&bus, fan_on);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(CAN_FAN_BIT_ON, bus.frames[CAN_MSG_FAN_STATUS].data[0], "bit0にONが入る");

    receive_engine(FAN_OFF_TEMP);
    receive_fault(0U);
    TEST_ASSERT_TRUE(fan_ecu_get_fan_status(&ctx, &fan_on));
    TEST_ASSERT_FALSE_MESSAGE(fan_on, "ファン出力OFFならOFFを送る");
    can_send_fan_status(&bus, fan_on);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00U, bus.frames[CAN_MSG_FAN_STATUS].data[0], "OFFならbit0は立たない");
}

/* 強制ONの間も、送るのはまとめた後の出力（ON） */
static void test_send_value_during_forced_on(void) {
    bool fan_on = false;
    receive_engine(25U);
    receive_fault(CAN_FAULT_BIT_TEMP);
    TEST_ASSERT_TRUE(fan_ecu_get_fan_status(&ctx, &fan_on));
    TEST_ASSERT_TRUE_MESSAGE(fan_on, "確定異常による強制ONでもONを送る");
}

/* イグニッションOFF中は送らない。OFF→ONの直後に受信より先に呼ばれても、OFF前の出力(ON)ではなくOFFを送る */
static void test_send_after_ignition_cycle(void) {
    bool fan_on = false;
    receive_engine(FAN_ON_TEMP);
    receive_fault(0U);
    TEST_ASSERT_EQUAL(FAN_ON, state.output);

    ignition.current = IGNITION_OFF;
    TEST_ASSERT_FALSE_MESSAGE(fan_ecu_get_fan_status(&ctx, &fan_on), "OFF中は送らない");

    ignition.current = IGNITION_ON;
    fan_on = true;
    TEST_ASSERT_TRUE(fan_ecu_get_fan_status(&ctx, &fan_on));
    TEST_ASSERT_FALSE_MESSAGE(fan_on, "ON直後は受信より先でもOFFを送る");
}

/* 警告灯データ（bitsはCAN_FAULT_BIT_*の組み合わせ）を新しいフレームとして書くだけで、受信させない。
   送られたがファン制御ECUがまだ受信していない状態を作るために使う */
static void write_fault(uint8_t bits) {
    CanFrame *f = &bus.frames[CAN_MSG_FAULT_STATUS];
    f->id      = CAN_ID_FAULT_STATUS;
    f->dlc     = 1U;
    f->data[0] = bits;
    f->timestamp_ms = now_ms++;
    f->valid   = true;
}

/* 水温センサの確定の瞬間：確定の前に送られた警告灯データ（正常）を、確定の後に送られた水温（25℃）より
   後に受信しても、25℃では判断しない（Day58。タスクの開始がずれると、実際にこの順番になる） */
static void test_no_off_at_fault_confirmation(void) {
    receive_engine(95U);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "前提: 95℃でON");
    write_fault(0U);                      /* 確定の前に送られた警告灯データ */
    receive_engine(25U);                  /* 確定の後に送られた、差し替え後の水温 */
    fan_ecu_receive_fault_status(&ctx);   /* 確定の前の警告灯データで判断する */
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "確定前の通知と差し替え後の水温の組み合わせでOFFにしない");
    receive_fault(CAN_FAULT_BIT_TEMP);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "確定の通知を受けてもONのまま");
}

/* 水温センサの復帰の瞬間：実際の並びでは、復帰後の警告灯データ（正常）を、復帰後の水温より先に受信する。
   確定中に受け取った25℃では判断せず、故障前の要求（ON）を保つ（Day58。タスクの開始がずれていなくても、
   この順番になる。test_temp_request_kept_during_faultは水温→警告灯データの順で、この並びを通らない） */
static void test_no_off_at_fault_recovery(void) {
    receive_engine(95U);
    receive_fault(0U);
    receive_engine(25U);
    receive_fault(CAN_FAULT_BIT_TEMP);
    receive_fault(0U);                    /* 復帰後の警告灯データ。手元の水温はまだ確定中の25℃ */
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "復帰直後に確定中の25℃でOFFにしない");
    receive_engine(FAN_OFF_TEMP + 1U);
    receive_fault(0U);
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "復帰後の閾値の間の水温ではONのまま");
}

/* 送信時刻が一周（2^32ms）をまたいでも、水温より後に送られた警告灯データを正しく見分ける */
static void test_judge_across_timestamp_wrap(void) {
    now_ms = 0xFFFFFFFDU;
    receive_engine(FAN_ON_TEMP);          /* 送信時刻 0xFFFFFFFD */
    receive_fault(0U);                    /* 0xFFFFFFFE */
    TEST_ASSERT_EQUAL_MESSAGE(FAN_ON, state.output, "前提: ON閾値でON");
    receive_engine(FAN_OFF_TEMP);         /* 0xFFFFFFFF */
    receive_fault(0U);                    /* 一周して0 */
    TEST_ASSERT_EQUAL_MESSAGE(FAN_OFF, state.output, "一周した後の警告灯データでも、OFF閾値の水温を使えると決める");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_off_until_first_temp);
    RUN_TEST(test_temp_hysteresis);
    RUN_TEST(test_temp_fault_forces_on);
    RUN_TEST(test_temp_request_kept_during_fault);
    RUN_TEST(test_returns_to_temp_after_fault);
    RUN_TEST(test_hold_on_after_temp_fault_until_usable_temp);
    RUN_TEST(test_hold_on_ends_with_low_usable_temp);
    RUN_TEST(test_fault_status_lost_forces_on);
    RUN_TEST(test_engine_status_lost_forces_on);
    RUN_TEST(test_ignition_cycle_discards_old_temp);
    RUN_TEST(test_ignition_cycle_restarts_link_count);
    RUN_TEST(test_nothing_while_ignition_off);
    RUN_TEST(test_send_value_follows_output);
    RUN_TEST(test_send_value_during_forced_on);
    RUN_TEST(test_send_after_ignition_cycle);
    RUN_TEST(test_no_off_at_fault_confirmation);
    RUN_TEST(test_no_off_at_fault_recovery);
    RUN_TEST(test_judge_across_timestamp_wrap);

    return UNITY_END();
}
