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

/* 読み込んだ文字列を解釈する。"clear"ならDTC記録をクリアし、それ以外の文字列(空文字列は除く)は認識できない旨を表示する */
void cmd_dispatch(const char *line, DtcRecord *dtc) {
    if (strcmp(line, "clear") == 0) {
        diag_clear(dtc);
    } else if (strcmp(line, "") != 0) {
        log_print_tagged("CMD", "Unknown command");
    }
}
