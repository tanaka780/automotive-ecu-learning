/* インクルードガード */
#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <stdint.h>  /* uint8_t / uint16_t などの固定幅整数型 */

/* センサの値の型と、どのセンサかの番号（Phase31でsensor.hから分けた）。
   エンジン監視ECUのセンサの処理（sensor.h）だけでなく、共通部品のcan.h（ゲージデータを送る関数の引数）と
   validate.h（受信側の値域の確認）も使うため、センサの処理の宣言とは別のヘッダに置く */

typedef struct {
    uint8_t  speed;        /* 車速 [km/h]: 0〜120（停車〜一般道上限相当）。値域チェックの正本はvalidate.h参照 */
    uint16_t rpm;          /* エンジン回転数 [rpm]: 0〜6000（アイドリング〜レッドライン）。値域チェックの正本はvalidate.h参照 */
    uint8_t  temperature;  /* 水温 [℃]: 25〜130（常温〜オーバーヒートより上）。値域チェックの正本はvalidate.h参照 */
} VehicleSensorData;

/* センサ種別。SensorStatus.levels / DtcRecord.entries の配列インデックスとしても使う */
typedef enum {
    SENSOR_SPEED = 0,
    SENSOR_RPM,
    SENSOR_TEMP,
    SENSOR_COUNT   /* センサ種別の数（配列サイズに使う。実際のセンサ種別ではない） */
} SensorId;

#endif /* SENSOR_TYPES_H */
