# Day44

## 実施内容

- 新規`include/dtc_status.h`/`src/dtc_status.c`を作成した
- `src/diag.c`の`diag_check`を`dtc_status_update`を呼ぶ形にリファクタリングした（`DtcStatus`の定義を`include/diag.h`から`dtc_status.h`へ移した）
- 新規`include/can_diag.h`/`src/can_diag.c`を作成した
- `src/main.c`を更新した（CAN受信タスクから`can_diag_check`を呼ぶよう配線した）
- `Makefile`に`src/dtc_status.c`・`src/can_diag.c`と、新規`test_dtc_status`・`test_can_diag`ターゲットを追加した
- 新規`test/test_dtc_status.c`（6ケース）・`test/test_can_diag.c`（5ケース）を作成した

## 確認内容

- `diag.c`のリファクタリングが既存`diag_check`の挙動を変えていないか（既存`test_diag.c`7ケース）
- `dtc_status_update`の発生回数・状態区分（NONE/ACTIVE/HISTORY）の遷移が正しいか
- `can_diag_check`が`CanLinkState`の遷移（OK→LOST→OK）を正しくDTC相当の記録に反映し、メッセージ間で独立しているか
- `make clean && make`が警告0件で通ること
- `make test`で全16ターゲットが通ること
- `make run`で既存動作（サンプル数・間隔）に影響が無いこと、CAN-DTCの表示が正しい書式で出ること

## 実行結果

- `make clean && make`：初回`can_diag.c`のsnprintfでバッファサイズ不足の警告が出たため`char line[48]`を`[64]`に直し、警告0件で通った
- `make test`：全16ターゲットPASS。`test_diag`は既存7ケースとも変わらずPASSし、新規`test_dtc_status`6件・`test_can_diag`5件もPASS
- `make run`：約19.1秒で完走（既存と同じ）。終了時に`[CAN-DTC] FaultStatus   NONE     link lost occurrences: 0`/`[CAN-DTC] EngineStatus  NONE     link lost occurrences: 0`が表示された
- `make run`ではCANリンクのLost/Recoveryは発生しなかった。原因を調べると、CAN送受信タスクが共にイグニッションON時のみ動作するため（OFF中は送受信双方が止まり、ON復帰後は直ちに新しいフレームを受信する）、Timeout/Invalid Dataを実際に`make run`で再現するには送信側だけを意図的に止める・壊すCAN用のFault Injection機構が必要と判明した

## 判定

`test_diag`の既存7ケースが変更なくPASSしたことから、`diag.c`のリファクタリングは挙動を変えていないと判断できる。新規テスト（`test_dtc_status`6件・`test_can_diag`5件）も期待通りの結果と一致し、発生回数・状態遷移のロジックは正しく動作していると判断できる。`make run`でLost/Recoveryの実行時再現はできなかったが、原因は既存のイグニッションゲーティング（意図した設計）であり、CAN用Fault Injectionという別テーマが未着手であることによる制約と切り分けられたため、Phase21の実装自体（記録ロジック・配線）の判定には影響しないと判断した。

## 今回の設計方針

- `diag_check`のエッジ検出→カウント→NONE/ACTIVE/HISTORY分類部分を、`debounce.c`と同じ「2つ目の具体的な利用先ができたら汎用化する」基準で新規`dtc_status.c`へ切り出した。フリーズフレーム相当の処理は物理センサ固有のため`diag.c`に残した
- CAN側の識別子は`SensorId`を流用せず、`CanMessageId`で別に持つ構造（`CanDtcRecord`）にした。実車のP-code/U-code分離に倣い型は分けたまま記録ロジックだけを共有する折衷案とし、AUTOSAR Demのような完全統一は識別子空間の変更が大きすぎるため見送り、`study_plan.md`の保留中の候補に将来案として記録した
- `can_diag_check`は`can.c`を変更せず、`CanMonitor.state`の前回値を`main.c`側で読み取ってから呼ぶ方式にした
- CAN受信タスクのイグニッションによるガードは、既存の「イグニッションOFF＝ECU無通電」モデルと整合しておりバグではないと判断した。Timeout/Invalid Dataの実行時確認には別途Fault Injectionが必要という制約として切り分けた

## 次回やること

- Fault Injection（CAN通信のTimeout/Invalid Dataを意図的に発生させる仕組み）に着手するかを検討する。フォルダ分割はECU2着手直前に行う方針のため、まだ時期尚早と判断した
