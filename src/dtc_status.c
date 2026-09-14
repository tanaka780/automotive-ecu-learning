#include "dtc_status.h"

/* was_bad/is_badの組み合わせに応じて、異常に入った瞬間（エッジ）だけcountを増やし、
   ACTIVE/HISTORY/NONEの状態を進める。異常の発生源（物理センサかCAN通信か等）は一切知らない */
void dtc_status_update(uint8_t *count, DtcStatus *status, bool was_bad, bool is_bad) {
    if (is_bad) {
        if (!was_bad) {
            (*count)++;
        }
        *status = DTC_ACTIVE;
    } else if (*status == DTC_ACTIVE) {
        *status = DTC_HISTORY;
    } else {
        /* NONE、またはHISTORY継続のため何もしない */
    }
}
