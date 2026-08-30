#include <stdio.h>
#include <stdint.h>   /* uint8_t / uint16_t を使うために必要 */
#include "persist.h"
#include "logger.h"

/* 保存する値の個数: センサ3種×(count, status)=6 + フリーズフレーム(captured, trigger, speed, rpm, temp)=5 */
#define PERSIST_VALUE_COUNT 11

/* テキスト化したDtcRecordが収まるサイズ（1値最大4桁+改行×11行に余裕を持たせる。他モジュールの128との慣習を保つ） */
#define PERSIST_BUF_SIZE 128

/* DtcRecordの内容を、固定順の整数を改行区切りにしたテキストへ組み立てる（ファイルは扱わない） */
static void dtc_to_text(const DtcRecord *dtc, char *buf, size_t bufsize) {
    snprintf(buf, bufsize,
             "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n",
             (int)dtc->entries[SENSOR_SPEED].count,
             (int)dtc->entries[SENSOR_SPEED].status,
             (int)dtc->entries[SENSOR_RPM].count,
             (int)dtc->entries[SENSOR_RPM].status,
             (int)dtc->entries[SENSOR_TEMP].count,
             (int)dtc->entries[SENSOR_TEMP].status,
             (int)dtc->freeze_frame.captured,
             (int)dtc->freeze_frame.trigger_sensor,
             (int)dtc->freeze_frame.data.speed,
             (int)dtc->freeze_frame.data.rpm,
             (int)dtc->freeze_frame.data.temperature);
}

/* 固定順の整数テキストをDtcRecordへ変換する（ファイルは扱わない）。
   個数が足りない場合は壊れたデータとみなしfalseを返し、dtcは変更しない。
   previous（前回状態、エッジ検出用の一時情報）は保存対象に含めないため、読み込み時は常にNORMALから再開する */
static bool text_to_dtc(const char *text, DtcRecord *dtc) {
    int values[PERSIST_VALUE_COUNT];

    int n = sscanf(text, "%d %d %d %d %d %d %d %d %d %d %d",
                   &values[0], &values[1], &values[2], &values[3],
                   &values[4], &values[5], &values[6], &values[7],
                   &values[8], &values[9], &values[10]);

    if (n != PERSIST_VALUE_COUNT) {
        return false;
    }

    dtc->entries[SENSOR_SPEED].sensor = SENSOR_SPEED;
    dtc->entries[SENSOR_SPEED].count  = (uint8_t)values[0];
    dtc->entries[SENSOR_SPEED].status = (DtcStatus)values[1];

    dtc->entries[SENSOR_RPM].sensor = SENSOR_RPM;
    dtc->entries[SENSOR_RPM].count  = (uint8_t)values[2];
    dtc->entries[SENSOR_RPM].status = (DtcStatus)values[3];

    dtc->entries[SENSOR_TEMP].sensor = SENSOR_TEMP;
    dtc->entries[SENSOR_TEMP].count  = (uint8_t)values[4];
    dtc->entries[SENSOR_TEMP].status = (DtcStatus)values[5];

    dtc->freeze_frame.captured         = (bool)values[6];
    dtc->freeze_frame.trigger_sensor   = (SensorId)values[7];
    dtc->freeze_frame.data.speed       = (uint8_t)values[8];
    dtc->freeze_frame.data.rpm         = (uint16_t)values[9];
    dtc->freeze_frame.data.temperature = (uint8_t)values[10];

    /* SENSOR_COUNTはenum定数のため、int（i）との比較にはキャストが必要（MISRA 10.4） */
    for (int i = 0; i < (int)SENSOR_COUNT; i++) {
        dtc->previous.levels[i] = LEVEL_NORMAL;
    }

    return true;
}

/* テキストをファイルへ書き込む（渡された文字列の中身は解釈せず、書き込むだけ） */
static bool write_text_to_file(const char *filename, const char *text) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return false;
    }

    fputs(text, fp);
    fclose(fp);
    return true;
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
    fclose(fp);
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
