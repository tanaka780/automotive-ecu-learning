#ifndef ALERT_H
#define ALERT_H

#include "sensor.h"   /* VehicleSensorData を参照するために必要 */

/*
 * アラート閾値の定義
 *
 * #define は定数に名前をつける仕組み（プリプロセッサマクロ）
 * 数値をそのまま書くと意味が分かりにくいため、名前で表す
 * 閾値を変えたいときはここだけ修正すればよい
 */
#define ALERT_SPEED_MAX  100  /* 車速の上限値 [km/h] */
#define ALERT_RPM_MAX   5000  /* エンジン回転数の上限値 [rpm] */
#define ALERT_TEMP_MAX    90  /* 水温の上限値 [℃] */

/*
 * alert_check: センサ値が閾値を超えていないか確認し、超えていれば警告を表示する
 * const VehicleSensorData *: センサ値を「読むだけ」で書き換えない
 * 実装は alert.c に書く
 */
void alert_check(const VehicleSensorData *data);

#endif /* ALERT_H */
