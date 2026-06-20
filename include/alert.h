#ifndef ALERT_H
#define ALERT_H

#include "sensor.h"   /* VehicleSensorData を参照するために必要 */

#define ALERT_SPEED_MAX  100  /* 車速の上限値 [km/h] */
#define ALERT_RPM_MAX   5000  /* エンジン回転数の上限値 [rpm] */
#define ALERT_TEMP_MAX    90  /* 水温の上限値 [℃] */

/* 各センサ値を閾値と比較し、超えていれば警告を出力する */
void alert_check(const VehicleSensorData *data);

#endif /* ALERT_H */
