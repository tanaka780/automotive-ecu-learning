/* インクルードガード */
#ifndef CAN_DIAG_H
#define CAN_DIAG_H

#include <stdint.h>     /* uint8_t を使うために必要 */
#include "can.h"        /* CanMessageId / CanLinkState / CAN_MSG_COUNT を参照するために必要 */
#include "dtc_status.h" /* DtcStatus / dtc_status_update を参照するために必要 */

/* CAN通信リンク別のDTC相当の記録（発生回数・状態区分）。識別子はSensorIdではなくCanMessageIdを使い、
   物理センサのDtcEntry（diag.h）とは型を分ける。実車のDTC分類、U-code（通信異常）がP-code（物理故障）
   とは別カテゴリなのに倣う（Phase20でCanLinkStateをFaultManagerと分けたのと同じ考え方、Phase21） */
typedef struct {
    uint8_t   count[CAN_MSG_COUNT];
    DtcStatus status[CAN_MSG_COUNT];
} CanDtcRecord;

/* 記録を初期値（0件、状態区分はDTC_NONE）にリセットする */
void can_diag_init(CanDtcRecord *rec);

/* CanLinkStateの前回(previous)/今回(current)比較により、msgに対応する発生回数・状態区分を更新する。
   確定Lost（CAN_LINK_LOST）に入った瞬間だけ発生回数を増やす（判定自体はdtc_status_updateに委譲する） */
void can_diag_check(CanDtcRecord *rec, CanMessageId msg, CanLinkState previous, CanLinkState current);

/* 記録された内容をコンソールに出力する */
void can_diag_print(const CanDtcRecord *rec);

#endif /* CAN_DIAG_H */
