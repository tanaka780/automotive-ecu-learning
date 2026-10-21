#include <stdio.h>
#include <stdint.h>   /* uint8_t / uint16_t を使うために必要 */
#include "persist.h"
#include "logger.h"

/* 保存する値の個数: 記録4種×(count, status)=8 + フリーズフレーム(captured, trigger, speed, rpm, temp)=5。
   Phase28で記録を4種にし、11個から13個に変えた。前の形（11個）のファイルは個数が合わないので、
   壊れたデータと同じく読み込まない（前の形の水温の回数は、オーバーヒートと水温センサの故障が混ざっていて分けられないため） */
#define PERSIST_VALUE_COUNT 13

/* 通信の途絶の記録の値の個数: メッセージ3種×(count, status)=6（Phase29）。受信監視状態（途絶の判断の途中経過）は
   保存しない。起動し直したら、途絶の判断をやり直すため */
#define PERSIST_CAN_VALUE_COUNT 6
/* 下の書式・sscanfはメッセージ3種の前提で書いているので、メッセージの種類が変わったらコンパイルで気づけるようにする */
_Static_assert(CAN_MSG_COUNT == 3, "persist.cの通信の途絶の記録の書式は、メッセージ3種の前提");

/* ログの1行（"Saved meter DTC data is corrupted, ignoring"等）が収まるサイズ */
#define PERSIST_LOG_SIZE 64

/* テキスト化したDtcRecordが収まるサイズ（1値最大5桁+改行×13行に余裕を持たせる。他モジュールの128との慣習を保つ） */
#define PERSIST_BUF_SIZE 128

/* DtcRecordの内容を、固定順の整数を改行区切りにしたテキストへ組み立てる（ファイルは扱わない）。
   並びは記録の種類の順（DtcId）に回数・状態区分、その後にフリーズフレーム */
static void dtc_to_text(const DtcRecord *dtc, char *buf, size_t bufsize) {
    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)snprintf(buf, bufsize,
             "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n",
             (int)dtc->entries[DTC_ID_SPEED].count,
             (int)dtc->entries[DTC_ID_SPEED].status,
             (int)dtc->entries[DTC_ID_RPM].count,
             (int)dtc->entries[DTC_ID_RPM].status,
             (int)dtc->entries[DTC_ID_TEMP_SENSOR].count,
             (int)dtc->entries[DTC_ID_TEMP_SENSOR].status,
             (int)dtc->entries[DTC_ID_OVERHEAT].count,
             (int)dtc->entries[DTC_ID_OVERHEAT].status,
             (int)dtc->freeze_frame.captured,
             (int)dtc->freeze_frame.trigger,
             (int)dtc->freeze_frame.data.speed,
             (int)dtc->freeze_frame.data.rpm,
             (int)dtc->freeze_frame.data.temperature);
}

/* 固定順の整数テキストをDtcRecordへ変換する（ファイルは扱わない）。
   個数が足りない場合は壊れたデータとみなしfalseを返し、dtcは変更しない。
   previous_bad（前回の異常の有無、エッジ検出用の一時情報）は保存対象に含めないため、読み込み時は常に異常なしから再開する */
static bool text_to_dtc(const char *text, DtcRecord *dtc) {
    int values[PERSIST_VALUE_COUNT];

    int n = sscanf(text, "%d %d %d %d %d %d %d %d %d %d %d %d %d",
                   &values[0], &values[1], &values[2], &values[3],
                   &values[4], &values[5], &values[6], &values[7],
                   &values[8], &values[9], &values[10], &values[11],
                   &values[12]);

    if (n != PERSIST_VALUE_COUNT) {
        return false;
    }

    /* DTC_ID_COUNTはenum定数のため、int（i）との比較にはキャストが必要（MISRA 10.4） */
    for (int i = 0; i < (int)DTC_ID_COUNT; i++) {
        dtc->entries[i].id     = (DtcId)i;
        dtc->entries[i].count  = (uint8_t)values[i * 2];
        dtc->entries[i].status = (DtcStatus)values[(i * 2) + 1];
        dtc->previous_bad[i]   = false;
    }

    dtc->freeze_frame.captured         = (bool)values[8];
    dtc->freeze_frame.trigger          = (DtcId)values[9];
    dtc->freeze_frame.data.speed       = (uint8_t)values[10];
    dtc->freeze_frame.data.rpm         = (uint16_t)values[11];
    dtc->freeze_frame.data.temperature = (uint8_t)values[12];

    return true;
}

/* テキストをファイルへ書き込む（渡された文字列の中身は解釈せず、書き込むだけ）。
   書き込みモードのfcloseはバッファのフラッシュを伴うため、失敗は実際のデータ損失を意味する。
   fputs/fcloseの戻り値を両方確認し、どちらかが失敗したらfalseを返す（MISRA Directive 4.7） */
