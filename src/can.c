#include <stdio.h>
#include "can.h"
#include "timer.h"
#include "validate.h"
#include "debounce.h"
#include "logger.h"

/* busの全フレームをvalid=falseの初期状態にする */
void can_bus_init(CanBus *bus) {
    for (int i = 0; i < (int)CAN_MSG_COUNT; i++) {
        bus->frames[i].id           = 0U;
        bus->frames[i].dlc          = 0U;
        bus->frames[i].timestamp_ms = 0U;
        bus->frames[i].valid        = false;
        for (int b = 0; b < 8; b++) {
            bus->frames[i].data[b] = 0U;
        }
    }
}

/* monの状態を初期値（CAN_LINK_OK、カウンタ0、キャッシュ0相当）にリセットする */
void can_monitor_init(CanMonitor *mon) {
    for (int i = 0; i < (int)CAN_MSG_COUNT; i++) {
        mon->last_seen_timestamp_ms[i] = 0U;
        mon->bad_count[i]              = 0U;
        mon->good_count[i]             = 0U;
        mon->state[i]                  = CAN_LINK_OK;
    }
    mon->last_engine_status.speed       = 0U;
    mon->last_engine_status.rpm         = 0U;
    mon->last_engine_status.temperature = 0U;
    mon->last_fault_status.fault_speed  = false;
    mon->last_fault_status.fault_rpm    = false;
    mon->last_fault_status.fault_temp   = false;
}

/* エンジンECU役：dataの生値をゲージデータとしてbusへ書く */
void can_send_engine_status(CanBus *bus, const VehicleSensorData *data) {
    CanFrame *frame = &bus->frames[CAN_MSG_ENGINE_STATUS];

    frame->id      = CAN_ID_ENGINE_STATUS;
    frame->dlc     = 4U;
    frame->data[0] = data->speed;
    frame->data[1] = (uint8_t)(data->rpm >> 8);
    frame->data[2] = (uint8_t)(data->rpm & 0xFFU);
    frame->data[3] = data->temperature;
    frame->timestamp_ms = timer_get_elapsed_ms();
    frame->valid        = true;
}

/* エンジンECU役：fault_mgrの確定状態(FAULT_DEGRADED)をビット詰めし警告灯データとしてbusへ書く */
void can_send_fault_status(CanBus *bus, const FaultManager *fault_mgr) {
    uint8_t flags = 0U;
    if (fault_mgr->state[SENSOR_SPEED] == FAULT_DEGRADED) { flags |= CAN_FAULT_BIT_SPEED; }
    if (fault_mgr->state[SENSOR_RPM]   == FAULT_DEGRADED) { flags |= CAN_FAULT_BIT_RPM; }
    if (fault_mgr->state[SENSOR_TEMP]  == FAULT_DEGRADED) { flags |= CAN_FAULT_BIT_TEMP; }

    CanFrame *frame = &bus->frames[CAN_MSG_FAULT_STATUS];
    frame->id           = CAN_ID_FAULT_STATUS;
    frame->dlc          = 1U;
    frame->data[0]      = flags;
    frame->timestamp_ms = timer_get_elapsed_ms();
    frame->valid        = true;
}

/* メーターECU役：busから最新のゲージデータを読み、Timeout/Invalid Dataを検知してmonを更新する */
CanLinkState can_receive_engine_status(CanMonitor *mon, const CanBus *bus, CanEngineStatus *out) {
    const CanFrame *frame = &bus->frames[CAN_MSG_ENGINE_STATUS];
    bool is_new = frame->valid && (frame->timestamp_ms != mon->last_seen_timestamp_ms[CAN_MSG_ENGINE_STATUS]);
    bool is_valid_data = false;

    if (is_new) {
        mon->last_seen_timestamp_ms[CAN_MSG_ENGINE_STATUS] = frame->timestamp_ms;

        uint8_t  speed = frame->data[0];
        uint16_t rpm   = (uint16_t)(((uint16_t)frame->data[1] << 8) | frame->data[2]);
        uint8_t  temp  = frame->data[3];

        is_valid_data = validate_in_range(SENSOR_SPEED, (int)speed)
                      && validate_in_range(SENSOR_RPM, (int)rpm)
                      && validate_in_range(SENSOR_TEMP, (int)temp);

        if (is_valid_data) {
            mon->last_engine_status.speed       = speed;
            mon->last_engine_status.rpm         = rpm;
            mon->last_engine_status.temperature = temp;
        }
    }

    bool is_bad  = (!is_new) || (!is_valid_data);
    bool is_good = is_new && is_valid_data;

    bool was_lost = (mon->state[CAN_MSG_ENGINE_STATUS] == CAN_LINK_LOST);
    bool now_lost = debounce_update(was_lost, is_bad, is_good,
                                     &mon->bad_count[CAN_MSG_ENGINE_STATUS],
                                     &mon->good_count[CAN_MSG_ENGINE_STATUS],
                                     CAN_DEBOUNCE_COUNT, CAN_RECOVERY_COUNT);

    if (now_lost != was_lost) {
        mon->state[CAN_MSG_ENGINE_STATUS] = now_lost ? CAN_LINK_LOST : CAN_LINK_OK;
        log_print_leveled(now_lost ? LOG_ERROR : LOG_INFO, "CAN",
                           now_lost ? "EngineStatus link lost" : "EngineStatus link recovered");
    }

    *out = mon->last_engine_status;
    return mon->state[CAN_MSG_ENGINE_STATUS];
}

