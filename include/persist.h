/* インクルードガード */
#ifndef PERSIST_H
#define PERSIST_H

#include <stdbool.h>  /* bool を使うために必要 */
#include "diag.h"     /* DtcRecord を参照するために必要 */
#include "can_diag.h" /* CanDtcRecord を参照するために必要（Phase29） */

/* DTC保存に使うデフォルトのファイル名 */
#define PERSIST_DTC_FILENAME "dtc_data.txt"
/* 通信の途絶の記録の保存に使うファイル名。記録はそれを見つけたECUが持つので、ECUごとに分ける（Phase29） */
#define PERSIST_METER_DTC_FILENAME "meter_dtc.txt"
#define PERSIST_FAN_DTC_FILENAME   "fan_dtc.txt"

/* DtcRecordの内容を指定したファイルにテキスト形式で保存する。成功時true、失敗時false */
bool persist_save_dtc(const DtcRecord *dtc, const char *filename);
/* 指定したファイルからDtcRecordを読み込む。ファイルが無い・内容が壊れている場合はfalseを返し、dtcは変更しない */
bool persist_load_dtc(DtcRecord *dtc, const char *filename);
/* 通信の途絶の記録（メッセージ別の回数・状態区分）を指定したファイルに保存する（Phase29）。
   ecu_nameはログに出すECUの名前（"meter"/"fan"）。成功時true、失敗時false */
bool persist_save_can_dtc(const CanDtcRecord *rec, const char *filename, const char *ecu_name);
/* 指定したファイルから通信の途絶の記録を読み込む（Phase29）。ファイルが無い・内容が壊れている場合はfalseを返し、
   recは変更しない */
bool persist_load_can_dtc(CanDtcRecord *rec, const char *filename, const char *ecu_name);

#endif /* PERSIST_H */
