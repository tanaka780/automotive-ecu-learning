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
    state->temp_sent_ms   = 0U;
    state->temp_received_ms = 0U;
    state->temp_stale       = false;
    state->decided_ms       = 0U;
    state->decision_stale   = false;
    state->temp_judged    = false;
    state->temp_usable    = false;
    state->temp_request   = FAN_OFF;
    state->hold_on        = false;
    state->output         = FAN_OFF;
}

/* イグニッションON中ならtrueを返す。OFF→ONに変わった最初の呼び出しで、判断の状態を初期値に戻し、
   受信監視状態は途絶の連続回数と途絶の状態だけを戻す（can_monitor_restart）。DTC相当の記録は戻さない。
   送信時刻の記録も戻さない。busはOFFの間も最後のフレームを持ち続けるので、
   送信時刻の記録を戻すと、OFF前の古いフレームを新しいものと見誤るため。
   OFFの間に一度は呼ばれることが前提（イグニッションはサンプル周期(1000ms)でしか変わらず、
   警告灯データの受信は200ms周期で呼ばれるので、今の構成では見逃さない）。
   ONにした時刻を、まだ水温を受け取っていない間の「最後に受け取った時刻」として覚える（Phase32） */
static bool fan_ecu_is_powered(FanEcuContext *ctx, uint32_t now_ms) {
    if (ctx->ignition->current != IGNITION_ON) {
        ctx->state->powered = false;
        return false;
    }
    if (!ctx->state->powered) {
        fan_ecu_init(ctx->state);
        can_monitor_restart(ctx->can_monitor);
        ctx->state->powered = true;
        ctx->state->temp_received_ms = now_ms;
        ctx->state->decided_ms       = now_ms;   /* まだ判断していない間は、ONにした時刻から数える */
    }
    return true;
}

/* ファン出力をnextにし、変わった瞬間だけログを出す。reasonはONにする理由（OFFのときは使わない）。
   判断（fan_ecu_decide）と、判断が古いときの送信（fan_ecu_get_fan_status）の両方から呼ぶ（Phase32） */
