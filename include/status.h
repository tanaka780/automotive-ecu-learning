/* インクルードガード */
#ifndef STATUS_H
#define STATUS_H

#include "sensor.h"   /* VehicleSensorData を参照するために必要 */

typedef enum {
    LEVEL_NORMAL   = 0,  /* 正常範囲内 */
    LEVEL_WARNING  = 1,  /* 注意（第1段階の閾値を超えた状態） */
    LEVEL_CRITICAL = 2   /* 危険（第2段階の閾値を超えた状態） */
} SensorLevel;

#define STATUS_SPEED_WARN   80   /* 車速 注意レベルの境界 [km/h] */
#define STATUS_SPEED_CRIT  100   /* 車速 危険レベルの境界 [km/h] */

#define STATUS_RPM_WARN   4000   /* RPM 注意レベルの境界 [rpm] */
#define STATUS_RPM_CRIT   5000   /* RPM 危険レベルの境界 [rpm] */

#define STATUS_TEMP_WARN    80   /* 水温 注意レベルの境界 [℃] */
#define STATUS_TEMP_CRIT    90   /* 水温 危険レベルの境界 [℃] */

/* センサの生の値は持たず、分類結果（SensorLevel）だけを持つ */
typedef struct {
    SensorLevel speed_level;  /* 車速の状態レベル */
    SensorLevel rpm_level;    /* RPM の状態レベル */
    SensorLevel temp_level;   /* 水温の状態レベル */
} SensorStatus;

/* 各センサ値を閾値と比較して状態レベルを分類する */
void status_check(SensorStatus *status, const VehicleSensorData *data);
/* 各センサの状態レベルを1行でコンソールに出力する */
void status_print(const SensorStatus *status);

#endif /* STATUS_H */
