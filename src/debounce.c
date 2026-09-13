#include "debounce.h"

/* 未確定中はis_badの連続回数を数え、confirm_threshold回連続で確定に進める。
   確定中はis_goodの連続回数を数え、recover_threshold回連続で復帰させる。
   確定/復帰しなかった場合、条件を満たさなかった側のカウンタを0に数え直す（連続回数が途切れたとみなす） */
bool debounce_update(bool is_confirmed, bool is_bad, bool is_good,
                      uint8_t *bad_count, uint8_t *good_count,
                      uint8_t confirm_threshold, uint8_t recover_threshold) {
    if (!is_confirmed) {
        if (is_bad) {
            (*bad_count)++;
            if (*bad_count >= confirm_threshold) {
                *good_count = 0U;
                return true;
            }
        } else {
            *bad_count = 0U;
        }
        return false;
    } else {
        if (is_good) {
            (*good_count)++;
            if (*good_count >= recover_threshold) {
                *bad_count = 0U;
                return false;
            }
        } else {
            *good_count = 0U;
        }
        return true;
    }
}
