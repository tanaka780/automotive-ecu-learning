#include <stdio.h>
#include "meter.h"
#include "logger.h"

/* メーターECU役（Phase26でmain.cから移動）。Schedulerへの登録とvoid *contextからの変換はmain.cが行い、
   このファイルはSchedulerを知らない */

/* 受信した値の表示はメーターECU役の仕事なので、can.cから移した。文面・タグは移す前と同じ */

/* 受信したゲージデータとリンク状態を1行でコンソールに出力する。
   temp_unknownがtrue（水温センサの確定異常の通知を受けている間）は、水温を"--"と表示する（Phase27）。
   ゲージデータの水温はフェイルセーフ値（常温相当）に差し替わっており、本当の水温は分からない。
   水温センサの故障の後にオーバーヒートしても見分けられないため、常温の値を見せて安心させない */
static void meter_print_engine_status(const CanEngineStatus *status, CanLinkState state, bool temp_unknown) {
    char line[80];   /* "EngineStatus[LOST] speed=120 rpm=6000 temp=130" が収まるサイズ */
    const char *state_str = (state == CAN_LINK_OK) ? "OK" : "LOST";

    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
    if (temp_unknown) {
        (void)snprintf(line, sizeof(line), "EngineStatus[%s] speed=%d rpm=%d temp=--",
                       state_str, (int)status->speed, (int)status->rpm);
    } else {
        (void)snprintf(line, sizeof(line), "EngineStatus[%s] speed=%d rpm=%d temp=%d",
                       state_str, (int)status->speed, (int)status->rpm, (int)status->temperature);
    }
    log_print_leveled(LOG_INFO, "METER", line);
}

/* 受信した警告灯データとリンク状態を1行でコンソールに出力する */
static void meter_print_fault_status(const CanFaultStatus *status, CanLinkState state) {
    char line[64];   /* "FaultStatus[LOST] speed=0 rpm=1 temp=0 overheat=0" が収まるサイズ */
    const char *state_str = (state == CAN_LINK_OK) ? "OK" : "LOST";

    (void)snprintf(line, sizeof(line), "FaultStatus[%s] speed=%d rpm=%d temp=%d overheat=%d",
                   state_str, (int)status->fault_speed, (int)status->fault_rpm, (int)status->fault_temp,
                   (int)status->overheat);
    log_print_leveled(LOG_INFO, "METER", line);
}

/* 受信したファン状態とリンク状態を1行でコンソールに出力する */
static void meter_print_fan_status(const CanFanStatus *status, CanLinkState state) {
    char line[32];   /* "FanStatus[LOST] fan=1" が収まるサイズ */
    const char *state_str = (state == CAN_LINK_OK) ? "OK" : "LOST";

    (void)snprintf(line, sizeof(line), "FanStatus[%s] fan=%d", state_str, (int)status->fan_on);
    log_print_leveled(LOG_INFO, "METER", line);
}

/* メーターECU役：警告灯データを受信し、Timeoutを検知して表示する（Phase20）。
   Lost/Recoveryの遷移をCanDtcRecordへ記録する（Phase21） */
void meter_receive_fault_status(MeterContext *ctx) {
    if (ctx->ignition->current == IGNITION_ON) {
        CanLinkState previous = ctx->can_monitor->state[CAN_MSG_FAULT_STATUS];
        CanFaultStatus status;
        CanLinkState state = can_receive_fault_status(ctx->can_monitor, ctx->can_bus, &status);
        /* Lost/Recoveryのログは、can.cから移した（Phase26）。文面・タグは移す前と同じ */
        if (state != previous) {
            log_print_leveled((state == CAN_LINK_LOST) ? LOG_ERROR : LOG_INFO, "CAN",
                              (state == CAN_LINK_LOST) ? "FaultStatus link lost" : "FaultStatus link recovered");
        }
        can_diag_check(ctx->can_dtc, CAN_MSG_FAULT_STATUS, previous, state);
        meter_print_fault_status(&status, state);
    }
}

/* メーターECU役：ゲージデータを受信し、Timeout/Invalid Dataを検知して表示する（Phase20）。
   Lost/Recoveryの遷移をCanDtcRecordへ記録する（Phase21） */
void meter_receive_engine_status(MeterContext *ctx) {
    if (ctx->ignition->current == IGNITION_ON) {
        CanLinkState previous = ctx->can_monitor->state[CAN_MSG_ENGINE_STATUS];
        CanEngineStatus status;
        CanLinkState state = can_receive_engine_status(ctx->can_monitor, ctx->can_bus, &status);
        /* Lost/Recoveryのログは、can.cから移した（Phase26）。文面・タグは移す前と同じ */
        if (state != previous) {
            log_print_leveled((state == CAN_LINK_LOST) ? LOG_ERROR : LOG_INFO, "CAN",
                              (state == CAN_LINK_LOST) ? "EngineStatus link lost" : "EngineStatus link recovered");
        }
        can_diag_check(ctx->can_dtc, CAN_MSG_ENGINE_STATUS, previous, state);
        /* 水温が分からないかは、最後に受け取った警告灯データの水温のビットで決める（Phase27）。
           警告灯データの受信タスクが先に動くため、確定・復帰の秒には、その秒のビットで表示が切り替わる */
        meter_print_engine_status(&status, state, ctx->can_monitor->last_fault_status.fault_temp);
    }
}

/* メーターECU役：ファン制御ECUからのファン状態を受信し、Timeoutを検知して表示する（Phase26）。
   途絶はファン制御ECUの異常として、CanDtcRecordのFanStatusへ記録する */
void meter_receive_fan_status(MeterContext *ctx) {
    if (ctx->ignition->current == IGNITION_ON) {
        CanLinkState previous = ctx->can_monitor->state[CAN_MSG_FAN_STATUS];
        CanFanStatus status;
        CanLinkState state = can_receive_fan_status(ctx->can_monitor, ctx->can_bus, &status);
        if (state != previous) {
            log_print_leveled((state == CAN_LINK_LOST) ? LOG_ERROR : LOG_INFO, "CAN",
                              (state == CAN_LINK_LOST) ? "FanStatus link lost" : "FanStatus link recovered");
        }
        can_diag_check(ctx->can_dtc, CAN_MSG_FAN_STATUS, previous, state);
        meter_print_fan_status(&status, state);
    }
}
