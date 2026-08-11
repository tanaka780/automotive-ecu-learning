/* インクルードガード */
#ifndef CMD_H
#define CMD_H

#include <stdbool.h>  /* bool を使うために必要 */
#include <stddef.h>   /* size_t を使うために必要 */
#include "diag.h"     /* DtcRecord を参照するために必要 */

/* コマンド入力を格納するバッファサイズ("clear"+改行+余裕を持たせたサイズ) */
#define CMD_BUF_SIZE 32

/* 標準入力から1行読み込み、末尾の改行を取り除く。EOF等で読み込めない場合はfalseを返す */
bool cmd_read_line(char *buf, size_t bufsize);
/* 読み込んだ文字列を解釈する。"clear"は全DTCを、"clear <センサ名>"（speed/rpm/temp）は指定センサ1件だけをクリアする。
   それ以外（想定外のコマンド、"clear"に続く不正なセンサ名、余分なトークン）は認識できない旨を表示する。空文字列は何もしない */
void cmd_dispatch(const char *line, DtcRecord *dtc);
/* 受付条件（イグニッションOFF時のみ）を満たさないため、コマンドを受け付けなかったことを通知する */
void cmd_notify_rejected(void);

#endif /* CMD_H */
