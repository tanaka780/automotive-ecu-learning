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
/* 読み込んだ文字列を解釈する。"clear"ならDTC記録をクリアし、それ以外の文字列(空文字列は除く)は認識できない旨を表示する */
void cmd_dispatch(const char *line, DtcRecord *dtc);

#endif /* CMD_H */
