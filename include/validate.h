/* インクルードガード */
#ifndef VALIDATE_H
#define VALIDATE_H

#include <stdbool.h>
#include "sensor.h"   /* SensorId を参照するために必要 */

/* センサidの物理的な値域（下限・上限）内かを判定する。
   config.c（閾値）・fixture.c（センサ固定値）のどちらも、パース直後のint値
   （uint8_t/uint16_tへキャストする前）を渡して使う想定。値域外ならfalseを返す。
   値域そのものの根拠はvalidate.cのg_sensor_ranges参照（sensor.hのコメントより
   こちらを正本とする） */
bool validate_in_range(SensorId id, int value);

/* LogLevel（logger.h、0=LOG_INFO〜2=LOG_ERROR）として妥当な範囲かを判定する。
   SensorIdの値域テーブルとは別枠（センサではなくログレベルのenum範囲チェックのため） */
bool validate_log_level(int value);

#endif /* VALIDATE_H */
