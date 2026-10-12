#include "meter.h"
#include "logger.h"

/* メーターECU役（Phase26でmain.cから移動）。Schedulerへの登録とvoid *contextからの変換はmain.cが行い、
   このファイルはSchedulerを知らない */

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
        /* Lost/Recoveryのログは、can.cから移した（Phase26）。文面・タグは移す前と同じ */
        if (state != previous) {
            log_print_leveled((state == CAN_LINK_LOST) ? LOG_ERROR : LOG_INFO, "CAN",
                              (state == CAN_LINK_LOST) ? "EngineStatus link lost" : "EngineStatus link recovered");
        }
        can_diag_check(ctx->can_dtc, CAN_MSG_ENGINE_STATUS, previous, state);
        can_print_engine_status(&status, state);
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
        can_print_fan_status(&status, state);
    }
}
