#include <stdio.h>
#include <stdlib.h>   /* rand() を使うために必要 */
#include "sensor.h"
#include "logger.h"

/* センサ値を初期値（停車・アイドリング・常温）に設定する */
void sensor_init(VehicleSensorData *data) {
    data->speed       = 0;    /* 停車状態 */
    data->rpm         = 800;  /* アイドリング */
    data->temperature = 25;   /* 常温 */
}

/* 実際の ECU はセンサから値を読み取るが、このプロジェクトでは学習用にランダム値で代替する */
void sensor_update(VehicleSensorData *data) {
    data->speed       = (uint8_t)(rand() % 121);          /* 0〜120 km/h */
    data->rpm         = (uint16_t)(800 + rand() % 5201);  /* 800〜6000 rpm */
    data->temperature = (uint8_t)(25 + rand() % 76);      /* 25〜100 ℃ */
}

/* センサ値を1行でコンソールに出力する */
void sensor_print(const VehicleSensorData *data) {
    char line[64];   /* "Speed: 120 km/h | RPM: 6000 | Temp: 100 C" が収まるサイズ */
    snprintf(line, sizeof(line), "Speed: %3d km/h | RPM: %4d | Temp: %3d C",
             (int)data->speed, (int)data->rpm, (int)data->temperature);
    log_print(line);
}
