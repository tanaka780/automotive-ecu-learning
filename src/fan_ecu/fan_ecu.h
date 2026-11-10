/* インクルードガード */
#ifndef FAN_ECU_H
#define FAN_ECU_H

#include <stdint.h>
#include <stdbool.h>
#include "ignition.h"   /* Ignition を参照するために必要 */
#include "can.h"        /* CanBus / CanMonitor を参照するために必要 */
#include "can_diag.h"   /* CanDtcRecord を参照するために必要 */
#include "fan_ctrl.h"   /* FanState を参照するために必要 */

/* 最後に新しい水温を受け取ってから、これを超えたら水温が古いとみなしてファンをONにする [ms]（Phase32）。
   ゲージデータは1000ms周期で、3回続けて届かないと途絶になる。途絶していない間は、新しい水温どうしの間が
   約3000msを超えないので、4000msを超えたら、ゲージデータの受信のタスクが呼ばれていないと分かる。
   途絶の確定（約3000ms）より短くすると、届かないフレームを途絶より先に古さとして見つけ、理由が混ざる */
#define FAN_TEMP_STALE_MS 4000U

/* 最後に判断してから、これを超えたら判断が古いとみなし、送るときにファンの出力をONにしてONを送る [ms]（Phase32）。
   判断（警告灯データの受信）は200ms周期で、警告灯データが途絶しても「途絶」と判断し続けるので、判断の時刻が
   進まなくなるのは、そのタスクが呼ばれないときだけ。普段の間隔（約200ms）より十分長い5回分にする */
#define FAN_DECISION_STALE_MS 1000U

/* ファン制御ECU役の判断に使う状態（Phase26）。イグニッションOFF→ONで初期値に戻す。
   実車のECUはOFFで電源が落ち、ONの後に新しく受信するまで信号を無効として扱うのに合わせる */
typedef struct {
    bool     powered;          /* 前回呼ばれたときにイグニッションONだったか（OFF→ONの検出用） */
    bool     has_valid_temp;   /* ONの後に、新しく届いた水温を受け取ったか */
    uint8_t  temp;             /* 最後に受け取った水温 [℃] */
    uint32_t temp_sent_ms;     /* その水温の送信時刻 */
    uint32_t temp_received_ms; /* 最後に新しい水温を受け取った時刻（自分の時計）。まだ受け取っていなければ、
                                  イグニッションONにした時刻（Phase32） */
    bool     temp_stale;       /* 水温が古いと判断しているか（切り替わった瞬間だけログを出すため、Phase32） */
    uint32_t decided_ms;       /* 最後に判断した時刻（自分の時計）。まだ判断していなければ、イグニッションONにした時刻（Phase32） */
    bool     decision_stale;   /* 判断が古いと判断しているか（切り替わった瞬間だけログを出すため、Phase32） */
    bool     temp_judged;      /* その水温より後に送られた警告灯データで、使えるかを決めたか */
    bool     temp_usable;      /* 決めた結果。その警告灯データで水温センサが正常ならtrue */
    FanState temp_request;     /* 水温による要求（fan_ctrl_decideに渡す前回の要求） */
    bool     hold_on;          /* 強制ONが解けた後、使える水温が決まるまでONを保つか（Phase27） */
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

/* 3つの関数のnow_msは、呼ばれた時刻（ファン制御ECU自身の時計）。このファイルは時計を読まず、
   呼び出し側（main.c）が渡す（Phase32）。フレームの送信時刻とは別のもの */

/* 警告灯データを受信し、Timeoutの検知・CanDtcRecordへの記録を行ったうえで、ファン出力を決める。
   イグニッションOFF中は何もしない */
void fan_ecu_receive_fault_status(FanEcuContext *ctx, uint32_t now_ms);

/* ゲージデータを受信し、Timeout/Invalid Dataの検知・CanDtcRecordへの記録を行い、新しく届いた水温を保持する。
   ファン出力は決めない（警告灯データを受けたときに決める）。イグニッションOFF中は何もしない */
void fan_ecu_receive_engine_status(FanEcuContext *ctx, uint32_t now_ms);

/* ファン状態として送る値（まとめた後のファン出力）をfan_onに返す。イグニッションOFF中はfalseを返し、
   送らない。最後に判断してからFAN_DECISION_STALE_MSを超えていたら、ファンの出力そのものをONにしてからONを返す
   （判断のタスクが止まっていると、出力が止まる前のまま固まるため。Phase32）。busへの書き込みは呼び出し側（main.c）が行う。エンジン監視ECUの送信と同じく、
   通信故障の注入（sim/）を挟む場所をmain.cに限るため */
bool fan_ecu_get_fan_status(FanEcuContext *ctx, uint32_t now_ms, bool *fan_on);

#endif /* FAN_ECU_H */
