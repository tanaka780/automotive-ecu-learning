#include <stdio.h>
#include "status.h"

/*
 * classify_level: 値と2段階の閾値を比較してレベルを返すヘルパー関数
 *
 * static: この関数は status.c の外からは見えない（内部専用）
 *         外部に公開しない関数には static をつけるのが C 言語の慣習
 *
 * 判定ロジック:
 *   value > crit  → LEVEL_CRITICAL（危険）
 *   value > warn  → LEVEL_WARNING（注意）
 *   それ以外       → LEVEL_NORMAL（正常）
 *
 * この関数を作った理由:
 *   speed / rpm / temp の3つで同じ判定ロジックが繰り返されるため、
 *   1つの関数にまとめて重複を避ける。
 */
static SensorLevel classify_level(int value, int warn, int crit) {
    if (value > crit) return LEVEL_CRITICAL;
    if (value > warn) return LEVEL_WARNING;
    return LEVEL_NORMAL;
}

/*
 * status_check: センサ値を読んで各センサの状態レベルを判定する
 *
 * SensorStatus *status: 判定結果を書き込む（ポインタ渡しで呼び出し元を更新）
 * const VehicleSensorData *data: センサ値は「読むだけ」で書き換えない
 *
 * status.c の責務は「センサ値の状態分類」のみ。
 * アラート表示は alert.c、センサ値の更新は sensor.c が担当する。
 */
void status_check(SensorStatus *status, const VehicleSensorData *data) {
    status->speed_level = classify_level(data->speed,
                                         STATUS_SPEED_WARN, STATUS_SPEED_CRIT);
    status->rpm_level   = classify_level(data->rpm,
                                         STATUS_RPM_WARN,   STATUS_RPM_CRIT);
    status->temp_level  = classify_level(data->temperature,
                                         STATUS_TEMP_WARN,  STATUS_TEMP_CRIT);
}

/*
 * level_to_str: SensorLevel の値を表示用文字列に変換するヘルパー関数
 *
 * static: status.c の外からは使わない内部専用関数
 * switch 文で enum の全ケースを網羅している
 * 文字数を8文字で揃えているのは status_print の表示幅を合わせるため
 */
static const char *level_to_str(SensorLevel level) {
    switch (level) {
        case LEVEL_NORMAL:   return "NORMAL  ";
        case LEVEL_WARNING:  return "WARNING ";
        case LEVEL_CRITICAL: return "CRITICAL";
        default:             return "UNKNOWN ";
    }
}

/*
 * status_print: 各センサの状態レベルをコンソールに表示する
 *
 * const SensorStatus *: 状態を「読むだけ」で書き換えない
 * この関数は表示のみを行う。判定は status_check が担当する。
 */
void status_print(const SensorStatus *status) {
    printf("[STATUS] Speed: %s | RPM: %s | Temp: %s\n",
           level_to_str(status->speed_level),
           level_to_str(status->rpm_level),
           level_to_str(status->temp_level));
}
