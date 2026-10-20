/* インクルードガード */
#ifndef STATUS_H
#define STATUS_H

#include <stdbool.h>  /* bool を使うために必要（status_is_overheat、Phase27） */
#include "sensor.h"   /* VehicleSensorDataと、levels配列のサイズに使うSENSOR_COUNTを参照するために必要 */
#include "config.h"   /* ConfigData を参照するために必要 */

typedef enum {
    LEVEL_NORMAL   = 0,  /* 正常範囲内 */
    LEVEL_WARNING  = 1,  /* 注意（第1段階の閾値を超えた状態） */
    LEVEL_CRITICAL = 2,  /* 危険（第2段階の閾値を超えた状態） */
    LEVEL_OUT_OF_RANGE = 3 /* 値域外（validate.cの値域の外。断線・ショートのような、ありえない値。Phase27） */
} SensorLevel;

/* config.cのconfig_initがデフォルト値として使う元値（設定ファイル未使用時の閾値そのものではない） */
#define STATUS_SPEED_WARN   80   /* 車速 注意レベルの境界 [km/h] */
#define STATUS_SPEED_CRIT  100   /* 車速 危険レベルの境界 [km/h] */

#define STATUS_RPM_WARN   4000   /* RPM 注意レベルの境界 [rpm] */
#define STATUS_RPM_CRIT   5000   /* RPM 危険レベルの境界 [rpm] */

#define STATUS_TEMP_WARN   100   /* 水温 注意レベルの境界 [℃]（ファンが回っても上がり続けている段階） */
#define STATUS_TEMP_CRIT   110   /* 水温 危険レベルの境界 [℃]（オーバーヒート） */

/* センサの生の値は持たず、分類結果（SensorLevel）だけを持つ。
   添字はSensorId（sensor.h）を使う */
typedef struct {
    SensorLevel levels[SENSOR_COUNT];
} SensorStatus;

/* 各センサ値をconfigの閾値と比較して状態レベルを分類する。
   値域外の値は、閾値と比べる前にLEVEL_OUT_OF_RANGEにする（高い側の値域外を危険と区別するため） */
void status_check(SensorStatus *status, const VehicleSensorData *data, const ConfigData *config);
/* 水温が高温（オーバーヒート）か、つまり危険（CRITICAL）の段階かを、status_checkと同じ閾値で判断する（Phase27）。
   値域外の値はCRITICALにならないため、高温とは判断しない。呼び出し側は、値域外の代わりに
   値域内だった直前の値（faultmgr_usable_temperature）を渡す */
bool status_is_overheat(uint8_t temperature, const ConfigData *config);
/* 各センサの状態レベルを1行でコンソールに出力する */
void status_print(const SensorStatus *status);

#endif /* STATUS_H */
