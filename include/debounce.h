/* インクルードガード */
#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <stdint.h>
#include <stdbool.h>

/* 「一時的なノイズ」と「確定した異常」を区別する、Debounce（確定）・Recovery（復帰）の
   連続回数カウントだけを汎用化したもの。対象（センサ値かCAN通信か等）の意味は一切知らない。
   faultmgr.c（センサ異常）とcan.c（CAN通信異常、Phase20）の両方から呼ばれる */

/* is_confirmed: 呼び出し時点で確定状態かどうか（呼び出し側が保持する状態をそのまま渡す）
   is_bad      : 未確定中に確定へ進める条件を満たすか（センサ:CRITICAL、CAN:未受信/値域外）
   is_good     : 確定中に復帰へ進める条件を満たすか（センサ:NORMAL、CAN:正常受信）
   bad_count/good_count: 呼び出し側が保持する連続回数カウンタへのポインタ（この関数が更新する）
   confirm_threshold/recover_threshold: 確定/復帰に必要な連続回数
   戻り値: 更新後に確定状態かどうか（呼び出し側はこれを自分の状態(enum)に反映する） */
bool debounce_update(bool is_confirmed, bool is_bad, bool is_good,
                      uint8_t *bad_count, uint8_t *good_count,
                      uint8_t confirm_threshold, uint8_t recover_threshold);

#endif /* DEBOUNCE_H */
