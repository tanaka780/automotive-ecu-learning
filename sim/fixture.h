/* インクルードガード */
#ifndef FIXTURE_H
#define FIXTURE_H

#include <stdbool.h>  /* bool を使うために必要 */
#include "sensor.h"   /* VehicleSensorData を参照するために必要 */
#include "ignition.h" /* IgnitionState を参照するために必要 */

/* 固定値注入ファイルのデフォルトのファイル名 */
#define FIXTURE_FILENAME "fixture.txt"

/* 指定したファイルをKEY=VALUE形式（MODE=FIXED/RANDOM, SPEED=, RPM=, TEMP=）で読み込む。
   MODE=FIXEDであれば、指定されたセンサ値をdataに反映してtrueを返す
   （SPEED/RPM/TEMPのうち指定の無いキーはdataの値をそのまま保持する）。
   ファイルが無い場合・MODE=RANDOMの場合・MODE行が無い場合はdataを変更せずfalseを返す
   （呼び出し側はsensor_updateによるランダム動作を使う） */
bool fixture_apply(VehicleSensorData *data, const char *filename);

/* 指定したファイルからIGNITION行（IGNITION=ON/OFF）だけを読み込む（Phase25）。
   有効な行があれば最後に書かれた値をstateに入れてtrueを返す（MODEの値には関係しない）。
   ファイルが無い場合・IGNITION行が無い場合・値がON/OFF以外の場合はstateを変更せずfalseを返す
   （呼び出し側はignition_updateによるランダム動作を使う） */
bool fixture_load_ignition(IgnitionState *state, const char *filename);

/* TEMP@N=値 の行を覚えておける数。冷却ファンのON/OFFを確かめる流れ（数回の上下）に足りる数にする */
#define FIXTURE_TEMP_STEP_MAX 16U

/* 実行の途中で水温を変える指定（TEMP@N=値：N秒目のサンプル以降は、その水温にする）。
   センサ値は1秒に1回しか読まないので、ミリ秒ではなくサンプル番号で指定する。
   実時間版・仮想時間版のどちらでも、同じサンプルで同じ値になる */
typedef struct {
    uint8_t count;
    int     from_sample[FIXTURE_TEMP_STEP_MAX];   /* N（1始まりのサンプル番号） */
    uint8_t temperature[FIXTURE_TEMP_STEP_MAX];   /* その水温 [℃] */
} FixtureTempSteps;

/* 指定したファイルからTEMP@N=値の行だけを読み込み、stepsに入れる（MODEの値には関係しない。
   固定値を使うかどうかはfixture_applyの戻り値で呼び出し側が決める）。
   Nが1以上の整数でない行・値域外の値の行は無視する。同じNが2回あれば後に書かれた値を使う。
   FIXTURE_TEMP_STEP_MAXを超えた分は無視する。ファイルが無い場合・行が無い場合はcount=0にしてfalseを返す */
bool fixture_load_temp_steps(FixtureTempSteps *steps, const char *filename);

/* sample_indexのサンプルで使う水温をtemperatureに入れてtrueを返す。sample_index以下で一番大きいNの値を使う。
   当てはまる行が無ければtemperatureを変更せずfalseを返す（呼び出し側はTEMP=の値をそのまま使う） */
bool fixture_temp_at(const FixtureTempSteps *steps, int sample_index, uint8_t *temperature);

/* IGNITION@N=ON/OFF の行を覚えておける数 */
#define FIXTURE_IGNITION_STEP_MAX 16U

/* 実行の途中でイグニッションを切り替える指定（IGNITION@N=ON/OFF：N秒目のサンプル以降は、その状態にする）。
   決め方はTEMP@Nと同じ。エンジンを切っている間の動きと、かけ直したときの動きを、決まった秒で再現するため */
typedef struct {
    uint8_t       count;
    int           from_sample[FIXTURE_IGNITION_STEP_MAX];   /* N（1始まりのサンプル番号） */
    IgnitionState state[FIXTURE_IGNITION_STEP_MAX];
} FixtureIgnitionSteps;

/* 指定したファイルからIGNITION@N=ON/OFFの行だけを読み込み、stepsに入れる。
   使うのはIGNITION=の行でイグニッションを固定したとき（fixture_load_ignitionがtrueのとき）だけで、
   固定しないランダム動作には混ぜない。不正な行の扱い・後勝ち・上限はTEMP@Nと同じ */
bool fixture_load_ignition_steps(FixtureIgnitionSteps *steps, const char *filename);

/* sample_indexのサンプルで使うイグニッションの状態をstateに入れてtrueを返す。決め方はfixture_temp_atと同じ。
   当てはまる行が無ければstateを変更せずfalseを返す（呼び出し側はIGNITION=の状態をそのまま使う） */
bool fixture_ignition_at(const FixtureIgnitionSteps *steps, int sample_index, IgnitionState *state);

#endif /* FIXTURE_H */
