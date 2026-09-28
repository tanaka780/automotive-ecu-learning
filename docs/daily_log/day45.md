# Day45

## 実施内容

- 新規`include/can_fault.h`・`src/can_fault.c`を作成した（`can.c`の送信関数をラップし、指定期間だけ意図的に送信をスキップしてTimeoutを再現する仕組み）
- `src/main.c`を更新した（CAN送信をcan_fault経由でラップするよう配線した）
- `Makefile`に`src/can_fault.c`と、新規`test_can_fault`ターゲットを追加した
- 新規`test/test_can_fault.c`（10ケース）を作成した

## 確認内容

- `can_fault_is_dropped`のDrop判定（対象メッセージの一致・時間範囲の境界）が正しいか
- `can_fault_load`のファイルパース（正常系・異常系）が正しいか
- `make clean && make`が警告0件で通ること
- `make test`で全17ターゲットが通ること
- `make run`で、`can_fault.txt`の設定通りにCANリンクのLost/Recoveryが実際に再現されるか

## 実行結果

- `make clean && make`：警告0件で通った
- `make test`：全17ターゲットPASS（既存16件に変更なし、新規`test_can_fault`10件PASS）
- `make run`（`can_fault.txt`無し）：既存動作に変更なし、`[CAN-FAULT] No CAN fault file (no injection)`が表示された
- `make run`（`can_fault.txt`：`TARGET=FAULT_STATUS`, `START_MS=3000`, `END_MS=6000`）：初回実装ではDropが一度も発生しなかった。原因を調べると、`timer_get_elapsed_ms`はシステム起動からの経過時間を返す（プログラム起動からではない）ため、`START_MS`/`END_MS`との比較が実質機能していなかったと判明した。`CanFaultConfig`に基準時刻（`base_ms`）を追加し差分を取る形に修正した後は、`[CAN] FaultStatus link lost`→`link recovered`、`[CAN-DTC] FaultStatus HISTORY link lost occurrences: 1`が正しく表示された

## 判定

テストは全てPASSし、`make run`でも意図した通りにCANリンクのLost/Recoveryが再現されたことから、Phase22の実装は期待通りに動作していると判断できる。ただし初回実装時に`timer_get_elapsed_ms`の仕様（システム起動からの経過時間である点）を誤解したバグがあり、これは`make run`の実行確認で初めて発覚した。自動テスト（`can_fault_is_dropped`への直接呼び出し）だけでは検出できなかった不具合であり、実行確認の重要性を再確認した。

## 今回の設計方針

- `can.c`本体は変更せず、新規`can_fault.c`が送信関数をラップする設計にした（責務を汚さないためのラップという、`debounce.c`/`dtc_status.c`の「共通ロジックの切り出し」とは異なる設計判断）
- Drop期間はメッセージの送信回数ではなく、プログラム起動からの経過時間（`START_MS`/`END_MS`）で指定する設計にした。実車のHIL試験ベンチが故障ウィンドウを時間で指定する慣習に合わせた
- `can_fault_is_dropped`を、`timer.c`を直接呼ばない純粋関数（経過時間を引数で受け取るだけ）にし、テストから任意の時刻を指定できるようにした
- `START_MS >= END_MS`のような矛盾はチェックしない方針にした（Phase12で決めた閾値大小関係チェック省略の方針を踏襲）

## 次回やること

- Phase22で対象外としたInvalid Data注入への拡張、または保留中の候補（NVM強化・Watchdog等）のどちらに進むかを次回検討する。フォルダ分割・ECU2着手は着手順序上まだ早いと判断し、引き続き見送る
