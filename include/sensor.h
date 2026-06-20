/* インクルードガード: 同じヘッダを複数回読み込んでも二重定義にならないようにする仕組み */
#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>  /* uint8_t / uint16_t などの固定幅整数型 */

typedef struct {
    uint8_t  speed;        /* 車速 [km/h]: 0〜120（停車〜一般道上限相当） */
    uint16_t rpm;          /* エンジン回転数 [rpm]: 0〜6000（アイドリング〜レッドライン） */
    uint8_t  temperature;  /* 水温 [℃]: 25〜100（常温〜冷却水沸点） */
} VehicleSensorData;

/* センサ値を初期値（停車・アイドリング・常温）に設定する */
void sensor_init(VehicleSensorData *data);
/* センサ値をランダム値で更新する（ECU のセンサ読み取りを模倣） */
void sensor_update(VehicleSensorData *data);
/* センサ値を1行でコンソールに出力する */
void sensor_print(const VehicleSensorData *data);

#endif /* SENSOR_H */
