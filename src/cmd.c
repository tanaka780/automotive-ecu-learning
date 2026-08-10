#include <stdio.h>
#include <string.h>
#include "cmd.h"
#include "logger.h"

/* コマンド内のトークン（"clear"/"speed"等）1つ分のバッファサイズ。sscanfの%15sと対応させる */
#define CMD_TOKEN_SIZE 16

/* 標準入力から1行読み込み、末尾の改行を取り除く。EOF等で読み込めない場合はfalseを返す */
bool cmd_read_line(char *buf, size_t bufsize) {
    if (fgets(buf, (int)bufsize, stdin) == NULL) {
        return false;
    }

    /* fgets は改行を含めて読み込むため、末尾の改行を取り除く */
    buf[strcspn(buf, "\n")] = '\0';

    return true;
}

/* "speed"/"rpm"/"temp"をSensorIdに変換する。該当しない文字列はfalseを返す */
static bool parse_sensor_name(const char *name, SensorId *sensor) {
    if (strcmp(name, "speed") == 0) {
        *sensor = SENSOR_SPEED;
        return true;
    }
    if (strcmp(name, "rpm") == 0) {
        *sensor = SENSOR_RPM;
        return true;
    }
    if (strcmp(name, "temp") == 0) {
        *sensor = SENSOR_TEMP;
        return true;
    }
    return false;
}

/* 読み込んだ文字列を解釈する。"clear"は全DTCを、"clear <センサ名>"は指定センサ1件だけをクリアする。
   それ以外（想定外のコマンド、"clear"に続く不正なセンサ名、余分なトークン）は認識できない旨を表示する。空文字列は何もしない */
void cmd_dispatch(const char *line, DtcRecord *dtc) {
    if (strcmp(line, "") == 0) {
        return;
    }

    char command[CMD_TOKEN_SIZE];
    char target[CMD_TOKEN_SIZE];
    char extra[CMD_TOKEN_SIZE];
    int n = sscanf(line, "%15s %15s %15s", command, target, extra);

    /* nが1未満(空白のみの入力)の場合はcommandが未初期化のため、strcmpより先に判定する */
    if (n < 1 || strcmp(command, "clear") != 0) {
        log_print_tagged("CMD", "Unknown command");
        return;
    }

    if (n == 1) {
        diag_clear(dtc);
        return;
    }

    if (n == 2) {
        SensorId sensor;
        if (parse_sensor_name(target, &sensor)) {
            diag_clear_sensor(dtc, sensor);
        } else {
            log_print_tagged("CMD", "Unknown target for clear (use speed/rpm/temp)");
        }
        return;
    }

    /* n == 3: "clear <対象> <余分なトークン>" は想定外の入力として扱う */
    log_print_tagged("CMD", "Unknown command");
}
