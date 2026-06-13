#include <stdio.h>
#include <stdlib.h>   /* rand() を使うために必要 */
#include "sensor.h"

/*
 * sensor_init: センサデータを初期値に設定する
 * *data はポインタ経由で呼び出し元の変数を直接書き換える
 * & を付けて渡された変数のアドレスを受け取り、-> で中身にアクセスする
 */
void sensor_init(VehicleSensorData *data) {
    data->speed       = 0;    /* 停車状態 */
    data->rpm         = 800;  /* アイドリング */
    data->temperature = 25;   /* 常温 */
}

/*
 * sensor_update: センサ値をランダムに更新する
 * rand() % N で 0 以上 N 未満の乱数を生成する
 * 例: rand() % 121 → 0〜120 の範囲
 *
 * 実際の ECU はセンサから値を読み取るが、
 * このプロジェクトでは学習用にランダム値で代替する
 */
void sensor_update(VehicleSensorData *data) {
    data->speed       = rand() % 121;         /* 0〜120 km/h */
    data->rpm         = 800 + rand() % 5201;  /* 800〜6000 rpm */
    data->temperature = 25 + rand() % 76;     /* 25〜100 ℃ */
}

/*
 * sensor_print: センサ値をコンソールに表示する
 * const VehicleSensorData *: 表示するだけで値は変更しない
 * %3d / %4d: 桁数を固定して表示を揃える
 */
void sensor_print(const VehicleSensorData *data) {
    printf("Speed: %3d km/h | RPM: %4d | Temp: %3d C\n",
           data->speed, data->rpm, data->temperature);
}
