/* インクルードガード */
#ifndef FIXTURE_H
#define FIXTURE_H

#include <stdbool.h>  /* bool を使うために必要 */
#include "sensor.h"   /* VehicleSensorData を参照するために必要 */

/* 固定値注入ファイルのデフォルトのファイル名 */
#define FIXTURE_FILENAME "fixture.txt"

/* 指定したファイルをKEY=VALUE形式（MODE=FIXED/RANDOM, SPEED=, RPM=, TEMP=）で読み込む。
   MODE=FIXEDであれば、指定されたセンサ値をdataに反映してtrueを返す
   （SPEED/RPM/TEMPのうち指定の無いキーはdataの値をそのまま保持する）。
   ファイルが無い場合・MODE=RANDOMの場合・MODE行が無い場合はdataを変更せずfalseを返す
   （呼び出し側はsensor_updateによるランダム動作を使う） */
bool fixture_apply(VehicleSensorData *data, const char *filename);

#endif /* FIXTURE_H */
