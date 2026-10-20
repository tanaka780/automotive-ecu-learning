/* インクルードガード */
#ifndef FAULTMGR_H
#define FAULTMGR_H

#include <stdint.h>   /* uint8_t を使うために必要 */
#include <stdbool.h>  /* bool を使うために必要 */
#include "sensor.h"   /* SensorId / VehicleSensorData / SENSOR_COUNT を参照するために必要 */
#include "status.h"   /* SensorStatus を参照するために必要 */

/* bad_count/good_count（uint8_t）と直接比較するため符号なしリテラルにする（MISRA 10.4） */
/* 悪い状態（車速・回転数はCRITICAL・値域外、水温は値域外）が何サンプル連続したら
   「一時的なノイズ」ではなく確定した異常(Degraded)とみなすか */
#define FAULTMGR_DEBOUNCE_COUNT 3U
/* Degraded中に良い状態（車速・回転数はNORMAL、水温は値域内）が何サンプル連続したら
   復帰(Recovery)とみなすか（Debounceと対称の回数） */
#define FAULTMGR_RECOVERY_COUNT 3U

/* Degraded中にalert_check/stats_updateへ渡す、センサ別のフェイルセーフ値（安全側の固定値） */
#define FAULTMGR_SAFE_SPEED 0     /* 停車相当 [km/h] */
#define FAULTMGR_SAFE_RPM   800   /* アイドリング相当 [rpm] */
#define FAULTMGR_SAFE_TEMP  25    /* 常温相当 [℃] */

/* センサ1つ分の確定状態：まだ確定した異常が無いか、Debounceで確定し縮退動作中か */
typedef enum {
    FAULT_NORMAL = 0,
    FAULT_DEGRADED
} FaultState;

/* センサ別にDebounce/Recoveryの連続回数と確定状態を持つ。添字はSensorId（sensor.h）を共有する */
typedef struct {
    uint8_t    bad_count[SENSOR_COUNT];  /* FAULT_NORMAL中の、悪い状態の連続回数（Debounce用） */
    uint8_t    good_count[SENSOR_COUNT]; /* FAULT_DEGRADED中の、良い状態の連続回数（Recovery用） */
    FaultState state[SENSOR_COUNT];
    /* 確定前の値域外の値の代わりに使う、値域内だった直前の値（Phase27）。
       起動してから値域内の値が一度も無いセンサは、フェイルセーフ値のまま */
    VehicleSensorData last_valid;
    bool       out_of_range[SENSOR_COUNT];   /* 今回の値が値域外か（faultmgr_checkが書き、差し替えで読む） */
} FaultManager;

/* 状態を初期値（FAULT_NORMAL、カウンタ0）にリセットする */
void faultmgr_init(FaultManager *fm);

/* status_checkの分類結果を見て、センサ別にDebounce/Recoveryのカウントを進め
   （何を悪い・良いとするかはセンサごとに違う。FAULTMGR_DEBOUNCE_COUNTのコメント参照）、
   確定(FAULT_DEGRADED)・復帰(FAULT_NORMAL)への遷移を判定する。遷移した瞬間だけログを出す。
   あわせて、rawのうち値域内の値をlast_validに覚え、値域外のセンサをout_of_rangeに記録する（Phase27） */
void faultmgr_check(FaultManager *fm, const SensorStatus *status, const VehicleSensorData *raw);

/* rawのセンサ値は変更せず、FAULT_DEGRADED中のセンサはフェイルセーフ値に、
   まだ確定していない値域外のセンサは値域内だった直前の値（last_valid）に差し替えたコピーをeffectiveに作る（Phase27）。
   status_check/diag_checkはraw（本当の値）を見て診断の正確性を保ち、alert_check/stats_updateはeffective
   （縮退後の値）を見て動作を継続する、という役割分担にするための関数 */
void faultmgr_apply_safe_values(const FaultManager *fm, const VehicleSensorData *raw, VehicleSensorData *effective);

/* 高温（オーバーヒート）の判断に使える水温を返す（Phase27）。値域内なら今回の本当の値、
   値域外なら（確定の前も後も）値域内だった直前の値（起動してから値域内の値が無ければフェイルセーフ値）。
   確定中もフェイルセーフ値に差し替えないため、オーバーヒートの途中で水温センサが壊れても、
   壊れる前の高温で高温の通知を続けられる */
uint8_t faultmgr_usable_temperature(const FaultManager *fm);

#endif /* FAULTMGR_H */
