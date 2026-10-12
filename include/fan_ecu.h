/* インクルードガード */
#ifndef FAN_ECU_H
#define FAN_ECU_H

#include <stdint.h>
#include <stdbool.h>
#include "ignition.h"   /* Ignition を参照するために必要 */
#include "can.h"        /* CanBus / CanMonitor を参照するために必要 */
#include "can_diag.h"   /* CanDtcRecord を参照するために必要 */
#include "fan_ctrl.h"   /* FanState を参照するために必要 */

/* ファン制御ECU役の判断に使う状態（Phase26）。イグニッションOFF→ONで初期値に戻す。
   実車のECUはOFFで電源が落ち、ONの後に新しく受信するまで信号を無効として扱うのに合わせる */
typedef struct {
    bool     powered;          /* 前回呼ばれたときにイグニッションONだったか（OFF→ONの検出用） */
    bool     has_valid_temp;   /* ONの後に、新しく届いた水温を受け取ったか */
    uint8_t  temp;             /* 最後に受け取った水温 [℃] */
    FanState temp_request;     /* 水温による要求（fan_ctrl_decideに渡す前回の要求） */
    FanState output;           /* 要求をまとめた後のファン出力 */
} FanEcuState;

/* ファン制御ECU役の処理が参照するポインタ一式（Phase26）。MeterContextと同じく、エンジン監視ECUの
   内部状態へのポインタは持たず、CANのbusからだけ受け取る。
   can_monitorはメーターECUとは別のものを渡す。共有すると、先に受信した側だけが新しいフレームと判定し、
   後から受信した側の空振りが同じカウンタに混ざる。途絶の瞬間を片方のECUが見逃し、復帰も確定しなくなる */
typedef struct {
    const Ignition *ignition;
    const CanBus   *can_bus;
    CanMonitor     *can_monitor;
    CanDtcRecord   *can_dtc;
    FanEcuState    *state;
} FanEcuContext;

/* 判断の状態を初期値（水温未受信・要求OFF・出力OFF）にする */
void fan_ecu_init(FanEcuState *state);

/* 警告灯データを受信し、Timeoutの検知・CanDtcRecordへの記録を行ったうえで、ファン出力を決める。
   イグニッションOFF中は何もしない */
void fan_ecu_receive_fault_status(FanEcuContext *ctx);

/* ゲージデータを受信し、Timeout/Invalid Dataの検知・CanDtcRecordへの記録を行い、新しく届いた水温を保持する。
   ファン出力は決めない（警告灯データを受けたときに決める）。イグニッションOFF中は何もしない */
void fan_ecu_receive_engine_status(FanEcuContext *ctx);

/* ファン状態として送る値（まとめた後のファン出力）をfan_onに返す。イグニッションOFF中はfalseを返し、
   送らない。busへの書き込みは呼び出し側（main.c）が行う。エンジン監視ECUの送信と同じく、
   通信故障の注入（sim/）を挟む場所をmain.cに限るため */
bool fan_ecu_get_fan_status(FanEcuContext *ctx, bool *fan_on);

#endif /* FAN_ECU_H */
