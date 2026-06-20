#ifndef STATS_H
#define STATS_H

#include <stdint.h>   /* uint8_t / uint16_t / uint32_t などの固定幅整数型 */
#include "sensor.h"   /* VehicleSensorData を参照するために必要 */

typedef struct {
    uint8_t  speed_min;   /* 車速の最小値 [km/h] */
    uint8_t  speed_max;   /* 車速の最大値 [km/h] */
    uint16_t speed_sum;   /* 車速の合計: 最大 20×120=2,400 → uint16_t で十分 */

    uint16_t rpm_min;     /* RPM の最小値 */
    uint16_t rpm_max;     /* RPM の最大値 */
    uint32_t rpm_sum;     /* RPM の合計: 最大 20×6,000=120,000 → uint16_t を超えるため uint32_t */

    uint8_t  temp_min;    /* 水温の最小値 [℃] */
    uint8_t  temp_max;    /* 水温の最大値 [℃] */
    uint16_t temp_sum;    /* 水温の合計: 最大 20×100=2,000 → uint16_t で十分 */

    uint8_t  count;       /* 記録したサンプル数: 最大 20 → uint8_t（最大 255）で十分 */
} VehicleStats;

/* 統計データを初期値にリセットする */
void stats_init(VehicleStats *stats);
/* 今回のサンプル値で min / max / sum / count を更新する */
void stats_update(VehicleStats *stats, const VehicleSensorData *data);
/* 統計データ（min / max / avg）をコンソールに出力する */
void stats_print(const VehicleStats *stats);

#endif /* STATS_H */
