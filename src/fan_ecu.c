#include <stdio.h>
#include "fan_ecu.h"
#include "logger.h"

/* ファン制御ECU役（Phase26）。受信・途絶の記録と、ファン出力の判断、送る値の用意を行う。
   Schedulerへの登録とvoid *contextからの変換、busへの送信はmain.cが行い、このファイルはSchedulerを知らない */

/* 受信した値の表示はメーターECUの役割のため、ここでは出さない。
   途絶・復帰のログは、メーターECUの[CAN]と区別できるよう[FAN]タグで出す */

void fan_ecu_init(FanEcuState *state) {
    state->powered        = false;
    state->has_valid_temp = false;
    state->temp           = 0U;
    state->temp_request   = FAN_OFF;
    state->output         = FAN_OFF;
}

/* イグニッションON中ならtrueを返す。OFF→ONに変わった最初の呼び出しで、判断の状態を初期値に戻す。
   受信監視状態とDTC相当の記録は戻さない。busはOFFの間も最後のフレームを持ち続けるので、
   送信時刻の記録を戻すと、OFF前の古いフレームを新しいものと見誤るため。
   OFFの間に一度は呼ばれることが前提（イグニッションはサンプル周期(1000ms)でしか変わらず、
   警告灯データの受信は200ms周期で呼ばれるので、今の構成では見逃さない） */
static bool fan_ecu_is_powered(FanEcuContext *ctx) {
    if (ctx->ignition->current != IGNITION_ON) {
        ctx->state->powered = false;
        return false;
    }
    if (!ctx->state->powered) {
        fan_ecu_init(ctx->state);
        ctx->state->powered = true;
    }
    return true;
}

/* 理由ごとの要求をまとめてファン出力を決め、変わった瞬間だけログを出す。
   受信途絶・水温センサの確定異常の間は、受け取った水温が使えない（古い、またはフェイルセーフ値に
   差し替わっている）ため強制ONにし、水温による要求は更新せずに保つ。強制ONの影響を、解除後の
   水温による判断に残さないため */
static void fan_ecu_decide(FanEcuContext *ctx, CanLinkState fault_link, const CanFaultStatus *fault) {
    FanEcuState *st = ctx->state;
    const char *reason = NULL;   /* ONにする理由。NULLならOFF */

    if ((fault_link == CAN_LINK_LOST) || (ctx->can_monitor->state[CAN_MSG_ENGINE_STATUS] == CAN_LINK_LOST)) {
        reason = "link lost";
    } else if (fault->fault_temp) {
        reason = "temp sensor fault";
    } else if (st->has_valid_temp) {
        st->temp_request = fan_ctrl_decide(st->temp, st->temp_request);
        if (st->temp_request == FAN_ON) {
            reason = "temp";
        }
    } else {
        /* 水温をまだ受け取っていない間は判断せず、OFFのままにする */
    }

    FanState next = (reason != NULL) ? FAN_ON : FAN_OFF;
    if (next != st->output) {
        char line[40];   /* "Fan ON (temp sensor fault)" が収まるサイズ */
        if (next == FAN_ON) {
            (void)snprintf(line, sizeof(line), "Fan ON (%s)", reason);
        } else {
            (void)snprintf(line, sizeof(line), "Fan OFF");
        }
        log_print_leveled(LOG_INFO, "FAN", line);
        st->output = next;
    }
}

void fan_ecu_receive_fault_status(FanEcuContext *ctx) {
    if (!fan_ecu_is_powered(ctx)) {
        return;
    }
    CanLinkState previous = ctx->can_monitor->state[CAN_MSG_FAULT_STATUS];
    CanFaultStatus status;
    CanLinkState state = can_receive_fault_status(ctx->can_monitor, ctx->can_bus, &status);
    if (state != previous) {
        log_print_leveled((state == CAN_LINK_LOST) ? LOG_ERROR : LOG_INFO, "FAN",
                          (state == CAN_LINK_LOST) ? "FaultStatus link lost" : "FaultStatus link recovered");
    }
    can_diag_check(ctx->can_dtc, CAN_MSG_FAULT_STATUS, previous, state);
    fan_ecu_decide(ctx, state, &status);
}

void fan_ecu_receive_engine_status(FanEcuContext *ctx) {
    if (!fan_ecu_is_powered(ctx)) {
        return;
    }
    CanLinkState previous = ctx->can_monitor->state[CAN_MSG_ENGINE_STATUS];
    uint32_t seen_before = ctx->can_monitor->last_seen_timestamp_ms[CAN_MSG_ENGINE_STATUS];
    CanEngineStatus status;
    CanLinkState state = can_receive_engine_status(ctx->can_monitor, ctx->can_bus, &status);
    if (state != previous) {
        log_print_leveled((state == CAN_LINK_LOST) ? LOG_ERROR : LOG_INFO, "FAN",
                          (state == CAN_LINK_LOST) ? "EngineStatus link lost" : "EngineStatus link recovered");
    }
    can_diag_check(ctx->can_dtc, CAN_MSG_ENGINE_STATUS, previous, state);

    /* 受信関数はTimeout中も直近のキャッシュを返すので、新しく届いたかは送信時刻の記録が変わったかで見る */
    bool is_new = (ctx->can_monitor->last_seen_timestamp_ms[CAN_MSG_ENGINE_STATUS] != seen_before);
    if (is_new && (state == CAN_LINK_OK)) {
        ctx->state->temp           = status.temperature;
        ctx->state->has_valid_temp = true;
    }
}

/* 受信と同じくfan_ecu_is_powered()を通す。OFF→ONの直後に受信より先に呼ばれても、
   OFF前の出力ではなく、戻した後の出力（OFF）を送るため */
bool fan_ecu_get_fan_status(FanEcuContext *ctx, bool *fan_on) {
    if (!fan_ecu_is_powered(ctx)) {
        return false;
    }
    *fan_on = (ctx->state->output == FAN_ON);
    return true;
}
