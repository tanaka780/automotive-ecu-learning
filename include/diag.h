/* インクルードガード */
#ifndef DIAG_H
#define DIAG_H

#include <stdint.h>   /* uint8_t を使うために必要 */
#include "status.h"   /* SensorStatus を参照するために必要 */

/* センサ種別。DtcRecord.entries の配列インデックスとしても使う */
typedef enum {
    SENSOR_SPEED = 0,
    SENSOR_RPM,
    SENSOR_TEMP,
    SENSOR_COUNT   /* センサ種別の数（配列サイズに使う。実際のセンサ種別ではない） */
} SensorId;

/* DTC1件分の情報：どのセンサが、何回CRITICALに入ったか */
typedef struct {
    SensorId sensor;
    uint8_t  count;
} DtcEntry;

/* センサ別のDTCエントリを配列で持ち、エッジ検出用の前回状態も保持する */
typedef struct {
    DtcEntry     entries[SENSOR_COUNT];
    SensorStatus previous;     /* 前回のSensorStatus（今回との比較に使う） */
} DtcRecord;

/* DTC記録を初期値（0件）にリセットする */
void diag_init(DtcRecord *dtc);
/* 前回CRITICALでなく今回CRITICALになった瞬間だけをDTCとして記録する */
void diag_check(DtcRecord *dtc, const SensorStatus *status);
/* 記録されたDTC一覧をコンソールに出力する */
void diag_print(const DtcRecord *dtc);

#endif /* DIAG_H */
