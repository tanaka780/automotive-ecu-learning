/* インクルードガード */
#ifndef CAN_FAULT_H
#define CAN_FAULT_H

#include <stdint.h>
#include <stdbool.h>
#include "can.h"        /* CanMessageId, CanBus を参照するために必要 */
#include "sensor.h"      /* VehicleSensorData を参照するために必要 */
#include "faultmgr.h"    /* FaultManager を参照するために必要 */

/* CAN Fault Injection設定ファイルのデフォルトのファイル名 */
#define CAN_FAULT_FILENAME "can_fault.txt"

/* 注入の種類。NONEは注入なし（MODE=NORMAL相当）。CORRUPTはENGINE_STATUSのみ対応する
   （FAULT_STATUSはビットフラグのみでcan.cにInvalid Data判定ロジックが無いため、Phase22拡張バックログで
   MODE=CORRUPT+TARGET=FAULT_STATUSは無効な組み合わせと決定した） */
typedef enum {
    CAN_FAULT_MODE_NONE = 0,
    CAN_FAULT_MODE_DROP,
    CAN_FAULT_MODE_CORRUPT
} CanFaultMode;

/* 意図的にDrop/Corruptする対象メッセージ・期間の設定（Phase22、Corruptは拡張バックログ）。
   can.cのフレーム構造・送受信ロジックは変更せず、送信をラップして意図的にスキップ/改ざんすることで
   Timeout/Invalid Dataを再現する。fixture.txtと同じKEY=VALUE形式のファイルから読み込む */
typedef struct {
    CanFaultMode mode;      /* 注入の種類 */
    CanMessageId target;    /* 対象メッセージ */
    uint32_t     start_ms;  /* プログラム起動からの経過時間がこれ以上で注入開始 */
    uint32_t     end_ms;    /* プログラム起動からの経過時間がこれを超えたら通常動作に戻る（Recovery確認用） */
    uint32_t     base_ms;   /* 基準時刻（can_fault_init呼び出し時点のtimer_get_elapsed_ms）。
                                timer_get_elapsed_msはシステム起動からの経過時間を返すため
                                （プログラム起動からではない）、start_ms/end_msと比較する前に
                                この基準値との差分を取り、プログラム起動からの経過時間に変換する */
} CanFaultConfig;

/* cfgを初期値（mode=NONE、注入なし）にする。base_msをこの呼び出し時点の
   timer_get_elapsed_msで記録するため、main()の起動直後に呼ぶ想定 */
void can_fault_init(CanFaultConfig *cfg);

/* 指定したファイルをKEY=VALUE形式（MODE=DROP/CORRUPT/NORMAL, TARGET=ENGINE_STATUS/FAULT_STATUS,
   START_MS=, END_MS=）で読み込む。MODE=DROPまたはCORRUPTかつTARGETが指定されていればcfgに反映し
   trueを返す。ただしMODE=CORRUPT+TARGET=FAULT_STATUSは無効な組み合わせとして注入なし扱いにする。
   ファイルが無い場合・MODE=NORMALの場合・MODE/TARGET行が無い/不正な場合はcfgを初期値のまま
   （注入なし）にしてfalseを返す。START_MSとEND_MSの大小関係はチェックしない
   （Phase12で決めた「閾値同士の大小関係はチェックしない」方針を踏襲） */
bool can_fault_load(CanFaultConfig *cfg, const char *filename);

/* elapsed_ms（プログラム起動からの経過時間、呼び出し側がbase_msとの差分を取って渡す）の時点で
   msgをDropすべきかを判定する（cfg->mode==DROP && cfg->target==msg && start_ms<=elapsed_ms<=end_ms）。
   timer.cを直接呼ばない純粋な判定関数にすることで、テストから任意の経過時間を指定できるようにする */
bool can_fault_is_dropped(const CanFaultConfig *cfg, CanMessageId msg, uint32_t elapsed_ms);

/* elapsed_msの時点でmsgをCorrupt（値域外データに差し替え）すべきかを判定する
   （cfg->mode==CORRUPT && cfg->target==msg && start_ms<=elapsed_ms<=end_ms）。
   is_droppedと対になる純粋な判定関数（Phase22拡張バックログ） */
bool can_fault_is_corrupted(const CanFaultConfig *cfg, CanMessageId msg, uint32_t elapsed_ms);

/* エンジンECU役の送信をラップする：現在時刻がDrop対象ならcan_send_engine_statusを呼ばず
   意図的に送信しない（Timeoutを再現）。Corrupt対象なら、渡されたdataとは別に値域外の値を持つ
   一時コピーを作ってcan_send_engine_statusに渡す（Invalid Dataを再現、Phase22拡張バックログ）。
   実際のセンサ値・呼び出し元のdataは変更しないため、diag.c/faultmgr.c等の物理センサ診断には
   影響しない。どちらでもなければそのままdataを渡してcan_send_engine_statusを呼ぶ */
void can_fault_send_engine_status(const CanFaultConfig *cfg, CanBus *bus, const VehicleSensorData *data);

/* エンジンECU役の送信をラップする（警告灯データ版） */
void can_fault_send_fault_status(const CanFaultConfig *cfg, CanBus *bus, const FaultManager *fault_mgr);

#endif /* CAN_FAULT_H */
