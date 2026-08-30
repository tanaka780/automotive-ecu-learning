#include "validate.h"
#include "logger.h"   /* LOG_INFO/LOG_ERROR を参照するために必要 */

/* センサごとの物理的な値域。SensorId（sensor.h）を添字にすることで、
   diag.c/status.cと同じ「enum+配列」のパターンをそのまま使い、
   センサ別のif分岐を増やさない */
typedef struct {
    int min;
    int max;
} SensorRange;

bool validate_in_range(SensorId id, int value) {
    /* validate_in_range内でのみ使うため、関数内のstatic constとして定義する（MISRA 8.9） */
    static const SensorRange sensor_ranges[SENSOR_COUNT] = {
        [SENSOR_SPEED] = {0, 120},    /* 車速 [km/h]: 停車〜一般道上限相当 */
        [SENSOR_RPM]   = {0, 6000},   /* エンジン回転数 [rpm]: アイドリング〜レッドライン */
        [SENSOR_TEMP]  = {25, 100},   /* 水温 [℃]: 常温〜冷却水沸点手前 */
    };

    if (id >= SENSOR_COUNT) {
        return false;
    }
    return value >= sensor_ranges[id].min && value <= sensor_ranges[id].max;
}

bool validate_log_level(int value) {
    /* LOG_INFO/LOG_ERRORはenum定数のため、int（value）との比較にはキャストが必要（MISRA 10.4） */
    return value >= (int)LOG_INFO && value <= (int)LOG_ERROR;
}
