#include "fan_ctrl.h"

/* ON閾値以上ならON、OFF閾値以下ならOFF、その間は前回の要求を保つ（ヒステリシス）。
   閾値付近で水温が上下しても、ON/OFFを繰り返さないようにするため */
FanState fan_ctrl_decide(uint8_t temp, FanState current) {
    if (temp >= FAN_ON_TEMP) {
        return FAN_ON;
    }
    if (temp <= FAN_OFF_TEMP) {
        return FAN_OFF;
    }
    return current;
}
