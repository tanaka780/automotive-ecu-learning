/* インクルードガード */
#ifndef UDS_TESTER_H
#define UDS_TESTER_H

#include <stdbool.h>   /* bool を使うために必要 */
#include <stdint.h>    /* uint8_t / uint32_t を使うために必要 */
#include "can.h"       /* CanBus / CAN_DIAG_SF_MAX_LEN を参照するために必要 */

/* スキャンツール役（PC上の検証専用、Phase30）。uds_request.txtに書いた診断の要求を、指定したサンプルの後に
   1つずつCANで送り、受け取った応答を表示する。実際のスキャンツールと同じく、1つ送ったら応答が返るか
   待ち時間（P2）が切れるまで次を送らない（busは最新の1フレームしか持たないため、続けて送ると前の要求が消える）。
   応答が期待通りかの判定はしない。表示を読んで判定するのはPythonのシナリオ検証の役割 */

/* 要求を書くファイルのデフォルトのファイル名。無ければ何も送らず、何も表示しない */
#define UDS_TESTER_FILENAME "uds_request.txt"

/* 覚えておける要求の数 */
#define UDS_TESTER_REQUEST_MAX 16U

/* 応答を待つ時間 [ms]。UDSで要求から応答までの目安とされる50ms（P2）。過ぎたら「応答なし」とする */
#define UDS_TESTER_P2_MS 50U

/* uds_request.txtから読んだ要求の一覧（REQ@N=19 01 FF：N秒目のサンプルの後に送る）。書いた順に送る */
typedef struct {
    uint8_t count;
    int     at_sample[UDS_TESTER_REQUEST_MAX];                     /* N（1始まりのサンプル番号） */
    uint8_t len[UDS_TESTER_REQUEST_MAX];
    uint8_t data[UDS_TESTER_REQUEST_MAX][CAN_DIAG_SF_MAX_LEN];
} UdsTesterScript;

/* スキャンツール役のタスクが参照するデータと、送る・待つの状態。
   sample_indexは試験台の時間の進み方として読むだけで、エンジン監視ECUの記録や状態には触れない */
typedef struct {
    const UdsTesterScript *script;
    const int             *sample_index;
    CanBus                *can_bus;
    uint8_t                next;               /* 次に送る要求の番号 */
    bool                   waiting;            /* 応答を待っている間true */
    uint32_t               sent_ms;            /* 待っている要求を送った時刻 */
    uint32_t               last_response_ms;   /* can_receive_diag_responseに渡す */
} UdsTesterContext;

/* filenameからREQ@N=の行だけを読み込み、scriptに入れる。値は空白で区切った16進数の1〜7バイト。
   Nが1以上の整数でない行・バイトが16進数の0〜FFでない行・バイト数が1〜7でない行は無視する。
   UDS_TESTER_REQUEST_MAXを超えた分は無視する。ファイルが無い場合・行が無い場合はcount=0にしてfalseを返す。
   読み込んだときだけ件数をログに出す */
bool uds_tester_load(UdsTesterScript *script, const char *filename);

/* ctxを、まだ何も送っていない状態にする */
void uds_tester_init(UdsTesterContext *ctx, const UdsTesterScript *script, const int *sample_index, CanBus *bus);

/* 応答を待っていれば、応答を受け取って表示するか、P2を過ぎたら「応答なし」と表示する。
   待っていなければ、次の要求のNが今のサンプル番号以下なら送って表示する。
   最後のサンプル（20）の後に送った要求は、実行が終わるので応答を受け取れない */
void uds_tester_run(UdsTesterContext *ctx);

#endif /* UDS_TESTER_H */
