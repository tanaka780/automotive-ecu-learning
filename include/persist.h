/* インクルードガード */
#ifndef PERSIST_H
#define PERSIST_H

#include <stdbool.h>  /* bool を使うために必要 */
#include "diag.h"     /* DtcRecord を参照するために必要 */

/* DTC保存に使うデフォルトのファイル名 */
#define PERSIST_DTC_FILENAME "dtc_data.txt"

/* DtcRecordの内容を指定したファイルにテキスト形式で保存する。成功時true、失敗時false */
bool persist_save_dtc(const DtcRecord *dtc, const char *filename);
/* 指定したファイルからDtcRecordを読み込む。ファイルが無い・内容が壊れている場合はfalseを返し、dtcは変更しない */
bool persist_load_dtc(DtcRecord *dtc, const char *filename);

#endif /* PERSIST_H */
