#include <stdio.h>
#include <string.h>
#include "cmd.h"
#include "logger.h"

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

/* 読み込んだ文字列を解釈する。"clear"は3つのECUの記録を全部、"clear <センサ名>"はエンジン監視ECUの指定センサの記録だけをクリアする。
   前後の空白はトリムせず、単一スペース区切りの完全一致のみを受け付ける（例:" clear"や"clear "は
   Unknown commandになる）。それ以外（想定外のコマンド、"clear"に続く不正なセンサ名、余分なトークン）は
   認識できない旨を表示する。空文字列は何もしない */
void cmd_dispatch(const char *line, DtcRecord *dtc, CanDtcRecord *meter_dtc, CanDtcRecord *fan_dtc) {
    if (strcmp(line, "") == 0) {
        return;
    }

    if (strcmp(line, "clear") == 0) {
        diag_clear(dtc);
        /* 対象を指定しない"clear"は、診断機で全部のECUの記録を消す操作に当たるとし、メーターECU役・ファン制御ECU役の
           通信の途絶の記録も消す（Phase29。記録が保存されるようになり、消す方法が要るため）。全部のECUを消すと決めるのは
           ここなので、ログもここで1行出す（[CAN-DTC]等のタグは記録の表示が使うため使わない） */
        can_diag_init(meter_dtc);
        can_diag_init(fan_dtc);
        log_print_leveled(LOG_INFO, "CMD", "Cleared records of all ECUs");
        return;
    }

    if (strncmp(line, "clear ", 6) == 0) {
        /* ポインタ演算(line + 6)ではなく配列添字表記にする（MISRA 18.4） */
        const char *target = &line[6];

        /* targetの中に空白が残っている場合は"clear <対象> <余分なトークン>"とみなす */
        if (strchr(target, ' ') != NULL) {
            log_print_leveled(LOG_INFO, "CMD", "Unknown command");
            return;
        }

        SensorId sensor;
        if (parse_sensor_name(target, &sensor)) {
            diag_clear_sensor(dtc, sensor);
        } else {
            log_print_leveled(LOG_INFO, "CMD", "Unknown target for clear (use speed/rpm/temp)");
        }
        return;
    }

    log_print_leveled(LOG_INFO, "CMD", "Unknown command");
}

/* 受付条件（イグニッションOFF時のみ）を満たさないため、コマンドを受け付けなかったことを通知する */
void cmd_notify_rejected(void) {
    log_print_leveled(LOG_INFO, "CMD", "Not accepted (ignition ON)");
}
