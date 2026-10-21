/* インクルードガード */
#ifndef DIAG_H
#define DIAG_H

#include <stdint.h>   /* uint8_t を使うために必要 */
#include <stdbool.h>  /* bool を使うために必要 */
#include "status.h"   /* SensorStatus を参照するために必要 */
#include "sensor.h"   /* VehicleSensorData（フリーズフレーム用）と、SensorId（センサ単位のクリアで使用）を参照するために必要 */
#include "dtc_status.h" /* DtcStatus を参照するために必要（Phase21、can_diag.cとの共通部分として切り出した） */

/* 記録（DTC）の種類（Phase28）。センサと1対1ではない。水温は、原因の違う2つの故障
   （水温センサの故障と、エンジンの過熱）を別の記録にし、整備で原因を見分けられるようにする */
typedef enum {
    DTC_ID_SPEED = 0,     /* 車速：危険・値域外 */
    DTC_ID_RPM,           /* 回転数：危険・値域外 */
    DTC_ID_TEMP_SENSOR,   /* 水温センサの故障：値域外（断線・ショート） */
    DTC_ID_OVERHEAT,      /* オーバーヒート：判断に使える水温が危険（高温の通知と同じ判断） */
    DTC_ID_COUNT
} DtcId;

/* DTC1件分の情報：どの記録が、何回異常に入ったか、現在の状態区分 */
typedef struct {
    DtcId     id;
    uint8_t   count;
    DtcStatus status;
} DtcEntry;

/* 最初に異常が発生した瞬間の車両状態を1件だけ保持するフリーズフレーム */
typedef struct {
    VehicleSensorData data;      /* 発生時の全センサ値（生の値） */
    DtcId             trigger;   /* 原因となった記録（Phase28で、センサから記録の種類に変えた） */
    bool              captured;  /* 記録済みかどうか（1件だけ保持するため） */
} FreezeFrame;

/* 記録の種類ごとのDTCエントリを配列で持ち、エッジ検出用の前回の異常の有無も保持する */
typedef struct {
    DtcEntry     entries[DTC_ID_COUNT];
    bool         previous_bad[DTC_ID_COUNT]; /* 前回、各記録が異常だったか（今回との比較に使う） */
    FreezeFrame  freeze_frame;                /* 最初の異常発生時のスナップショット */
} DtcRecord;

/* DTC記録を初期値（0件、状態区分はNONE、フリーズフレーム未記録）にリセットする */
void diag_init(DtcRecord *dtc);
/* 前回/今回の異常の比較により、発生回数・ACTIVE/HISTORYの状態区分・フリーズフレームを更新する。
   車速・回転数はCRITICAL・値域外、水温センサの故障は水温の値域外を異常とする。オーバーヒートは、
   呼び出し側が判断に使える水温で決めたoverheat（高温の通知と同じ値）を異常とする（Phase28） */
void diag_check(DtcRecord *dtc, const SensorStatus *status, const VehicleSensorData *data, bool overheat);
/* エンジンをかけ直したとき（イグニッションOFF→ON）に、前回の異常の有無だけを異常なしに戻す。
   切っている間はECUが通電していないので、起動したときと同じく、かけ直した後も異常なら数え直す。
   回数・状態区分・フリーズフレームは残す（起動し直したときに保存ファイルから引き継ぐのと同じ） */
void diag_reset_previous(DtcRecord *dtc);
/* 診断コマンド（"clear"相当）によるクリア要求を受けて、DTC記録・フリーズフレームを初期状態(diag_initと同じ)に戻す */
void diag_clear(DtcRecord *dtc);
/* 診断コマンド（"clear <センサ名>"相当）によるクリア要求を受けて、指定したセンサの記録だけを初期状態に戻す
   （水温は、水温センサの故障とオーバーヒートの2つとも、Phase28）。フリーズフレームは、その原因が
   消した記録だった場合だけ合わせて初期状態に戻し、原因が別の記録なら保持する */
void diag_clear_sensor(DtcRecord *dtc, SensorId sensor);
/* 記録されたDTC一覧をコンソールに出力する */
void diag_print(const DtcRecord *dtc);

#endif /* DIAG_H */
