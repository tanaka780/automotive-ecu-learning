#include <stdio.h>
#include "logger.h"

/* log_print_leveled の現在の表示閾値。デフォルトはLOG_INFO（全て表示） */
static LogLevel g_level = LOG_INFO;

/* タグなしメッセージをそのまま1行出力する */
void log_print(const char *message) {
    printf("%s\n", message);
}

/* タグ付きメッセージを [TAG] message の形式で1行出力する */
void log_print_tagged(const char *tag, const char *message) {
    printf("[%s] %s\n", tag, message);
}

/* levelが現在の表示閾値未満なら何もせず、それ以外はタグ付きメッセージとして出力する */
void log_print_leveled(LogLevel level, const char *tag, const char *message) {
    if (level < g_level) {
        return;
    }
    log_print_tagged(tag, message);
}

/* log_print_leveled の表示閾値を設定する */
void logger_set_level(LogLevel level) {
    g_level = level;
}
