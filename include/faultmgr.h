/* インクルードガード */
#ifndef FAULTMGR_H
#define FAULTMGR_H

#include <stdint.h>   /* uint8_t を使うために必要 */
#include "sensor.h"   /* SensorId / VehicleSensorData / SENSOR_COUNT を参照するために必要 */
#include "status.h"   /* SensorStatus を参照するために必要 */

/* critical_count/normal_count（uint8_t）と直接比較するため符号なしリテラルにする（MISRA 10.4） */
/* CRITICALが何サンプル連続したら「一時的なノイズ」ではなく確定した異常(Degraded)とみなすか */
#define FAULTMGR_DEBOUNCE_COUNT 3U
/* Degraded中にNORMALが何サンプル連続したら復帰(Recovery)とみなすか（Debounceと対称の回数） */
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
    uint8_t    critical_count[SENSOR_COUNT]; /* FAULT_NORMAL中の連続CRITICAL回数（Debounce用） */
    uint8_t    normal_count[SENSOR_COUNT];   /* FAULT_DEGRADED中の連続NORMAL回数（Recovery用） */
    FaultState state[SENSOR_COUNT];
} FaultManager;

/* 状態を初期値（FAULT_NORMAL、カウンタ0）にリセットする */
void faultmgr_init(FaultManager *fm);

/* status_checkの分類結果を見て、センサ別にDebounce/Recoveryのカウントを進め、
   確定(FAULT_DEGRADED)・復帰(FAULT_NORMAL)への遷移を判定する。遷移した瞬間だけログを出す */
void faultmgr_check(FaultManager *fm, const SensorStatus *status);

/* rawのセンサ値は変更せず、FAULT_DEGRADED中のセンサだけフェイルセーフ値に差し替えたコピーをeffectiveに作る。
   status_check/diag_checkはraw（本当の値）を見て診断の正確性を保ち、alert_check/stats_updateはeffective
   （縮退後の値）を見て動作を継続する、という役割分担にするための関数 */
void faultmgr_apply_safe_values(const FaultManager *fm, const VehicleSensorData *raw, VehicleSensorData *effective);

#endif /* FAULTMGR_H */
