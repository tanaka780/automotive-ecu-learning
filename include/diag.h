/* インクルードガード */
#ifndef DIAG_H
#define DIAG_H

#include <stdint.h>   /* uint8_t を使うために必要 */
#include "status.h"   /* SensorStatus を参照するために必要 */

/* センサ別の「CRITICALへ入った回数」と、エッジ検出用の前回状態を持つ */
typedef struct {
    uint8_t speed_crit_count;  /* 車速がCRITICALになった回数 */
    uint8_t rpm_crit_count;    /* RPM がCRITICALになった回数 */
    uint8_t temp_crit_count;   /* 水温がCRITICALになった回数 */
    SensorStatus previous;     /* 前回のSensorStatus（今回との比較に使う） */
} DtcRecord;

/* DTC記録を初期値（0件）にリセットする */
void diag_init(DtcRecord *dtc);
/* 前回CRITICALでなく今回CRITICALになった瞬間だけをDTCとして記録する */
void diag_check(DtcRecord *dtc, const SensorStatus *status);
/* 記録されたDTC一覧をコンソールに出力する */
void diag_print(const DtcRecord *dtc);

#endif /* DIAG_H */
