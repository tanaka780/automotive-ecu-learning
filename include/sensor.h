/* インクルードガード: 同じヘッダを複数回読み込んでも二重定義にならないようにする仕組み */
#ifndef SENSOR_H
#define SENSOR_H

#include "sensor_types.h"  /* VehicleSensorData・SensorId（Phase31で分けた） */

/* センサ値を初期値（停車・アイドリング・常温）に設定する */
void sensor_init(VehicleSensorData *data);
/* センサ値をランダム値で更新する（ECU のセンサ読み取りを模倣） */
void sensor_update(VehicleSensorData *data);
/* センサ値を1行でコンソールに出力する */
void sensor_print(const VehicleSensorData *data);

#endif /* SENSOR_H */
