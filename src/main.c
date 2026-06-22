#include <stdio.h>
#include <stdlib.h>   /* srand(), rand() */
#include <time.h>     /* time() */
#include <unistd.h>   /* sleep() */
#include "sensor.h"
#include "stats.h"
#include "alert.h"
#include "status.h"
#include "diag.h"

/* サンプル数: ここを変えるだけでループ回数を変えられる */
#define SAMPLE_COUNT 20

int main(void) {
    /* srand: time(NULL) を種にすることで実行ごとに異なる乱数列を生成する */
    srand((unsigned int)time(NULL));

    VehicleSensorData sensor_data;   /* センサ値（各サンプルごとに上書き更新） */
    sensor_init(&sensor_data);

    VehicleStats stats;              /* 統計データ（全サンプル分を集計） */
    stats_init(&stats);

    SensorStatus sensor_status;     /* 状態レベル（status_check が毎回上書き、初期化不要） */

    DtcRecord dtc;                  /* DTC記録（センサ別のCRITICAL発生回数、前回状態を内部に保持） */
    diag_init(&dtc);

    /* main.c は処理の順序制御のみ。各処理の詳細はモジュールに書く */
    for (int i = 1; i <= SAMPLE_COUNT; i++) {
        printf("[Sample %02d]\n", i);
        sensor_update(&sensor_data);                  /* センサ値を更新する (書く) */
        sensor_print(&sensor_data);                   /* センサ値を表示する (読む) */
        status_check(&sensor_status, &sensor_data);   /* 状態レベルを判定する */
        status_print(&sensor_status);                 /* 状態レベルを表示する (読む) */
        diag_check(&dtc, &sensor_status);             /* CRITICALに入った瞬間をDTCとして記録する */
        alert_check(&sensor_data);                    /* 閾値超過の警告を表示する (読む) */
        stats_update(&stats, &sensor_data);           /* 統計データを更新する */
        sleep(1);                                     /* 1秒待つ */
    }

    stats_print(&stats);
    diag_print(&dtc);                                 /* DTC一覧を表示する */

    return 0;
}
