/* インクルードガード */
#ifndef UDS_H
#define UDS_H

#include <stdint.h>     /* uint8_t / uint32_t を使うために必要 */
#include "diag.h"       /* DtcRecord を参照するために必要 */
#include "ignition.h"   /* Ignition を参照するために必要 */
#include "can.h"        /* CanBus を参照するために必要 */

/* エンジン監視ECU役の診断（UDS、ISO 14229）の応答（Phase30）。CANで受け取った要求に対して、
   記録の件数の読み出し（0x19 01）と全消去（0x14 FFFFFF）に応答し、それ以外には否定応答（NRC）を返す。
   1フレームに収まる応答だけを扱う */

/* 応答の最大バイト数。0x19 01の肯定応答（59 01 <対応ビット> <形式> <件数の上位> <件数の下位>）が最も長い */
#define UDS_RESPONSE_MAX_LEN 6U

/* 応答のタスクが参照するポインタ一式と、最後に受け取った要求の送信時刻 */
typedef struct {
    const Ignition *ignition;
    CanBus         *can_bus;
    DtcRecord      *dtc;
    uint32_t        last_request_ms;   /* can_receive_diag_requestに渡す。起動時は0 */
} UdsContext;

/* 要求（request、lenバイト）を処理し、応答をresponse（UDS_RESPONSE_MAX_LENバイト以上の配列）に書いて、
   そのバイト数を返す。消去の要求ならdtcを消す。lenが0なら何もせず0を返す */
uint8_t uds_handle_request(DtcRecord *dtc, const uint8_t *request, uint8_t len, uint8_t *response);

/* busに新しい要求があれば処理して応答をbusへ書く。イグニッションOFFの間は、ECUに電気が来ていないので
   要求を受け取れない。そのため要求を読んで捨て、ONになってから古い要求に答えないようにする */
void uds_server_run(UdsContext *ctx);

#endif /* UDS_H */
