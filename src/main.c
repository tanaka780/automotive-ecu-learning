#include <stdio.h>
#include <stdlib.h>   /* srand(), rand() */
#include <time.h>     /* time() */
#include <unistd.h>   /* sleep() */
#include "sensor.h"

/* サンプル数: ここを変えるだけでループ回数を変えられる */
#define SAMPLE_COUNT 20

int main(void) {
    /*
     * srand: 乱数の種を設定する
     * time(NULL) で実行するたびに異なる種になるため、毎回違うセンサ値が得られる
     * srand を呼ばないと毎回同じ値になる
     */
    srand((unsigned int)time(NULL));

    /*
     * VehicleSensorData をローカル変数として宣言する
     * sensor_init に & を付けて渡すことで、
     * 関数の中からこの変数を直接書き換えられる
     */
    VehicleSensorData sensor_data;
    sensor_init(&sensor_data);

    /*
     * メインループ: 1秒ごとにセンサ値を更新・表示する
     * main.c は「何を・どの順番で呼ぶか」だけを管理する
     * 各処理の中身は対応するモジュール (.c ファイル) に書く
     */
    for (int i = 1; i <= SAMPLE_COUNT; i++) {
        printf("[Sample %02d]\n", i);
        sensor_update(&sensor_data);  /* センサ値を更新する (書く) */
        sensor_print(&sensor_data);   /* センサ値を表示する (読む) */
        sleep(1);                     /* 1秒待つ */
    }

    return 0;
}
