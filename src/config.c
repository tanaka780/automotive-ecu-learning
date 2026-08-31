#include <stdio.h>
#include <string.h>
#include "config.h"
#include "alert.h"    /* ALERT_* マクロ（デフォルト値）を参照するために必要 */
#include "status.h"   /* STATUS_* マクロ（デフォルト値）を参照するために必要 */
#include "logger.h"
#include "validate.h" /* validate_in_range/validate_log_level で値域チェックするために必要 */

/* 設定ファイルの1行（"STATUS_SPEED_WARN=100"等）が収まるサイズ */
#define CONFIG_LINE_SIZE 64

/* 設定値を、alert.h/status.hの現行マクロ値で初期化する */
void config_init(ConfigData *config) {
    config->alert_speed_max   = ALERT_SPEED_MAX;
    config->alert_rpm_max     = ALERT_RPM_MAX;
    config->alert_temp_max    = ALERT_TEMP_MAX;
    config->status_speed_warn = STATUS_SPEED_WARN;
    config->status_speed_crit = STATUS_SPEED_CRIT;
    config->status_rpm_warn   = STATUS_RPM_WARN;
    config->status_rpm_crit   = STATUS_RPM_CRIT;
    config->status_temp_warn  = STATUS_TEMP_WARN;
    config->status_temp_crit  = STATUS_TEMP_CRIT;
    config->log_level         = LOG_INFO;
}

/* "KEY=VALUE" の1行を解釈し、該当するキーがあればconfigの対応フィールドを上書きする。
   未知のキー・数値に変換できない行・値域外の値は無視する（正常系・異常系の細かい扱いはtest_config.cで検討する）。
   値域チェックはvalidate_in_range/validate_log_level（validate.h）が持つ物理的な値域を基準にする。
   int型のvalueのまま検証してからuint8_t/uint16_tへキャストすることで、キャストによる
   オーバーフロー（例: 999が231に化ける等）が起きる前に不正な値をはじく */
/* fixture.cにも同名のstatic関数があり、識別子が別ファイルで重複しないよう関数名にモジュール名を付ける（MISRA 5.9） */
static void config_apply_line(ConfigData *config, const char *line) {
    char key[CONFIG_LINE_SIZE];
    int value;

    if (sscanf(line, "%63[^=]=%d", key, &value) != 2) {
        return;
    }

    if (strcmp(key, "ALERT_SPEED_MAX") == 0) {
        if (validate_in_range(SENSOR_SPEED, value)) {
            config->alert_speed_max = (uint8_t)value;
        }
    } else if (strcmp(key, "ALERT_RPM_MAX") == 0) {
        if (validate_in_range(SENSOR_RPM, value)) {
            config->alert_rpm_max = (uint16_t)value;
        }
    } else if (strcmp(key, "ALERT_TEMP_MAX") == 0) {
        if (validate_in_range(SENSOR_TEMP, value)) {
            config->alert_temp_max = (uint8_t)value;
        }
    } else if (strcmp(key, "STATUS_SPEED_WARN") == 0) {
        if (validate_in_range(SENSOR_SPEED, value)) {
            config->status_speed_warn = (uint8_t)value;
        }
    } else if (strcmp(key, "STATUS_SPEED_CRIT") == 0) {
        if (validate_in_range(SENSOR_SPEED, value)) {
            config->status_speed_crit = (uint8_t)value;
        }
    } else if (strcmp(key, "STATUS_RPM_WARN") == 0) {
        if (validate_in_range(SENSOR_RPM, value)) {
            config->status_rpm_warn = (uint16_t)value;
        }
    } else if (strcmp(key, "STATUS_RPM_CRIT") == 0) {
        if (validate_in_range(SENSOR_RPM, value)) {
            config->status_rpm_crit = (uint16_t)value;
        }
    } else if (strcmp(key, "STATUS_TEMP_WARN") == 0) {
        if (validate_in_range(SENSOR_TEMP, value)) {
            config->status_temp_warn = (uint8_t)value;
        }
    } else if (strcmp(key, "STATUS_TEMP_CRIT") == 0) {
        if (validate_in_range(SENSOR_TEMP, value)) {
            config->status_temp_crit = (uint8_t)value;
        }
    } else if (strcmp(key, "LOG_LEVEL") == 0) {
        /* 0=LOG_INFO, 1=LOG_WARNING, 2=LOG_ERROR（logger.hのLogLevel定義順と一致させる） */
        if (validate_log_level(value)) {
            config->log_level = (LogLevel)value;
        }
    } else {
        /* 未知のキーは無視する */
    }
}

/* 指定したファイルからKEY=VALUE形式で設定値を読み込む。
   ここの2つのログはlog_level自体が確定する前（本関数の実行中）に出るため、
   あえてlog_print_taggedのまま（レベル制御の対象外）にしている */
bool config_load(ConfigData *config, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        log_print_tagged("CONFIG", "No config file (using defaults)");
        return false;
    }

    char line[CONFIG_LINE_SIZE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        config_apply_line(config, line);
    }
    /* 読み込みモードのfcloseのため、失敗しても既読データの正しさに影響しない。戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)fclose(fp);

    log_print_tagged("CONFIG", "Loaded config file");
    return true;
}

/* 設定値を1行でコンソールに出力する（確認用） */
void config_print(const ConfigData *config) {
    char line[128];   /* "AlertMax(...) StatusWarn(...) StatusCrit(...)" が収まるサイズ */
    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)snprintf(line, sizeof(line),
             "AlertMax(speed=%d,rpm=%d,temp=%d) StatusWarn(speed=%d,rpm=%d,temp=%d) StatusCrit(speed=%d,rpm=%d,temp=%d)",
             (int)config->alert_speed_max, (int)config->alert_rpm_max, (int)config->alert_temp_max,
             (int)config->status_speed_warn, (int)config->status_rpm_warn, (int)config->status_temp_warn,
             (int)config->status_speed_crit, (int)config->status_rpm_crit, (int)config->status_temp_crit);
    log_print_leveled(LOG_INFO, "CONFIG", line);
}
