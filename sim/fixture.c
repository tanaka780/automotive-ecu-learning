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

/* "IGNITION=ON/OFF" の1行を解釈し、有効ならvalueに入れてtrueを返す。
   IGNITION以外のキーと、ON/OFF以外の値（小文字・数値・空を含む）は無視する（MODEと同じく厳密に比較する） */
static bool fixture_ignition_line(const char *line, IgnitionState *value) {
    char key[FIXTURE_LINE_SIZE];
    char str_value[FIXTURE_LINE_SIZE];

    if (sscanf(line, "%63[^=]=%63s", key, str_value) != 2) {
        return false;
    }
    if (strcmp(key, "IGNITION") != 0) {
        return false;
    }

    if (strcmp(str_value, "ON") == 0) {
        *value = IGNITION_ON;
        return true;
    }
    if (strcmp(str_value, "OFF") == 0) {
        *value = IGNITION_OFF;
        return true;
    }
    return false;
}

/* fixture_applyとは別に、同じファイルをもう一度開いてIGNITION行だけを読む。
   fixture_applyはIGNITIONを未知のキーとして無視するため、センサ値の固定の動作は変わらない */
bool fixture_load_ignition(IgnitionState *state, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return false;   /* ファイルが無いことはfixture_applyがログに出しているため、ここでは出さない */
    }

    bool found = false;
    IgnitionState value = IGNITION_OFF;
    char line[FIXTURE_LINE_SIZE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (fixture_ignition_line(line, &value)) {
            found = true;   /* 後に書かれた有効な行で上書きされる（後勝ち） */
        }
    }
    /* 読み込みモードのfcloseのため、失敗しても既読データの正しさに影響しない。戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)fclose(fp);

    if (!found) {
        return false;
    }

    *state = value;
    log_print_leveled(LOG_INFO, "FIXTURE",
                      (value == IGNITION_ON) ? "Ignition fixed: ON" : "Ignition fixed: OFF");
    return true;
}

/* "TEMP@N=値" の1行を解釈し、有効ならfrom_sample・temperatureに入れてtrueを返す。
   キーは"TEMP@"の直後に整数だけが続くものに限り（%cで余分な文字が残っていないかを見る）、Nは1以上とする。
   値はTEMP=と同じく、数値に変換できない・値域外なら無視する */
static bool fixture_temp_step_line(const char *line, int *from_sample, uint8_t *temperature) {
    char key[FIXTURE_LINE_SIZE];
    char str_value[FIXTURE_LINE_SIZE];
    int  n;
    int  value;
    char extra;

    if (sscanf(line, "%63[^=]=%63s", key, str_value) != 2) {
        return false;
    }
    if ((sscanf(key, "TEMP@%d%c", &n, &extra) != 1) || (n < 1)) {
        return false;
    }
    if ((sscanf(str_value, "%d", &value) != 1) || (!validate_in_range(SENSOR_TEMP, value))) {
        return false;
    }

    *from_sample = n;
    *temperature = (uint8_t)value;
    return true;
}

/* fixture_load_ignitionと同じく、同じファイルをもう一度開いてTEMP@N行だけを読む。
   fixture_apply・fixture_load_ignitionはTEMP@Nを未知のキーとして無視するため、既存の動作は変わらない */
bool fixture_load_temp_steps(FixtureTempSteps *steps, const char *filename) {
    steps->count = 0U;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return false;   /* ファイルが無いことはfixture_applyがログに出しているため、ここでは出さない */
    }

    char line[FIXTURE_LINE_SIZE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        int     from_sample;
        uint8_t temperature;
        if (!fixture_temp_step_line(line, &from_sample, &temperature)) {
            continue;
        }

        /* 同じNが既にあれば上書きする（後勝ち）。無ければ空きがある間だけ足す */
        bool replaced = false;
        for (uint8_t i = 0U; i < steps->count; i++) {
            if (steps->from_sample[i] == from_sample) {
                steps->temperature[i] = temperature;
                replaced = true;
            }
        }
        if ((!replaced) && (steps->count < FIXTURE_TEMP_STEP_MAX)) {
            steps->from_sample[steps->count] = from_sample;
            steps->temperature[steps->count] = temperature;
            steps->count++;
        }
    }
    /* 読み込みモードのfcloseのため、失敗しても既読データの正しさに影響しない。戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)fclose(fp);

    if (steps->count == 0U) {
        return false;
    }

    char msg[40];   /* "Temp changes loaded: 16" が収まるサイズ */
    (void)snprintf(msg, sizeof(msg), "Temp changes loaded: %d", (int)steps->count);
    log_print_leveled(LOG_INFO, "FIXTURE", msg);
    return true;
}

/* 行は書かれた順のまま持っているので、sample_index以下のNのうち一番大きいものを探す */
bool fixture_temp_at(const FixtureTempSteps *steps, int sample_index, uint8_t *temperature) {
    bool found = false;
    int  best  = 0;
    for (uint8_t i = 0U; i < steps->count; i++) {
        if ((steps->from_sample[i] <= sample_index) && (steps->from_sample[i] > best)) {
            best         = steps->from_sample[i];
            *temperature = steps->temperature[i];
            found        = true;
        }
    }
    return found;
}
