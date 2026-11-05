#include <stdio.h>
#include <ctype.h>
#include "uds_tester.h"
#include "logger.h"
#include "timer.h"

#define UDS_TESTER_LINE_SIZE 64

/* "19 01 FF" のような、空白で区切った16進数のバイト列を読む。1〜7バイトで、どれも0〜FFならtrue */
static bool uds_tester_parse_bytes(const char *text, uint8_t *data, uint8_t *len) {
    size_t       pos   = 0U;   /* 読み終えた文字数。ポインタに足さず添字で進める（MISRA 18.4） */
    uint8_t      count = 0U;
    unsigned int value;
    int          consumed;

    while (sscanf(&text[pos], " %x%n", &value, &consumed) == 1) {
        if ((value > 0xFFU) || (count >= CAN_DIAG_SF_MAX_LEN)) {
            return false;
        }
        data[count] = (uint8_t)value;
        count++;
        pos += (size_t)consumed;
    }
    /* 読めなくなった後に空白以外が残っていれば、16進数でない値がある */
    while (isspace((unsigned char)text[pos]) != 0) {
        pos++;
    }
    if ((text[pos] != '\0') || (count == 0U)) {
        return false;
    }

    *len = count;
    return true;
}

/* "REQ@N=19 01 FF" の1行を解釈し、有効ならat_sample・data・lenに入れてtrueを返す。
   キーの決め方はfixture.txtのTEMP@N・IGNITION@Nと同じ */
static bool uds_tester_line(const char *line, int *at_sample, uint8_t *data, uint8_t *len) {
    char key[UDS_TESTER_LINE_SIZE];
    char str_value[UDS_TESTER_LINE_SIZE];
    int  n;
    char extra;

    if (sscanf(line, "%63[^=]=%63[^\n]", key, str_value) != 2) {
        return false;
    }
    if ((sscanf(key, "REQ@%d%c", &n, &extra) != 1) || (n < 1)) {
        return false;
    }
    if (!uds_tester_parse_bytes(str_value, data, len)) {
        return false;
    }

    *at_sample = n;
    return true;
}

bool uds_tester_load(UdsTesterScript *script, const char *filename) {
    script->count = 0U;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return false;   /* 要求を書いていない実行では、何も表示しない */
    }

    char line[UDS_TESTER_LINE_SIZE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (script->count >= UDS_TESTER_REQUEST_MAX) {
            break;
        }
        uint8_t i = script->count;
        if (uds_tester_line(line, &script->at_sample[i], script->data[i], &script->len[i])) {
            script->count++;
        }
    }
    /* 読み込みモードのfcloseのため、失敗しても既読データの正しさに影響しない。戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)fclose(fp);

    if (script->count == 0U) {
        return false;
    }

    char msg[32];   /* "Requests loaded: 16" が収まるサイズ */
    (void)snprintf(msg, sizeof(msg), "Requests loaded: %d", (int)script->count);
    log_print_leveled(LOG_INFO, "TESTER", msg);
    return true;
}

void uds_tester_init(UdsTesterContext *ctx, const UdsTesterScript *script, const int *sample_index, CanBus *bus) {
    ctx->script           = script;
    ctx->sample_index     = sample_index;
    ctx->can_bus          = bus;
    ctx->next             = 0U;
    ctx->waiting          = false;
    ctx->sent_ms          = 0U;
    ctx->last_response_ms = 0U;
}

/* "Request 19 01 FF" のように、見出しの後ろにバイト列を16進数で並べて表示する */
static void uds_tester_print(const char *label, const uint8_t *data, uint8_t len) {
    char msg[48];   /* "Response" + " XX"×7 が収まるサイズ */
    int  pos = snprintf(msg, sizeof(msg), "%s", label);
    for (uint8_t i = 0U; i < len; i++) {
        pos += snprintf(&msg[pos], sizeof(msg) - (size_t)pos, " %02X", (unsigned int)data[i]);
    }
    log_print_leveled(LOG_INFO, "TESTER", msg);
}

void uds_tester_run(UdsTesterContext *ctx) {
    if (ctx->waiting) {
        uint8_t response[CAN_DIAG_SF_MAX_LEN];
        uint8_t len = 0U;
        if (can_receive_diag_response(ctx->can_bus, &ctx->last_response_ms, response, &len)) {
            uds_tester_print("Response", response, len);
            ctx->waiting = false;
        } else if ((timer_get_elapsed_ms() - ctx->sent_ms) >= UDS_TESTER_P2_MS) {
            log_print_leveled(LOG_INFO, "TESTER", "No response");
            ctx->waiting = false;
        } else {
            return;   /* まだ待つ */
        }
    }

    const UdsTesterScript *script = ctx->script;
    if ((ctx->next >= script->count) || (*ctx->sample_index < script->at_sample[ctx->next])) {
        return;
    }

    uint8_t i = ctx->next;
    (void)can_send_diag_request(ctx->can_bus, script->data[i], script->len[i]);
    uds_tester_print("Request", script->data[i], script->len[i]);
    ctx->waiting = true;
    ctx->sent_ms = timer_get_elapsed_ms();
    ctx->next++;
}
