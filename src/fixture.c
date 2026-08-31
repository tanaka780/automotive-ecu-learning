#include <stdio.h>
#include <string.h>
#include "fixture.h"
#include "logger.h"
#include "validate.h" /* validate_in_range で値域チェックするために必要 */

/* 固定値注入ファイルの1行（"MODE=FIXED"等）が収まるサイズ */
#define FIXTURE_LINE_SIZE 64

/* ファイルを読みながら組み立てる途中結果。
   mode_seen/mode_fixedでMODE行の有無と内容を、dataでSPEED/RPM/TEMPの反映結果を保持する */
typedef struct {
    bool mode_seen;
    bool mode_fixed;
    VehicleSensorData data;
} FixtureValues;

/* "KEY=VALUE" の1行を解釈する。
   MODEキーは文字列として比較し、SPEED/RPM/TEMPは数値に変換できた場合だけ反映する。
   未知のキー・値欠落・数値に変換できない行・値域外の値は無視する（config.cのapply_lineと同じ考え方）。
   値域チェックはconfig.cと同じvalidate_in_range（validate.h）を使い、int型のまま検証してから
   uint8_t/uint16_tへキャストする */
/* config.cにも同名のstatic関数があり、識別子が別ファイルで重複しないよう関数名にモジュール名を付ける（MISRA 5.9） */
static void fixture_apply_line(FixtureValues *fv, const char *line) {
    char key[FIXTURE_LINE_SIZE];
    char str_value[FIXTURE_LINE_SIZE];
    int  int_value;

    if (sscanf(line, "%63[^=]=%63s", key, str_value) != 2) {
        return;
    }

    if (strcmp(key, "MODE") == 0) {
        fv->mode_seen = true;
        fv->mode_fixed = (strcmp(str_value, "FIXED") == 0);
        return;
    }

    if (sscanf(str_value, "%d", &int_value) != 1) {
        return;   /* 数値に変換できない値は無視する */
    }

    if (strcmp(key, "SPEED") == 0) {
        if (validate_in_range(SENSOR_SPEED, int_value)) {
            fv->data.speed = (uint8_t)int_value;
        }
    } else if (strcmp(key, "RPM") == 0) {
        if (validate_in_range(SENSOR_RPM, int_value)) {
            fv->data.rpm = (uint16_t)int_value;
        }
    } else if (strcmp(key, "TEMP") == 0) {
        if (validate_in_range(SENSOR_TEMP, int_value)) {
            fv->data.temperature = (uint8_t)int_value;
        }
    } else {
        /* 未知のキーは無視する */
    }
}

bool fixture_apply(VehicleSensorData *data, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        log_print_leveled(LOG_INFO, "FIXTURE", "No fixture file (using random)");
        return false;
    }

    FixtureValues fv;
    fv.mode_seen  = false;
    fv.mode_fixed = false;
    fv.data       = *data;   /* SPEED/RPM/TEMPの指定が無いキーは呼び出し時点の値のまま残す */

    char line[FIXTURE_LINE_SIZE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        fixture_apply_line(&fv, line);
    }
    /* 読み込みモードのfcloseのため、失敗しても既読データの正しさに影響しない。戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)fclose(fp);

    if (fv.mode_seen && fv.mode_fixed) {
        *data = fv.data;
        log_print_leveled(LOG_INFO, "FIXTURE", "Loaded fixture file (FIXED mode)");
        return true;
    }

    log_print_leveled(LOG_INFO, "FIXTURE", "Loaded fixture file (RANDOM mode)");
    return false;
}