static bool write_text_to_file(const char *filename, const char *text) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return false;
    }

    bool write_ok = (fputs(text, fp) != EOF);
    bool close_ok = (fclose(fp) == 0);
    return write_ok && close_ok;
}

/* ファイルの内容をそのままバッファへ読み込む（内容の意味は解釈しない） */
static bool read_text_from_file(const char *filename, char *buf, size_t bufsize) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return false;   /* 初回起動などでファイルが無い場合はここに来る */
    }

    /* bufsizeはsize_t（符号なし）のため、引く数値も符号なしリテラルにする（MISRA 10.4） */
    size_t len = fread(buf, 1, bufsize - 1U, fp);
    buf[len] = '\0';
    /* 読み込みモードのfcloseのため、失敗しても既読データの正しさに影響しない。戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)fclose(fp);
    return true;
}

/* DtcRecordを指定したファイルに保存する */
bool persist_save_dtc(const DtcRecord *dtc, const char *filename) {
    char text[PERSIST_BUF_SIZE];
    dtc_to_text(dtc, text, sizeof(text));

    if (!write_text_to_file(filename, text)) {
        log_print_leveled(LOG_ERROR, "PERSIST", "Failed to save DTC data");
        return false;
    }

    log_print_leveled(LOG_INFO, "PERSIST", "Saved DTC data");
    return true;
}

/* 指定したファイルからDtcRecordを読み込む */
bool persist_load_dtc(DtcRecord *dtc, const char *filename) {
    char text[PERSIST_BUF_SIZE];

    /* ファイルが無い場合(初回起動)と、ファイルはあるが内容が壊れている場合を区別して表示する */
    if (!read_text_from_file(filename, text, sizeof(text))) {
        log_print_leveled(LOG_INFO, "PERSIST", "No saved data (first run)");
        return false;
    }

    if (!text_to_dtc(text, dtc)) {
        log_print_leveled(LOG_ERROR, "PERSIST", "Saved data is corrupted, ignoring");
        return false;
    }

    log_print_leveled(LOG_INFO, "PERSIST", "Loaded saved DTC data");
    return true;
}

/* ログの文の間にどのECUの記録かを入れる（例："Saved " + "meter" + " DTC data"）。タグは[PERSIST]のまま
   （保存のログはpersist.cが出す。[CAN-DTC]等は記録の表示のタグ）。書式の文字列は引数で受け取らず、ここで固定する */
static void persist_log_can(LogLevel level, const char *before, const char *ecu_name, const char *after) {
    char line[PERSIST_LOG_SIZE];
    /* 表示幅は固定の文とECUの名前（"meter"/"fan"）で決まり切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)snprintf(line, sizeof(line), "%s%s%s", before, ecu_name, after);
    log_print_leveled(level, "PERSIST", line);
}

/* 通信の途絶の記録を、メッセージの順（CanMessageId）に回数・状態区分を並べた改行区切りのテキストにして保存する（Phase29） */
bool persist_save_can_dtc(const CanDtcRecord *rec, const char *filename, const char *ecu_name) {
    char text[PERSIST_BUF_SIZE];
    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)snprintf(text, sizeof(text), "%d\n%d\n%d\n%d\n%d\n%d\n",
             (int)rec->count[0], (int)rec->status[0],
             (int)rec->count[1], (int)rec->status[1],
             (int)rec->count[2], (int)rec->status[2]);

    if (!write_text_to_file(filename, text)) {
        persist_log_can(LOG_ERROR, "Failed to save ", ecu_name, " DTC data");
        return false;
    }

    persist_log_can(LOG_INFO, "Saved ", ecu_name, " DTC data");
    return true;
}

/* 通信の途絶の記録を読み込む（Phase29）。値が6個そろわなければ壊れたデータとみなし、recは変更しない */
bool persist_load_can_dtc(CanDtcRecord *rec, const char *filename, const char *ecu_name) {
    char text[PERSIST_BUF_SIZE];

    if (!read_text_from_file(filename, text, sizeof(text))) {
        persist_log_can(LOG_INFO, "No saved ", ecu_name, " DTC data (first run)");
        return false;
    }

    int values[PERSIST_CAN_VALUE_COUNT];
    int n = sscanf(text, "%d %d %d %d %d %d",
                   &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]);
    if (n != PERSIST_CAN_VALUE_COUNT) {
        persist_log_can(LOG_ERROR, "Saved ", ecu_name, " DTC data is corrupted, ignoring");
        return false;
    }

    /* CAN_MSG_COUNTはenum定数のため、int（i）との比較にはキャストが必要（MISRA 10.4） */
    for (int i = 0; i < (int)CAN_MSG_COUNT; i++) {
        rec->count[i]  = (uint8_t)values[i * 2];
        rec->status[i] = (DtcStatus)values[(i * 2) + 1];
    }

    persist_log_can(LOG_INFO, "Loaded saved ", ecu_name, " DTC data");
    return true;
}
