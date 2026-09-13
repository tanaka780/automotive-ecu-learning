# Day43

## 実施内容

- 新規`include/debounce.h`/`src/debounce.c`を作成した
- `src/faultmgr.c`の`faultmgr_check`を、内部で`debounce_update`を呼ぶ形にリファクタリングした
- 新規`include/can.h`/`src/can.c`を作成した
- `src/main.c`を更新した（`CanBus`/`CanMonitor`の初期化、CAN送受信タスク3個の登録、`sample_index`の管理方式変更）
- `Makefile`に`src/debounce.c`・`src/can.c`を追加し、`test_debounce`・`test_can`ターゲットを新設した
- 新規`test/test_debounce.c`（7ケース）・`test/test_can.c`（8ケース）を作成した

## 確認内容

- `make clean && make`が警告0件で通ること
- `make test`で既存13ターゲット（`test_faultmgr`含む）＋新規`test_debounce`・`test_can`が全てPASSすること。特に`test_faultmgr.c`はソース内容を変更しておらず、リファクタリング後も検証結果が変わらないことを確認する
- `make run`で、CAN送受信タスク追加後もサンプル数・間隔（20サンプル・1000ms間隔、約19.1秒）が変わらないこと
- ゲージデータ（1000ms周期）・警告灯データ（200ms周期）がそれぞれ意図した頻度で送受信され、イグニッションOFF中はCAN送受信も出力されないこと

## 実行結果

- `make clean && make`：警告0件
- `make test`：既存13ターゲット＋`test_debounce`（7件）・`test_can`（8件）を含む全テストがPASS（`test_faultmgr`は10件、リファクタリング前と同じ検証内容・件数のままPASS）
- `make run`：約19.097秒（従来の約19.1秒と一致）、Sample 1〜20を確認。`[METER] EngineStatus[OK] ...`が1000ms間隔で、`[METER] FaultStatus[OK] ...`が200ms間隔（1サンプルあたり約5回）で出力され、イグニッションOFF区間では`[METER]`行が出力されないことを確認した

## 判定

確認内容と実行結果が一致しており、CAN通信（Phase20）の追加によって既存の動作（サンプル数・間隔、`test_faultmgr`の検証結果）に影響が出ていないことを確認した。

## 今回の設計方針

- 技術選定の判断軸をCLAUDE.mdの判断順序（①ストーリー上の必要性）に統一した。「車載で使われる技術だから学ぶ」ではなく「このプロジェクトのストーリーに必要か」で判断し、SocketCANは本筋のファームウェア実装ではなくPC側ツールの技術と整理できた
- 案2でも、フレーム構造・非ブロッキング受信・Scheduler連携・Timeout/Invalid Data検知といった、実際のECUファームウェア設計に直結する学びはすべて得られることを確認した
- 当初「Phase化はCAN実装着手時に行う」つもりだったが、実現方式（案1/案2）の決定自体がすでにPhase17（DTO整理）と同じ「決定のみでPhaseを区切る」パターンに当てはまると気づき、Day43時点でPhase20として先に区切ることにした
- プロセス構成の検討では、当初「CAN通信＝実行ファイルを2つに分けてIPCで繋ぐ」と無意識に決めつけていたことに気づいた。Phase20の理解目標（フレーム構造・Scheduler統合・Fail-safe接続）にはプロセス間通信の技術自体は含まれておらず、実行ファイルを2つにすると、前回保留にしたフォルダ分割・ECU2着手のタイミング問題を巻き込んでしまう上、Unityでの自動テストができなくなり（実プロセス間通信の検証にはPythonでの結合テスト等、未着手のSILS領域が必要）、CLAUDE.mdの判断順序③（テスト方法）にも反する。単一プロセス内でSchedulerタスクとして表現する形にすれば、既存のTimer/Scheduler/faultmgr資産にそのまま乗り、Unityでも従来通りテストできる
- 実装中に、Scheduler（Phase19）に複数周期のタスクを混在させたときの前提崩れに気づいた。Phase19時点ではタスクが1個のみだったため「1回の`scheduler_run_due`呼び出し＝1サンプル」という単純な対応関係でmain()のループを書いていたが、これはタスクが1個であることに暗黙に依存した設計だった。2個目以降の周期タスク（CAN）を追加して初めて顕在化した前提であり、Phase19時点でこの前提を明示的にコメントしておくべきだったと反省した

## 次回やること

- Phase20（CAN通信）は完了。次の候補は、study_plan.mdで保留していた「フォルダ分割のタイミング」の再検討（CAN通信の実装は完了したため、判断材料が揃った）と、保留中の候補にある「CAN異常処理」（Timeout/Invalid Data確定をDTC記録につなげる）のどちらを先に検討するかを次回決める
