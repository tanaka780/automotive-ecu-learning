/* インクルードガード */
#ifndef LOGGER_H
#define LOGGER_H

/* タグなしで1行出力する（sensor.c / stats.c など、値そのものを表示するモジュール用） */
void log_print(const char *message);

/* タグ付き（[TAG] message）で1行出力する（status.c / alert.c / diag.c など、診断イベントを表示するモジュール用） */
void log_print_tagged(const char *tag, const char *message);

#endif /* LOGGER_H */
