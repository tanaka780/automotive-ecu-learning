#include "uds.h"

/* このファイルは、診断の要求を読み解いて応答を作ることだけを担う。記録の数え方・消し方はdiag.c、
   フレームの形（1フレームの長さ）はcan.cが持つ（Phase30） */

/* サービスID。肯定応答は、要求のサービスIDに0x40を足した値で返す */
#define UDS_SID_CLEAR_DTC        0x14U
#define UDS_SID_READ_DTC         0x19U
#define UDS_POSITIVE_OFFSET      0x40U
#define UDS_NEGATIVE_RESPONSE    0x7FU

/* 0x19のサブファンクション：ステータスマスクに当てはまるDTCの件数を返す */
#define UDS_READ_DTC_COUNT_BY_MASK 0x01U

/* 否定応答の理由（NRC） */
#define UDS_NRC_SERVICE_NOT_SUPPORTED     0x11U
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED 0x12U
#define UDS_NRC_INCORRECT_LENGTH          0x13U
#define UDS_NRC_REQUEST_OUT_OF_RANGE      0x31U

/* DTCのステータスのビット。このプログラムの記録が当てはまるのは2つだけ。
   confirmedDTC（bit3）は使わない。記録は、異常が初めて出た回にそのまま作り、確定を経ていないため */
#define UDS_STATUS_TEST_FAILED                  0x01U   /* bit0：今も異常（ACTIVE） */
#define UDS_STATUS_TEST_FAILED_SINCE_LAST_CLEAR 0x20U   /* bit5：前回消してから異常になった（NONE以外） */
#define UDS_STATUS_AVAILABILITY_MASK (UDS_STATUS_TEST_FAILED | UDS_STATUS_TEST_FAILED_SINCE_LAST_CLEAR)

/* DTCの形式：SAE J2012（Pコードの形式）。次の段階でDTCの番号を返すときもこの形式にする */
#define UDS_DTC_FORMAT_J2012 0x00U

/* 否定応答（7F <サービスID> <NRC>）を書き、そのバイト数を返す */
static uint8_t uds_negative(uint8_t sid, uint8_t nrc, uint8_t *response) {
    response[0] = UDS_NEGATIVE_RESPONSE;
    response[1] = sid;
    response[2] = nrc;
    return 3U;
}

/* 記録1件の状態区分を、UDSのステータスのビットに直す。回数ではなく状態区分から作る。
   回数（uint8_t）は256回目に0へ戻るため、異常中でもbit5が0になってしまう */
static uint8_t uds_dtc_status(DtcStatus status) {
    uint8_t bits = 0U;
    if (status == DTC_ACTIVE) {
        bits |= UDS_STATUS_TEST_FAILED;
    }
    if (status != DTC_NONE) {
        bits |= UDS_STATUS_TEST_FAILED_SINCE_LAST_CLEAR;
    }
    return bits;
}

/* 0x19 01：ステータスがマスクと1ビットでも重なる記録の件数を返す */
static uint8_t uds_read_dtc(const DtcRecord *dtc, const uint8_t *request, uint8_t len, uint8_t *response) {
    if (len < 2U) {
        return uds_negative(UDS_SID_READ_DTC, UDS_NRC_INCORRECT_LENGTH, response);
    }
    if (request[1] != UDS_READ_DTC_COUNT_BY_MASK) {
        return uds_negative(UDS_SID_READ_DTC, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED, response);
    }
    if (len != 3U) {
        return uds_negative(UDS_SID_READ_DTC, UDS_NRC_INCORRECT_LENGTH, response);
    }

    uint8_t  mask  = request[2];
    uint16_t count = 0U;
    for (int i = 0; i < (int)DTC_ID_COUNT; i++) {
        if ((uds_dtc_status(dtc->entries[i].status) & mask) != 0U) {
            count++;
        }
    }

    response[0] = (uint8_t)(UDS_SID_READ_DTC + UDS_POSITIVE_OFFSET);
    response[1] = UDS_READ_DTC_COUNT_BY_MASK;
    response[2] = UDS_STATUS_AVAILABILITY_MASK;
    response[3] = UDS_DTC_FORMAT_J2012;
    response[4] = (uint8_t)(count >> 8);
    response[5] = (uint8_t)(count & 0xFFU);
    return 6U;
}

/* 0x14：groupOfDTCが全部（FFFFFF）のときだけ消す。記録の種類ごとの消去は受け付けない */
static uint8_t uds_clear_dtc(DtcRecord *dtc, const uint8_t *request, uint8_t len, uint8_t *response) {
    if (len != 4U) {
        return uds_negative(UDS_SID_CLEAR_DTC, UDS_NRC_INCORRECT_LENGTH, response);
    }
    if ((request[1] != 0xFFU) || (request[2] != 0xFFU) || (request[3] != 0xFFU)) {
        return uds_negative(UDS_SID_CLEAR_DTC, UDS_NRC_REQUEST_OUT_OF_RANGE, response);
    }

    diag_clear(dtc);
    response[0] = (uint8_t)(UDS_SID_CLEAR_DTC + UDS_POSITIVE_OFFSET);
    return 1U;
}

/* 要求のサービスIDで振り分ける */
uint8_t uds_handle_request(DtcRecord *dtc, const uint8_t *request, uint8_t len, uint8_t *response) {
    if (len == 0U) {
        return 0U;
    }

    switch (request[0]) {
        case UDS_SID_READ_DTC:  return uds_read_dtc(dtc, request, len, response);
        case UDS_SID_CLEAR_DTC: return uds_clear_dtc(dtc, request, len, response);
        default:                return uds_negative(request[0], UDS_NRC_SERVICE_NOT_SUPPORTED, response);
    }
}

/* busの要求を読み、ONの間だけ応答する */
void uds_server_run(UdsContext *ctx) {
    uint8_t request[CAN_DIAG_SF_MAX_LEN];
    uint8_t len = 0U;
    if (!can_receive_diag_request(ctx->can_bus, &ctx->last_request_ms, request, &len)) {
        return;
    }
    if (ctx->ignition->current != IGNITION_ON) {
        return;   /* 読んだ要求は捨てる（受け取り済みとしては記録済み） */
    }

    uint8_t response[UDS_RESPONSE_MAX_LEN];
    uint8_t response_len = uds_handle_request(ctx->dtc, request, len, response);
    if (response_len > 0U) {
        (void)can_send_diag_response(ctx->can_bus, response, response_len);
    }
}
