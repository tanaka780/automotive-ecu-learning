#include <stdio.h>
#include "logger.h"

/* タグなしメッセージをそのまま1行出力する */
void log_print(const char *message) {
    printf("%s\n", message);
}

/* タグ付きメッセージを [TAG] message の形式で1行出力する */
void log_print_tagged(const char *tag, const char *message) {
    printf("[%s] %s\n", tag, message);
}