/* メーターECU役：busから最新の警告灯データを読み、Timeoutを検知してmonを更新する。
   ビットフラグ自体は値域チェックの対象にならないため、Invalid Dataの判定はゲージデータのみ行う */
CanLinkState can_receive_fault_status(CanMonitor *mon, const CanBus *bus, CanFaultStatus *out) {
    const CanFrame *frame = &bus->frames[CAN_MSG_FAULT_STATUS];
    bool is_new = frame->valid && (frame->timestamp_ms != mon->last_seen_timestamp_ms[CAN_MSG_FAULT_STATUS]);

    if (is_new) {
        mon->last_seen_timestamp_ms[CAN_MSG_FAULT_STATUS] = frame->timestamp_ms;
        mon->last_fault_status.fault_speed = (frame->data[0] & CAN_FAULT_BIT_SPEED) != 0U;
        mon->last_fault_status.fault_rpm   = (frame->data[0] & CAN_FAULT_BIT_RPM)   != 0U;
        mon->last_fault_status.fault_temp  = (frame->data[0] & CAN_FAULT_BIT_TEMP)  != 0U;
    }

    bool is_bad  = !is_new;
    bool is_good = is_new;

    bool was_lost = (mon->state[CAN_MSG_FAULT_STATUS] == CAN_LINK_LOST);
    bool now_lost = debounce_update(was_lost, is_bad, is_good,
                                     &mon->bad_count[CAN_MSG_FAULT_STATUS],
                                     &mon->good_count[CAN_MSG_FAULT_STATUS],
                                     CAN_DEBOUNCE_COUNT, CAN_RECOVERY_COUNT);

    if (now_lost != was_lost) {
        mon->state[CAN_MSG_FAULT_STATUS] = now_lost ? CAN_LINK_LOST : CAN_LINK_OK;
        log_print_leveled(now_lost ? LOG_ERROR : LOG_INFO, "CAN",
                           now_lost ? "FaultStatus link lost" : "FaultStatus link recovered");
    }

    *out = mon->last_fault_status;
    return mon->state[CAN_MSG_FAULT_STATUS];
}

/* メーターECU役の表示：受信したゲージデータとリンク状態を1行でコンソールに出力する */
void can_print_engine_status(const CanEngineStatus *status, CanLinkState state) {
    char line[80];   /* "EngineStatus[LOST] speed=120 rpm=6000 temp=100" が収まるサイズ */
    const char *state_str = (state == CAN_LINK_OK) ? "OK" : "LOST";

    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)snprintf(line, sizeof(line), "EngineStatus[%s] speed=%d rpm=%d temp=%d",
                   state_str, (int)status->speed, (int)status->rpm, (int)status->temperature);
    log_print_leveled(LOG_INFO, "METER", line);
}

/* メーターECU役の表示：受信した警告灯データとリンク状態を1行でコンソールに出力する */
void can_print_fault_status(const CanFaultStatus *status, CanLinkState state) {
    char line[64];   /* "FaultStatus[LOST] speed=0 rpm=1 temp=0" が収まるサイズ */
    const char *state_str = (state == CAN_LINK_OK) ? "OK" : "LOST";

    (void)snprintf(line, sizeof(line), "FaultStatus[%s] speed=%d rpm=%d temp=%d",
                   state_str, (int)status->fault_speed, (int)status->fault_rpm, (int)status->fault_temp);
    log_print_leveled(LOG_INFO, "METER", line);
}
