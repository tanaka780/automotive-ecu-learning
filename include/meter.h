/* インクルードガード */
#ifndef METER_H
#define METER_H

#include "ignition.h"   /* Ignition を参照するために必要 */
#include "can.h"        /* CanBus / CanMonitor を参照するために必要 */
#include "can_diag.h"   /* CanDtcRecord を参照するために必要 */

/* メーターECU役の受信処理が参照するポインタ一式。警告灯・ゲージの両受信で共有する。
   エンジン監視ECUの内部状態(fault_mgr等)へのポインタは持たず、CANのbusからだけ受け取る（Phase26）。
   can.hを通じてFaultManagerの型自体は見えるが、ポインタを持たないため触れない。
   ignitionだけはエンジン監視ECUと同じ構造体を読む。実車で各ECUが電源線からIG信号を受け取るのに相当する */
typedef struct {
    const Ignition *ignition;
    const CanBus   *can_bus;
    CanMonitor     *can_monitor;
    CanDtcRecord   *can_dtc;         /* Phase21 */
} MeterContext;

/* 警告灯データを受信し、Timeoutの検知・CanDtcRecordへの記録・表示を行う。イグニッションOFF中は何もしない */
void meter_receive_fault_status(MeterContext *ctx);

/* ゲージデータを受信し、Timeout/Invalid Dataの検知・CanDtcRecordへの記録・表示を行う。イグニッションOFF中は何もしない */
void meter_receive_engine_status(MeterContext *ctx);

#endif /* METER_H */
