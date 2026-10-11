#include "meter.h"

/* メーターECU役（Phase26でmain.cから移動）。Schedulerへの登録とvoid *contextからの変換はmain.cが行い、
   このファイルはSchedulerを知らない */

/* メーターECU役：警告灯データを受信し、Timeoutを検知して表示する（Phase20）。
   Lost/Recoveryの遷移をCanDtcRecordへ記録する（Phase21） */
void meter_receive_fault_status(MeterContext *ctx) {
    if (ctx->ignition->current == IGNITION_ON) {
        CanLinkState previous = ctx->can_monitor->state[CAN_MSG_FAULT_STATUS];
        CanFaultStatus status;
        CanLinkState state = can_receive_fault_status(ctx->can_monitor, ctx->can_bus, &status);
        can_diag_check(ctx->can_dtc, CAN_MSG_FAULT_STATUS, previous, state);
        can_print_fault_status(&status, state);
    }
}

/* メーターECU役：ゲージデータを受信し、Timeout/Invalid Dataを検知して表示する（Phase20）。
   Lost/Recoveryの遷移をCanDtcRecordへ記録する（Phase21） */
void meter_receive_engine_status(MeterContext *ctx) {
    if (ctx->ignition->current == IGNITION_ON) {
        CanLinkState previous = ctx->can_monitor->state[CAN_MSG_ENGINE_STATUS];
        CanEngineStatus status;
        CanLinkState state = can_receive_engine_status(ctx->can_monitor, ctx->can_bus, &status);
        can_diag_check(ctx->can_dtc, CAN_MSG_ENGINE_STATUS, previous, state);
        can_print_engine_status(&status, state);
    }
}
