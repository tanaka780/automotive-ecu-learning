/* インクルードガード */
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdint.h>   /* uint8_t / uint16_t を使うために必要 */
#include "sensor.h"   /* VehicleSensorData を参照するために必要 */
#include "diag.h"     /* DtcRecord を参照するために必要 */

/* 期待した結果かどうかを表示し、pass/fail件数を数える */
void test_check(const char *label, int condition);

/* main.c の1サンプル分の流れ（status_check → diag_check）を再現する */
void test_run_sample(DtcRecord *dtc, const VehicleSensorData *data);

/* 固定値のセンサ値を1サンプル分作って流し込む */
void test_feed(DtcRecord *dtc, uint8_t speed, uint16_t rpm, uint8_t temperature);

/* pass/fail件数のサマリを表示する。戻り値は全て成功なら0、1件でも失敗があれば1 */
int test_summary(void);

#endif /* TEST_COMMON_H */
