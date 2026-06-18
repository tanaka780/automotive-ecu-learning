/* インクルードガード */
#ifndef STATUS_H
#define STATUS_H

#include "sensor.h"   /* VehicleSensorData を参照するために必要 */

/*
 * SensorLevel: センサ値の状態を3段階で表す列挙型（enum）
 *
 * enum を使う理由:
 *   数値（0, 1, 2）をそのまま使うと意味が分かりにくい。
 *   LEVEL_NORMAL / LEVEL_WARNING / LEVEL_CRITICAL という名前をつけることで
 *   コードを読んだだけで状態の意味が伝わる。
 *
 * typedef enum: 毎回 "enum SensorLevel" と書かずに "SensorLevel" だけで使えるようにする
 */
typedef enum {
    LEVEL_NORMAL   = 0,  /* 正常範囲内 */
    LEVEL_WARNING  = 1,  /* 注意（第1段階の閾値を超えた状態） */
    LEVEL_CRITICAL = 2   /* 危険（第2段階の閾値を超えた状態） */
} SensorLevel;

/*
 * 各センサの2段階閾値
 *
 * alert.h の閾値（上限1つ）とは異なり、status.h では WARN / CRIT の2段階を持つ。
 * この2段階設計により、緊急度に応じた段階的な対応が可能になる。
 *
 * 設計例:
 *   speed <= 80    → NORMAL（通常走行）
 *   80 < speed <= 100 → WARNING（速度注意）
 *   speed > 100    → CRITICAL（速度超過）
 */
#define STATUS_SPEED_WARN   80   /* 車速 注意レベルの境界 [km/h] */
#define STATUS_SPEED_CRIT  100   /* 車速 危険レベルの境界 [km/h] */

#define STATUS_RPM_WARN   4000   /* RPM 注意レベルの境界 [rpm] */
#define STATUS_RPM_CRIT   5000   /* RPM 危険レベルの境界 [rpm] */

#define STATUS_TEMP_WARN    80   /* 水温 注意レベルの境界 [℃] */
#define STATUS_TEMP_CRIT    90   /* 水温 危険レベルの境界 [℃] */

/*
 * SensorStatus: 各センサの状態レベルをひとまとめにする型
 *
 * センサ値そのもの（speed, rpm, temperature）は持たない。
 * 「今の値がどの状態カテゴリに属するか」という分類結果だけを持つ。
 * これにより、status.c の責務が「分類と表示」だけに絞られる。
 */
typedef struct {
    SensorLevel speed_level;  /* 車速の状態レベル */
    SensorLevel rpm_level;    /* RPM の状態レベル */
    SensorLevel temp_level;   /* 水温の状態レベル */
} SensorStatus;

/*
 * 関数宣言:
 *   status_check: センサ値を読んで状態レベルを判定し、SensorStatus に書き込む
 *   status_print: 判定済みの状態レベルを表示する（書き込みなし）
 *
 * const VehicleSensorData *data: センサ値は「読むだけ」で書き換えない
 */
void status_check(SensorStatus *status, const VehicleSensorData *data);
void status_print(const SensorStatus *status);

#endif /* STATUS_H */
