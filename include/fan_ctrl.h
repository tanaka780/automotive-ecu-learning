/* インクルードガード */
#ifndef FAN_CTRL_H
#define FAN_CTRL_H

#include <stdint.h>

/* ファン制御ECUの、水温による冷却ファンのON/OFF判断（Phase26）。通信（CAN）は扱わず、
   受け取った値から次の要求を計算するだけにする。受信・送信はファン制御ECU本体のモジュールが担う */

/* 水温によるファン要求のON/OFF境界 [℃]。95℃以上でON、90℃以下でOFF、その間は前回の要求を保つ。
   エンジン監視ECUのSTATUS_TEMP_WARN(100)より低くし、注意の段階になる前にファンが回り始めるようにする
   （実車では後から調整できる設定値として持つが、このプロジェクトではマクロで簡略化する） */
#define FAN_ON_TEMP  95U
#define FAN_OFF_TEMP 90U

typedef enum {
    FAN_OFF = 0,
    FAN_ON
} FanState;

/* temp   : 受信した水温 [℃]。有効な値（水温センサの確定異常・受信途絶中でない）のときだけ呼ぶ
   current: 水温による前回の要求。実際のファン出力ではない（確定異常・受信途絶による強制ONは、
            呼び出し側がこの要求とは別に持ち、まとめて出力を決める）
   戻り値 : 水温による今回の要求 */
FanState fan_ctrl_decide(uint8_t temp, FanState current);

#endif /* FAN_CTRL_H */
