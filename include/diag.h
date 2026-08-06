/* インクルードガード */
#ifndef DIAG_H
#define DIAG_H

#include <stdint.h>   /* uint8_t を使うために必要 */
#include <stdbool.h>  /* bool を使うために必要 */
#include "status.h"   /* SensorStatus を参照するために必要 */
#include "sensor.h"   /* VehicleSensorData（フリーズフレーム用）と、SensorId（DtcEntry.sensor等で使用）を参照するために必要 */

/* DTCの状態区分：現在も継続中か、過去に発生して解消済みか */
typedef enum {
    DTC_NONE = 0,  /* まだ一度もCRITICALになっていない */
    DTC_ACTIVE,    /* 現在CRITICAL中 */
    DTC_HISTORY    /* 過去にCRITICALになったが、現在は解消済み */
} DtcStatus;

/* DTC1件分の情報：どのセンサが、何回CRITICALに入ったか、現在の状態区分 */
typedef struct {
    SensorId  sensor;
    uint8_t   count;
    DtcStatus status;
} DtcEntry;

/* 最初にCRITICALが発生した瞬間の車両状態を1件だけ保持するフリーズフレーム */
typedef struct {
    VehicleSensorData data;           /* 発生時の全センサ値 */
    SensorId          trigger_sensor; /* 原因となったセンサ */
    bool              captured;       /* 記録済みかどうか（1件だけ保持するため） */
} FreezeFrame;

/* センサ別のDTCエントリを配列で持ち、エッジ検出用の前回状態も保持する */
typedef struct {
    DtcEntry     entries[SENSOR_COUNT];
    SensorStatus previous;     /* 前回のSensorStatus（今回との比較に使う） */
    FreezeFrame  freeze_frame; /* 最初のCRITICAL発生時のスナップショット */
} DtcRecord;

/* DTC記録を初期値（0件、状態区分はNONE、フリーズフレーム未記録）にリセットする */
void diag_init(DtcRecord *dtc);
/* 前回/今回のCRITICAL比較により、発生回数・ACTIVE/HISTORYの状態区分・フリーズフレームを更新する */
void diag_check(DtcRecord *dtc, const SensorStatus *status, const VehicleSensorData *data);
/* 診断コマンド（"clear"相当）によるクリア要求を受けて、DTC記録・フリーズフレームを初期状態(diag_initと同じ)に戻す */
void diag_clear(DtcRecord *dtc);
/* 記録されたDTC一覧をコンソールに出力する */
void diag_print(const DtcRecord *dtc);

#endif /* DIAG_H */
