/* インクルードガード */
#ifndef LOGGER_H
#define LOGGER_H

/* ログメッセージの重要度（値が大きいほど重要度が高い） */
typedef enum {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

/* タグなしで1行出力する（sensor.c / stats.c など、値そのものを表示するモジュール用） */
void log_print(const char *message);

/* タグ付き（[TAG] message）で1行出力する（status.c / alert.c / diag.c など、診断イベントを表示するモジュール用） */
void log_print_tagged(const char *tag, const char *message);

/* レベル・タグ・メッセージを受け取り、現在の表示閾値（logger_set_level参照）以上のときだけ [TAG] message の形式で出力する */
void log_print_leveled(LogLevel level, const char *tag, const char *message);

/* log_print_leveled の表示閾値を設定する。このレベル未満のlevelを渡した呼び出しは出力されなくなる */
void logger_set_level(LogLevel level);

#endif /* LOGGER_H */
