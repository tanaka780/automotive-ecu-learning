/* インクルードガード */
#ifndef CAN_H
#define CAN_H

#include <stdint.h>
#include <stdbool.h>
#include "sensor.h"     /* VehicleSensorData を参照するために必要 */

/* CAN通信の実現方式（Phase20、Day43決定）：実プロセス分離は行わず、単一プロセス内で
   エンジンECU役／メーターECU役をSchedulerタスクとして表現する。CanBusは両役が共有する
   「バス役」のメールボックス（メッセージごとに最新の1フレームだけを保持する） */

/* このプロジェクトで扱うCANメッセージの種別。CanBus/CanMonitorの配列添字として使う */
typedef enum {
    CAN_MSG_FAULT_STATUS = 0,  /* 警告灯データ（確定異常フラグ、200ms周期） */
    CAN_MSG_ENGINE_STATUS,     /* ゲージデータ（speed/rpm/tempの生値、1000ms周期） */
    CAN_MSG_FAN_STATUS,        /* ファン状態（ファン制御ECU→メーターECU、1000ms周期、Phase26） */
    CAN_MSG_COUNT
} CanMessageId;

/* メッセージのID。値が小さいほど実車のバス調停で優先される慣例に合わせ、
   異常系（警告灯）を優先させるため小さい値にした */
#define CAN_ID_FAULT_STATUS  0x100U
#define CAN_ID_ENGINE_STATUS 0x200U
#define CAN_ID_FAN_STATUS    0x300U

/* 警告灯データ(data[0])のビット割り当て。SensorId(sensor.h)の並びに合わせる */
#define CAN_FAULT_BIT_SPEED 0x01U
#define CAN_FAULT_BIT_RPM   0x02U
#define CAN_FAULT_BIT_TEMP  0x04U

/* ファン状態(data[0])のビット割り当て（Phase26） */
#define CAN_FAN_BIT_ON 0x01U

/* Timeout/Invalid Dataの確定・復帰に必要な連続回数。faultmgr.cのDebounce/Recoveryと同じ3回 */
#define CAN_DEBOUNCE_COUNT 3U
#define CAN_RECOVERY_COUNT 3U

/* 実CANに準拠したフレーム構造（ID・DLC・data配列）。timestamp_msは送信時刻
   （timer_get_elapsed_msの値）で、受信側が「新しいデータか」「Timeoutしていないか」を
   同じ値から判定するために使う（非ブロッキング方式、Day43決定） */
typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
    uint32_t timestamp_ms;
    bool     valid;   /* まだ一度も送信されていない（起動直後）かどうかの判定用 */
} CanFrame;

/* バス役：メッセージごとに最新の1フレームだけを保持する（実プロセス分離無しの簡易モデル） */
typedef struct {
    CanFrame frames[CAN_MSG_COUNT];
} CanBus;

/* 受信側が復元したゲージデータ */
typedef struct {
    uint8_t  speed;
    uint16_t rpm;
    uint8_t  temperature;
} CanEngineStatus;

/* 警告灯データの中身（センサ別の確定異常フラグ）。送信側はこれを詰め、受信側はこれに復元する */
typedef struct {
    bool fault_speed;
    bool fault_rpm;
    bool fault_temp;
} CanFaultStatus;

/* 受信側が復元したファン状態（Phase26） */
typedef struct {
    bool fan_on;
} CanFanStatus;

/* メッセージ単位の通信リンク状態。物理センサの異常(FaultState)とは別カテゴリとして扱う
   （実車のDTC分類、P-code=センサ等の物理故障とU-code=ECU間通信異常が別カテゴリなのに倣う） */
typedef enum {
    CAN_LINK_OK = 0,
    CAN_LINK_LOST
} CanLinkState;

/* メーターECU役が持つ、メッセージ別の受信監視状態。最後に有効受信した値をキャッシュしておき、
   Timeout中でも直近の値を表示できるようにする */
typedef struct {
    uint32_t        last_seen_timestamp_ms[CAN_MSG_COUNT];
    uint8_t         bad_count[CAN_MSG_COUNT];
    uint8_t         good_count[CAN_MSG_COUNT];
    CanLinkState    state[CAN_MSG_COUNT];
    CanEngineStatus last_engine_status;
    CanFaultStatus  last_fault_status;
    CanFanStatus    last_fan_status;   /* Phase26 */
} CanMonitor;

/* busの全フレームをvalid=falseの初期状態にする */
void can_bus_init(CanBus *bus);
/* monの状態を初期値（CAN_LINK_OK、カウンタ0、キャッシュ0相当）にリセットする */
void can_monitor_init(CanMonitor *mon);

/* エンジンECU役：dataの生値をゲージデータとしてbusへ書く */
void can_send_engine_status(CanBus *bus, const VehicleSensorData *data);
/* エンジンECU役：statusのセンサ別の確定異常フラグをビット詰めし、警告灯データとしてbusへ書く。
   FaultManagerからフラグを作るのは送る側（main.c）で、このファイルはFaultManagerを知らない */
void can_send_fault_status(CanBus *bus, const CanFaultStatus *status);
/* ファン制御ECU役：まとめた後のファン出力(fan_on)をファン状態としてbusへ書く（Phase26）。
   水温による要求・強制ONの区別は送らず、実際の出力だけを送る */
void can_send_fan_status(CanBus *bus, bool fan_on);

/* メーターECU役：busから最新のゲージデータを読み、Timeout（新しいフレームが来ていない）・
   Invalid Data（値域外、validate.cで判定）を検知してmonの状態を更新する。
   outには常に最後に有効受信した値（Timeout中は直近のキャッシュ）を返す */
CanLinkState can_receive_engine_status(CanMonitor *mon, const CanBus *bus, CanEngineStatus *out);
/* メーターECU役：busから最新の警告灯データを読み、Timeoutを検知してmonの状態を更新する */
CanLinkState can_receive_fault_status(CanMonitor *mon, const CanBus *bus, CanFaultStatus *out);
/* メーターECU役：busから最新のファン状態を読み、Timeoutを検知してmonの状態を更新する（Phase26） */
CanLinkState can_receive_fan_status(CanMonitor *mon, const CanBus *bus, CanFanStatus *out);

#endif /* CAN_H */