static void fan_ecu_set_output(FanEcuState *st, FanState next, const char *reason) {
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

/* 送信時刻aがbより後ならtrueを返す。uint32_tの引き算は桁あふれをまたいでも経過時間になるため、
   差が半周（2^31ms、約24.8日）未満なら後とみなす。大小をそのまま比べると、約49.7日で時刻が一周したときに誤る */
static bool fan_ecu_sent_after(uint32_t a, uint32_t b) {
    uint32_t diff = a - b;
    return (diff != 0U) && (diff < 0x80000000U);
}

/* 水温と、水温センサの確定異常の通知は別のメッセージ・別の周期で届く。水温センサの確定・復帰の前後では、
   差し替え後の水温（25℃）を確定前の通知（正常）と組み合わせたり、確定中に受け取った25℃を復帰後の通知（正常）と
   組み合わせたりしうる。そこで、水温1つごとに、それより後に送られた最初の警告灯データで使えるかを1回だけ決める。
   エンジン監視ECUは確定異常の判定を水温の送信より先に行うので、水温より後に送られた警告灯データは、
   その水温が差し替え後かどうかを必ず反映している */
static void fan_ecu_judge_temp(FanEcuContext *ctx, const CanFaultStatus *fault) {
    FanEcuState *st = ctx->state;
    if (st->has_valid_temp && (!st->temp_judged)
        && fan_ecu_sent_after(ctx->can_monitor->last_seen_timestamp_ms[CAN_MSG_FAULT_STATUS], st->temp_sent_ms)) {
        st->temp_judged = true;
        st->temp_usable = !fault->fault_temp;
    }
}

/* 最後に新しい水温を受け取ってからFAN_TEMP_STALE_MSを超えたら、水温が古いと判断する（Phase32）。
   途絶の数え上げはゲージデータの受信のタスクの中にあるので、そのタスクが止まると途絶とは判断されない。
   そこで判断する側（警告灯データの受信）が、自分の時計で古さを確かめる。
   ゲージデータが途絶している間は見ない。途絶として扱われているうえ、見るとログが途絶させた実行にも出てしまう。
   値域外のフレームでも受け取った時刻は進むが、それは受信のタスクが動いている間だけなので、止まったことは隠さない。
   判断の状態が変わった瞬間だけログを出す。ファンが既にONでも、止まったことの跡を残すため（記録（DTC）は付けない） */
static void fan_ecu_check_temp_age(FanEcuContext *ctx, uint32_t now_ms) {
    FanEcuState *st = ctx->state;
    bool link_ok = (ctx->can_monitor->state[CAN_MSG_ENGINE_STATUS] == CAN_LINK_OK);
    bool stale   = link_ok && ((uint32_t)(now_ms - st->temp_received_ms) > FAN_TEMP_STALE_MS);
    if (stale != st->temp_stale) {
        log_print_leveled(stale ? LOG_ERROR : LOG_INFO, "FAN",
                          stale ? "No new temp for too long" : "New temp received again");
        st->temp_stale = stale;
    }
}

/* 理由ごとの要求をまとめてファン出力を決め、変わった瞬間だけログを出す。
   受信途絶・水温センサの確定異常の間は、受け取った水温が使えない（古い、またはフェイルセーフ値に
   差し替わっている）ため強制ONにし、水温による要求は更新せずに保つ。強制ONの影響を、解除後の
   水温による判断に残さないため。
   ただし、強制ONが解けた直後は、まだ使える水温が決まっていないことがある。水温センサの確定異常の解除は
   警告灯データ1回で伝わり、解除後の水温より先に届くため（通信の途絶は、正常受信が3回続くまで解けないので、
   その間に水温が決まる）。そこで、使える水温が決まるまではONを保ち、強制ONの前の要求（例：OFF）で
   一瞬止めない（Phase27） */
static void fan_ecu_decide(FanEcuContext *ctx, CanLinkState fault_link, const CanFaultStatus *fault) {
    FanEcuState *st = ctx->state;
    const char *reason = NULL;   /* ONにする理由。NULLならOFF */

    if ((fault_link == CAN_LINK_LOST) || (ctx->can_monitor->state[CAN_MSG_ENGINE_STATUS] == CAN_LINK_LOST)) {
        reason = "link lost";
        st->hold_on = true;
    } else if (fault->fault_temp) {
        reason = "temp sensor fault";
        st->hold_on = true;
    } else if (st->temp_stale) {
        reason = "temp stale";   /* ゲージデータの受信のタスクが止まっている（Phase32） */
        st->hold_on = true;
    } else {
        /* 使えると決まった水温でだけ要求を更新する。まだ決まっていない・使えない水温なら、前の要求を保つ */
        if (st->has_valid_temp && st->temp_judged && st->temp_usable) {
            st->temp_request = fan_ctrl_decide(st->temp, st->temp_request);
            st->hold_on = false;
        }
        if (st->hold_on) {
            reason = "hold";   /* 強制ONの続きなので出力はONのままで、ログには出ない */
        } else if (st->has_valid_temp && (st->temp_request == FAN_ON)) {
            reason = "temp";
        } else {
            /* 水温による要求がOFFの間と、水温をまだ受け取っていない間（判断しない）はOFF */
        }
    }

    fan_ecu_set_output(st, (reason != NULL) ? FAN_ON : FAN_OFF, reason);
}

void fan_ecu_receive_fault_status(FanEcuContext *ctx, uint32_t now_ms) {
    if (!fan_ecu_is_powered(ctx, now_ms)) {
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
    fan_ecu_judge_temp(ctx, &status);
    fan_ecu_check_temp_age(ctx, now_ms);
    /* 判断のタスクが動き出したら、判断の古さは解ける。出力はこの後の判断でいつもどおり決め直す（Phase32） */
    if (ctx->state->decision_stale) {
        log_print_leveled(LOG_INFO, "FAN", "Decision resumed");
        ctx->state->decision_stale = false;
    }
    fan_ecu_decide(ctx, state, &status);
    ctx->state->decided_ms = now_ms;
}

void fan_ecu_receive_engine_status(FanEcuContext *ctx, uint32_t now_ms) {
    if (!fan_ecu_is_powered(ctx, now_ms)) {
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
        ctx->state->temp_sent_ms   = ctx->can_monitor->last_seen_timestamp_ms[CAN_MSG_ENGINE_STATUS];
        ctx->state->temp_judged    = false;
        ctx->state->temp_received_ms = now_ms;   /* 古さは送信時刻ではなく、受け取った時刻で測る（Phase32） */
    }
}

/* 受信と同じくfan_ecu_is_powered()を通す。OFF→ONの直後に受信より先に呼ばれても、
   OFF前の出力ではなく、戻した後の出力（OFF）を送るため。
   最後に判断してからFAN_DECISION_STALE_MSを超えていたら、判断のタスク（警告灯データの受信）が止まっているので、
   送る値だけでなくファンの出力そのものをONにする（Phase32）。送る値だけをONにすると、本当のファンはOFFのまま
   メーターにONと報告することになるため。古くなった瞬間だけログを出す（記録（DTC）は付けない） */
bool fan_ecu_get_fan_status(FanEcuContext *ctx, uint32_t now_ms, bool *fan_on) {
    if (!fan_ecu_is_powered(ctx, now_ms)) {
        return false;
    }
    FanEcuState *st = ctx->state;
    if ((uint32_t)(now_ms - st->decided_ms) > FAN_DECISION_STALE_MS) {
        if (!st->decision_stale) {
            log_print_leveled(LOG_ERROR, "FAN", "No decision for too long");
            st->decision_stale = true;
        }
        fan_ecu_set_output(st, FAN_ON, "decision stale");
    }
    *fan_on = (st->output == FAN_ON);
    return true;
}
