/* インクルードガード */
#ifndef DTC_STATUS_H
#define DTC_STATUS_H

#include <stdint.h>   /* uint8_t を使うために必要 */
#include <stdbool.h>  /* bool を使うために必要 */

/* DTCの状態区分：現在も継続中か、過去に発生して解消済みか。物理センサ(diag.c)・CAN通信(can_diag.c)など、
   異常の発生源によらず共通で使う（Phase21、debounce.cと同じ「2つ目の具体的な利用先ができたら汎用化する」
   基準でdiag.cから切り出した） */
typedef enum {
    DTC_NONE = 0,  /* まだ一度も異常になっていない */
    DTC_ACTIVE,    /* 現在異常中 */
    DTC_HISTORY    /* 過去に異常になったが、現在は解消済み */
} DtcStatus;

/* 前回/今回の異常有無(was_bad/is_bad)を比較し、異常に入った瞬間（エッジ）だけcountを増やし、
   現在異常中ならDTC_ACTIVE、異常から外れたらDTC_HISTORYへ状態を進める */
void dtc_status_update(uint8_t *count, DtcStatus *status, bool was_bad, bool is_bad);

#endif /* DTC_STATUS_H */
