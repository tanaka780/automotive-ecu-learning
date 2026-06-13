/* インクルードガード: 同じヘッダを複数回読み込んでも二重定義にならないようにする仕組み */
#ifndef SENSOR_H
#define SENSOR_H

/*
 * struct: 関連するデータをひとまとめにする型
 * speed / rpm / temperature をバラバラの変数で持つより、
 * 1つの型としてまとめることで関数に渡しやすくなる
 *
 * typedef: 「struct VehicleSensorData」と毎回書かずに
 * 「VehicleSensorData」だけで使えるようにする
 */
typedef struct {
    int speed;        /* 車速 [km/h] */
    int rpm;          /* エンジン回転数 [rpm] */
    int temperature;  /* 水温 [℃] */
} VehicleSensorData;

/*
 * 関数宣言 (プロトタイプ宣言):
 * .h に書くことで、他の .c ファイルからこれらの関数を使えるようになる
 * 実装 (中身) は sensor.c に書く
 *
 * const VehicleSensorData *: このポインタ経由では値を変更しない宣言
 * 「読むだけ」の責務を型で表現している
 */
void sensor_init(VehicleSensorData *data);
void sensor_update(VehicleSensorData *data);
void sensor_print(const VehicleSensorData *data);

#endif /* SENSOR_H */
