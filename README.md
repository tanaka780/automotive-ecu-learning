# automotive-ecu-learning

車載ソフトウェア開発への理解を深めるため、C言語、状態管理、診断、通信の基礎を段階的に学ぶ個人開発プロジェクト。

---

## 概要

C言語未経験から始め、車載ECUで使われる考え方を小規模な実装で段階的に学ぶ。
実車ECUの再現が目的ではなく、「何をしたいコードか」「なぜその構成にしたか」を
自分で説明できる状態にすることを目指す。

---

## 学習目的

- C言語の基礎を理解する
- データの流れを追える設計を身につける
- 車載ソフトウェアで使われる考え方を小さく段階的に理解する
- 学習過程をGitHubに記録し、第三者に説明できる状態にする

---

## プロジェクトの方向性

リポジトリ名（automotive-ecu-learning）が示す通り、当初からC言語で自動車関連プログラムを作ることを目的としていた。Phase1〜9では、その足がかりとして、センサ・DTC・ログ・永続化・設定ファイル等、自動車関連プログラムで使われる個々の簡単な機能を1つずつ作りながら学んできた。これらが揃ってきた現在は、それらを統合し、「車両状態を模擬し、異常を検出・診断・記録し、電源再投入後も診断情報を保持できるECUソフトウェアシミュレータ」として完成させる方向へ焦点を移している。

Phase10以降も新しい技術（Timer・Scheduler・CAN・Watchdog・Python等）の学習自体は続くが、テーマを選ぶ基準は「学習順序として妥当か」から「ECUの完成ストーリーに必要か」（CLAUDE.md記載の判断順序）へ変わる。

Phase1〜9はこの完成目標に向けた基礎実装・設計基盤の構築期間として位置付ける。Phase10以降の方向性は docs/study_plan.md を参照。代表的な車両シナリオ（正常走行／故障発生／診断コマンド／電源再投入／設定ファイル異常時のフェイルセーフ／通信故障／冷却ファン制御／水温センサ故障時の冷却ファン制御）は docs/scenarios.md を参照。

---

## ドキュメント

| ファイル | 内容 |
| --- | --- |
| [docs/project_context.md](docs/project_context.md) | 現在の実装状況・モジュール構成 |
| [docs/study_plan.md](docs/study_plan.md) | 学習計画とPhase構成 |
| [docs/scenarios.md](docs/scenarios.md) | 車両シナリオの定義 |
| [docs/learning_journal.md](docs/learning_journal.md) | 学んだことの理解メモ |
| [docs/daily_log/](docs/daily_log/) | 日ごとの作業記録 |
| [docs/AI_workflow.md](docs/AI_workflow.md) | AI活用方針 |

---

## 開発環境

- OS: Windows / WSL2 Ubuntu
- Editor / AI: ChatGPT, Claude
- Compiler: gcc (`-Wall -Wextra -std=c11`)

---

## 実行方法

```bash
make clean && make && make run
```

テスト（diag.c・persist.c・stats.c・alert.c・ignition.c・cmd.c・config.c・fixture.c・validate.c・faultmgr.c・timer.c・scheduler.c・debounce.c・can.c・dtc_status.c・can_diag.c・can_fault.c の動作確認、固定値データ使用。Phase13より[Unity](https://github.com/ThrowTheSwitch/Unity)形式に統一）:

```bash
make test
```

テストカバレッジ計測（Phase23。17テストターゲットそれぞれについて、担当モジュール1つのgcov実行行数割合を表示。`lcov`等によるプロジェクト全体の合算は未導入）:

```bash
make coverage
```

---

## 現在の実装

| 機能 | 概要 |
| --- | --- |
| センサシミュレーション | 車速・RPM・水温をランダム更新・表示 |
| 統計表示 | 20サンプル分の最小・最大・平均を表示 |
| アラート表示 | 閾値を超えたセンサ値を警告表示 |
| センサ状態表示 | NORMAL / WARNING / CRITICAL の3段階で状態を分類・表示 |
| 固定幅整数型 | センサ値・統計値を `uint8_t` / `uint16_t` / `uint32_t` で型明示 |
| DTC記録 | CRITICALに入った瞬間を検出し、センサ別の発生回数と状態区分（ACTIVE / HISTORY）を記録・表示 |
| フリーズフレーム | 最初にCRITICALが発生した瞬間の全センサ値を1件だけ記録・表示 |
| イグニッション状態 | OFF/ONの状態をランダム更新・表示し、遷移した瞬間だけイベント表示 |
| DTC永続化 | プログラム終了時にDTC記録をテキストファイルへ保存し、次回起動時に読み込んで継続する |
| 診断コマンド | プログラム終了時、イグニッションOFF時のみ`clear`コマンドを入力すると全DTC記録を、`clear <センサ名>`（speed/rpm/temp）を入力すると指定センサ1件分のDTC記録だけをリセットできる（UDS風のDTCクリアの簡易再現）。フリーズフレームは、その原因がクリア対象のセンサと一致する場合だけ合わせてリセットする。イグニッションON時はコマンドを受け付けず、受け付けなかった旨を表示する |
| 閾値の外部設定 | `config.txt`（`KEY=VALUE`形式）から警告・状態判定の閾値9個を読み込む。ファイルが無ければデフォルト値（`alert.h`/`status.h`のマクロ値）のまま動作する |
| ログレベル制御 | `config.txt`の`LOG_LEVEL`（0=INFO/1=WARNING/2=ERROR）で実行時のログ表示閾値を切り替える。`ALERT`はWARNING、`PERSIST`の保存失敗・データ破損はERROR、それ以外はINFOとして扱う |
| センサ固定値注入 | `fixture.txt`（`MODE=FIXED/RANDOM`形式）から固定センサ値を読み込む。`MODE=FIXED`ならセンサ値を固定値に差し替え、ファイルが無い／`MODE=RANDOM`なら従来通りランダムで動作する |
| 入力値域チェック | `config.txt`（閾値）・`fixture.txt`（センサ固定値）から読み込んだ値が、センサごとの物理的にありえる範囲（speed 0〜120／rpm 0〜6000／temp 25〜100）・LOG_LEVELの範囲（0〜2）に収まっているかを検証する。範囲外の値はそのキーだけ無視され、他のキーは反映される |
| 故障確定とFail-safe | センサ別にCRITICALが3回連続したら「一時的なノイズ」ではなく確定した異常（Degraded）とみなし（Debounce）、確定後はそのセンサの値をフェイルセーフ値に差し替えて警告・統計に反映する（縮退動作）。NORMALが3回連続したら復帰する（Recovery）。DTC記録・診断コマンド（`clear`）はDegraded状態と独立して動作する |
| 起動時自己診断（POST） | 起動時に`config.txt`・`dtc_data.txt`の読み込み結果を合成し、両方成功なら`[POST] Self-check passed`、いずれか失敗なら`[POST] Self-check did not pass, continuing with defaults`を表示する。判定結果で動作を変えるものではなく、既存のフェイルセーフ（デフォルト値継続）を可視化するのみ |
| 周期処理（Timer） | サンプルループの待機を、固定`sleep(1)`から単調増加クロック（`CLOCK_MONOTONIC`）による経過時間ベースの周期判定（`timer_is_due`）に置き換える。外部から見た1秒間隔の動作自体は変わらない |
| タスク管理（Scheduler） | 関数ポインタ＋周期を持つタスクを複数保持できるタスクテーブルを導入し、周期の待機・呼び出しをmain.cから委譲する。サンプル周期タスクに加え、Phase20でCAN送受信タスクを登録している |
| CAN通信 | 実プロセス分離は行わず、単一プロセス内でエンジンECU役（送信）・メーターECU役（受信）をSchedulerタスクとして表現する。ゲージデータ（speed/rpm/temp、1000ms周期）と警告灯データ（センサ別の確定異常フラグをビット詰め、200ms周期）を別メッセージ（CAN ID）で送受信し、受信側はTimeout（新しいフレームが来ない）・Invalid Data（validate.cでの値域外判定）をDebounce/Recovery（3回連続）で確定・復帰させる |
| CAN異常処理（DTC反映） | CANリンクのLost/Recoveryをdiag.cのDTC相当の記録（発生回数・状態区分NONE/ACTIVE/HISTORY）としてメッセージ別に記録する。診断コード管理の共通ロジック（エッジ検出→カウント→状態分類）はdiag.cから切り出し、物理センサ・CAN通信の両方から使う |
| CAN Fault Injection | `can_fault.txt`の`MODE`で、指定したメッセージ・期間（プログラム起動からの経過時間）だけ意図的にCAN送信をスキップしTimeoutを再現する（`MODE=DROP`）、またはEngineStatusの送信データを値域外に差し替えInvalid Dataを再現する（`MODE=CORRUPT`、EngineStatusのみ対応）。can.c本体は変更せず、送信関数をラップする新規can_fault.cが担う。ファイルが無ければ従来通り注入なしで動作する |

---

## モジュール構成

| モジュール | 役割 |
| --- | --- |
| `main.c` | 初期化・ループ制御。起動時自己診断（POST、`config.txt`/`dtc_data.txt`の読み込み結果の合成）も担う |
| `sensor.c` | センサ値の更新・表示 |
| `stats.c` | 統計値の更新・表示 |
| `alert.c` | 閾値チェック・警告表示 |
| `status.c` | センサ値の状態分類（NORMAL / WARNING / CRITICAL）と表示 |
| `diag.c` | DTC（故障診断コード）の記録・表示、フリーズフレームの記録、全体クリア（`diag_clear`）とセンサ単位クリア（`diag_clear_sensor`）。発生回数・状態区分（NONE/ACTIVE/HISTORY）の更新自体は`dtc_status.c`に委譲する |
| `logger.c` | 出力の窓口の一元化。タグなし（`log_print`）・タグ付き（`log_print_tagged`）に加え、レベル付き（`log_print_leveled`）で`logger_set_level`が設定した表示閾値未満のログを抑制する |
| `ignition.c` | イグニッション状態（OFF/ON）の更新・遷移検出・表示 |
| `persist.c` | DTC記録のファイルへの保存・読み込み、成功/失敗のログ表示 |
| `cmd.c` | 標準入力から診断コマンド（`clear`／`clear <センサ名>`相当）を読み込み、解釈してDTC記録のクリア（全体／センサ単位）を要求する。想定外の入力は理由に応じて区別して通知する。イグニッションOFF時のみという受付条件を満たさない場合の通知（`cmd_notify_rejected`）も担う |
| `config.c` | 閾値9個（alert.c/status.c）とログレベル（`LOG_LEVEL`）の設定値を保持し、`KEY=VALUE`形式の設定ファイルから読み込む（ファイルが無ければデフォルト値のまま）。読み込んだ値はvalidate.cで値域チェックしたうえで反映し、alert.c/status.cの判定、およびlogger.cの表示閾値に反映される |
| `fixture.c` | `fixture.txt`を`KEY=VALUE`形式で読み込む。`MODE=FIXED`なら`SPEED`/`RPM`/`TEMP`をvalidate.cで値域チェックしたうえでセンサ値に反映し、`main.c`はそれ以降の`sensor_update`呼び出しをスキップする。`MODE=RANDOM`・ファイル無し・`MODE`未指定の場合は何もせず、従来通りランダムに動作する |
| `validate.c` | `SensorId`（sensor.h）をインデックスにした値域テーブル（min/max）を持ち、値がセンサごとの物理的な範囲内かを判定する（`validate_in_range`）。LOG_LEVEL用に0〜2の範囲チェック（`validate_log_level`）も別途提供する。config.c・fixture.cの両方から呼ばれる |
| `faultmgr.c` | センサ別に`status_check`の分類結果を見て、CRITICALの連続回数（Debounce）・確定後のNORMAL連続回数（Recovery）をカウントし、確定(Degraded)・復帰(Recovery)を判定する。Degraded中のセンサ値をフェイルセーフ値に差し替えたコピーを作る（`faultmgr_apply_safe_values`）。`main.c`は`diag_check`より後・`alert_check`/`stats_update`より前にこの差し替えを適用し、診断の正確性（raw値）と縮退動作の継続（フェイルセーフ値）を両立させる |
| `timer.c` | 単調増加クロック（`CLOCK_MONOTONIC`）による起動からの経過時間取得（`timer_get_elapsed_ms`）と、周期判定（`timer_is_due`）を提供する。`main.c`はサンプルループの待機（旧`sleep(1)`）をこの周期判定のポーリングに置き換えている |
| `scheduler.c` | 関数ポインタ・context・`timer.c`の`Timer`を1組にしたタスクを固定長配列（`SCHEDULER_MAX_TASKS`）で複数保持し、優先度を持たず登録順に周期判定・実行する（`scheduler_add_task`/`scheduler_run_due`）。各タスクは初回呼び出し時は周期を待たず即座に実行され（offset=0相当）、2回目以降は登録した周期(`period_ms`)ごとに実行される。`main.c`はサンプル周期タスク（`run_sample_cycle`）に加え、Phase20でCAN送受信タスクを3個登録している（計4個、`SCHEDULER_MAX_TASKS`と一致） |
| `debounce.c` | 「一時的なノイズ」と「確定した異常」を区別する、Debounce（確定）・Recovery（復帰）の連続回数カウントだけを汎用化したもの（`debounce_update`）。対象（センサ値かCAN通信か等）の意味は一切知らない。`faultmgr.c`（センサ異常）・`can.c`（CAN通信異常）の両方から呼ばれる（Phase20、`validate.c`と同じ「2つ目の利用先ができたら汎用化する」基準で`faultmgr.c`から切り出した） |
| `can.c` | 実CANに準拠したフレーム構造（ID・DLC・data配列）を持つ`CanFrame`と、メッセージごとに最新の1フレームを保持する`CanBus`（バス役）を提供する。エンジンECU役はゲージデータ（`can_send_engine_status`）・警告灯データ（`can_send_fault_status`、`faultmgr.c`の確定状態をビット詰め）を送信する。メーターECU役は受信時に送信タイムスタンプ（`timer.c`）から新しいフレームかを判定し（非ブロッキング方式）、Timeout（新しいフレームが来ない）・Invalid Data（`validate.c`での値域外判定）を`debounce.c`で確定・復帰させる（`CanLinkState`、`FaultManager`とは別カテゴリの通信専用状態） |
| `dtc_status.c` | DTCの状態区分（`DtcStatus`：NONE/ACTIVE/HISTORY）と、前回/今回の異常有無を比較して発生回数・状態区分を更新する`dtc_status_update`を提供する。異常の発生源（物理センサかCAN通信か等）は一切知らない汎用ロジックで、`diag.c`・`can_diag.c`の両方から呼ばれる |
| `can_diag.c` | CAN通信リンク別（`CanMessageId`）のDTC相当の記録（`CanDtcRecord`：発生回数・状態区分）を提供する。`CanLinkState`の前回/今回比較を`dtc_status.c`に渡して更新する（`can_diag_check`）。物理センサの`DtcEntry`（`SensorId`）とは別の識別子で持ち、実車のU-code（通信異常）とP-code（物理故障）の分離に倣う |
| `can_fault.c` | `can_fault.txt`（`MODE=DROP/CORRUPT/NORMAL`、`TARGET=ENGINE_STATUS/FAULT_STATUS`、`START_MS`/`END_MS`）を読み込む。`MODE=DROP`は指定期間だけ`can.c`の送信関数（`can_send_engine_status`/`can_send_fault_status`）の呼び出しをスキップしてTimeoutを再現する。`MODE=CORRUPT`はEngineStatusのみ対応し、送信専用の値域外データ（speed/rpm/temp全て`0xFF`）を持つ一時コピーを作って`can_send_engine_status`に渡すことでInvalid Dataを再現する（実センサ値は変更せず物理センサ診断には影響しない。FaultStatusとの組み合わせは無効な指定として注入なし扱い）。`can.c`本体は変更せず送信をラップするのみ |
| `test/test_diag.c` | 固定値データによる diag.c の動作確認（`make test`で実行） |
| `test/test_persist.c` | 固定値データ・意図的に壊したデータによる persist.c の正常系・異常系の動作確認（`make test`で実行） |
| `test/test_stats.c` | 固定値データによる stats.c の動作確認（`make test`で実行）。サンプル投入用のヘルパーは test_common.c を使わずファイル内にローカルで定義 |
| `test/test_alert.c` | 標準出力キャプチャ（`freopen`＋`dup`/`dup2`）による alert.c の動作確認（`make test`で実行）。閾値境界・単独超過・複数同時超過時の警告出力を確認する |
| `test/test_ignition.c` | 標準出力キャプチャ（`freopen`＋`dup`/`dup2`）による ignition.c の動作確認（`make test`で実行）。OFF/ONの4パターン（遷移あり/なし）で、遷移した瞬間だけイベントが出力されることを確認する |
| `test/test_cmd.c` | 固定値データによる `diag_clear`・`diag_clear_sensor`・`cmd_dispatch`（全体クリア／センサ単位クリア／不正なセンサ名／余分なトークン／空白のみ等）の動作確認（`make test`で実行）。標準入力を扱う `cmd_read_line` は対象外（`make run`での実行確認で扱う） |
| `test/test_config.c` | 固定値データによる `config_load` のファイルパース動作確認（`make test`で実行）。正常系（全9キーの反映）、異常系（未知のキー・値欠落・数値以外の行は無視される、値域外の値は無視される、キー重複時は後勝ち）を確認する |
| `test/test_fixture.c` | 固定値データによる `fixture_apply` のファイルパース動作確認（`make test`で実行）。正常系（`MODE=FIXED`で全キーの反映）、異常系（未知のキー・値欠落・数値以外の行は無視される、値域外の値は無視される、キー重複時は後勝ち、`MODE=RANDOM`時はセンサ値を変更しない）を確認する |
| `test/test_validate.c` | 固定値による `validate_in_range`・`validate_log_level` の境界値確認（`make test`で実行）。speed/rpm/temp各センサの下限・上限・範囲外、不正な`SensorId`、LOG_LEVELの下限・上限・範囲外を確認する |
| `test/test_faultmgr.c` | 固定値による `faultmgr_check`・`faultmgr_apply_safe_values` の動作確認（`make test`で実行）。Debounce確定前後の境界、連続が途切れた場合のカウント数え直し、Recoveryの境界（WARNING止まりでは復帰しない）、複数センサの独立性、フェイルセーフ値差し替え時のraw非破壊を確認する |
| `test/test_timer.c` | timer.c の動作確認（`make test`で実行）。経過時間の単調増加、周期判定（`timer_is_due`）がinit直後はfalse・周期経過後はtrue・trueを返した直後は再びfalseに戻ることを確認する。実際の待機（数十ms）を伴う点が他のテストと異なる |
| `test/test_scheduler.c` | scheduler.c の動作確認（`make test`で実行）。登録直後の初回呼び出しが周期を待たず即座に実行されること、初回実行後は周期(`period_ms`)が経過するまで次が実行されないこと、上限（`SCHEDULER_MAX_TASKS`）を超える登録が`false`を返すことを確認する。timer.cと同様、実際の待機を伴う |
| `test/test_debounce.c` | debounce.c の動作確認（`make test`で実行）。確定・復帰それぞれの閾値到達前後の境界、連続が途切れた場合のカウント数え直し、confirm_threshold/recover_thresholdが異なる値でも独立して動作することを確認する |
| `test/test_can.c` | can.c の動作確認（`make test`で実行）。ゲージデータ・警告灯データそれぞれの送受信の往復一致、未受信が3回連続した場合のTimeout確定、値域外データが3回連続した場合のInvalid Data確定、有効受信が3回連続した場合のRecoveryを確認する。timer.c/scheduler.cと同様、Timeout解除の確認には実際の待機を伴う |
| `test/test_dtc_status.c` | dtc_status.c の動作確認（`make test`で実行）。異常発生の瞬間（エッジ）だけ発生回数が増えること、継続中は二重カウントしないこと、解消時のHISTORY遷移、再発生時の再カウントを確認する |
| `test/test_can_diag.c` | can_diag.c の動作確認（`make test`で実行）。`CanLinkState`のLost確定・Recovery復帰によるDTC相当の記録の更新、メッセージ（EngineStatus/FaultStatus）間の独立性を確認する |
| `test/test_can_fault.c` | can_fault.c の動作確認（`make test`で実行）。Drop判定（`can_fault_is_dropped`）・Corrupt判定（`can_fault_is_corrupted`）の対象メッセージ一致・時間範囲の境界と、`can_fault.txt`のファイルパース（`can_fault_load`の正常系・異常系、`MODE=CORRUPT`+`TARGET=FAULT_STATUS`が無効な組み合わせとして注入なし扱いになることを含む）を確認する |
| `test/test_common.c` | test_diag.c・test_persist.c・test_cmd.c・test_alert.c・test_config.cで共通のテスト補助関数（サンプル投入用の`test_feed`/`test_run_sample`、デフォルトの`ConfigData`を返す`test_default_config`）を提供する。Phase13でテスト自体をUnity形式に統一したため、結果判定・サマリ表示（旧`test_check`/`test_summary`）の役割はUnityに置き換わった |

---

## 学習フェーズ

| Phase | テーマ | 状態 |
| --- | --- | --- |
| Phase1 | C基礎・データフロー・責務分割 | 完了 |
| Phase2 | DTC（故障診断コード）管理 | 完了 |
| Phase3 | logger（出力層の共通化） | 完了 |
| Phase4 | 状態遷移（イグニッション状態管理） | 完了（OFF中は他モジュール呼び出しをスキップ。サイクルまたぎのDTC/統計はリセットせず継続） |
| Phase5 | DTCの永続化（ファイルI/O） | 完了（永続化の実装、エラーハンドリング、test/test_persist.cによるテスト、test_diag.cとの重複処理のtest_common.h/.cへの切り出し、stats.cへの自動テスト追加まで完了） |
| Phase6 | 標準出力を伴う判定処理のテスト（標準出力キャプチャ） | 完了（alert.c・ignition.cの自動テスト（test/test_alert.c・test/test_ignition.c）、Makefile統合まで完了） |
| Phase7 | 診断コマンド入力（UDS風のDTCクリア） | 完了。拡張バックログのうち、クリア範囲の指定（`clear <センサ名>`）、拒否理由の区別（不正なセンサ名／想定外コマンドの区別）、受付条件の制限（イグニッションOFF時のみ）まで対応済み。イグニッションOFF遷移時に受付タイミングを変える案はTimer/Scheduler以降に先送り |
| Phase8 | Config（閾値の外部化） | 完了（`config.h`/`config.c`の新規作成、`main.c`への組み込み、`alert.c`/`status.c`への反映、非デフォルトconfigでの判定反映を確認する自動テスト、`test_config.c`によるファイルパースの正常系・異常系テストまで完了） |
| Phase9 | Logger拡張（ログレベル） | 完了（`LogLevel`と`log_print_leveled`の新設、`ConfigData`への`log_level`追加、`main.c`からの配線、既存ログ呼び出しの`log_print_leveled`への置き換えまで完了。`config.c`自身の読み込み結果ログ2箇所は、表示閾値が確定する前に呼ばれるため対象外とした） |
| Phase10 | 車両シナリオの定義 | 完了。正常走行／故障発生／診断コマンド（DTCクリア）／電源再投入／設定ファイル異常時のフェイルセーフ（リンプホームモード）／通信故障（将来項目）の6シナリオを`docs/scenarios.md`に整理した |
| Phase11 | 固定値注入によるシナリオ再現の仕組み構築 | 完了（`fixture.h`/`fixture.c`の新規作成、`main.c`への組み込み、`test/test_fixture.c`による自動テスト、`make run`での動作確認まで完了） |
| Phase12 | 入力妥当性チェック（Guard Clause） | 完了（`validate.h`/`validate.c`の新規作成、config.c/fixture.cへの組み込み、`test/test_validate.c`による自動テスト、`test_config.c`/`test_fixture.c`への値域外ケース追加、`make run`での実行確認まで完了） |
| Phase13 | Unity試用 | 完了（自作`test_common.c`パターンと比較しUnity採用を決定、既存9テストターゲット全てをUnity形式（`vendor/unity/`）に移行。検証内容は変更なし） |
| Phase14 | MISRA対応 | 完了（cppcheckのMISRA C:2012アドオンで検出した12ルールを判断。5.9/8.9/10.4/10.8/15.6/15.7・17.7（26件、`persist.c`の保存失敗検出修正含む）・12.1/18.4は適用、21.6/21.10・15.5は不採用（理由は既知の制約参照）） |
| Phase15 | 故障確定とFail-safe（Debounce→Degraded mode→復帰） | 完了（新規`faultmgr.h`/`faultmgr.c`を作成。センサ別にDebounce（3回連続CRITICAL）で確定、Degraded中はフェイルセーフ値に差し替えて`alert_check`/`stats_update`に渡す縮退動作、Recovery（3回連続NORMAL）で復帰。`status_check`/`diag_check`は常にraw値を見てDTCの正確性を保つ。`test/test_faultmgr.c`による自動テスト、`make run`での実行確認まで完了） |
| Phase16 | 起動時自己診断（POST） | 完了（`main.c`に、`config.txt`・`dtc_data.txt`の読み込み結果を合成する自己診断を追加。判定結果はログ出力のみで動作は変えず、既存のフェイルセーフ（デフォルト値継続）を可視化する。ロジックが単純なためmain.cにインラインで実装し、main.cはUnityのテストターゲットに含められないため自動テストは対象外とし、`make run`で4パターン全て確認した） |
| Phase17 | DTO整理 | 完了（複数ECU化構想を見据え、エンジン監視ECUが送信する想定のデータ範囲（生センサ値＋確定異常フラグ）、メーターECUが受信する範囲、既存構造体を流用するか専用型を新設するかの方針を決定した。CAN通信の実装は伴わず、CAN Phase着手時に反映する） |
| Phase18 | Timer（周期処理の時間管理基盤） | 完了（新規`timer.h`/`timer.c`を作成。単調増加クロック`CLOCK_MONOTONIC`による経過時間取得と、周期判定`timer_is_due`を実装。`main.c`のサンプルループの`sleep(1)`をこの周期判定のポーリングに置き換えた。コールバックによる自動起動は見送り、Schedulerへの土台となる最小限の機能に留めた。`test/test_timer.c`による自動テスト、`make run`での実行確認まで完了） |
| Phase19 | Scheduler（周期処理のタスク管理基盤） | 完了（新規`scheduler.h`/`scheduler.c`を作成。関数ポインタ＋周期を持つタスクを固定長配列で複数保持し、優先度は持たず登録順に判定・実行する最小構成を実装した。`main.c`はサンプル周期タスク（`run_sample_cycle`）を1個登録し、旧`timer_is_due`のポーリング待機を委譲する形に変更。実車OSEK/AUTOSAR OSのoffset（初回起動までの時間）概念を踏まえた上で、タスクが1個のみの現状ではoffsetの効果が無いためoffset機構は実装せず、初回呼び出しは即座に実行する形（offset=0相当）とし、既存の動作（20サンプル・1000ms間隔・初回即時実行）を変えていない。`test/test_scheduler.c`による自動テスト、`make run`での実行確認まで完了） |
| Phase20 | CAN通信（エンジンECU役→メーターECU役のメッセージ送受信） | 完了（実現方式はSocketCAN不採用・自作フレームシミュレーション採用、プロセス構成は単一プロセス内でSchedulerタスクとして表現する方針を決定。新規`can.h`/`can.c`を作成し、ゲージデータ（`CAN_ID_ENGINE_STATUS`、1000ms周期）・警告灯データ（`CAN_ID_FAULT_STATUS`、確定異常フラグをビット詰め、200ms周期）の2メッセージを実装。受信側はTimeout（新しいフレーム未着）・Invalid Data（`validate.c`での値域外）を確定・復帰させる仕組みが必要になり、`faultmgr.c`のDebounce/Recoveryカウント処理を汎用化した新規`debounce.c`に切り出して両方から使う設計にした（`FaultManager`とは別カテゴリの`CanLinkState`として持ち、実車のDTC分類P-code/U-codeの分離に倣う）。`test/test_debounce.c`・`test/test_can.c`による自動テスト、`make run`での実行確認まで完了） |
| Phase21 | CAN異常処理（Timeout/Invalid DataのDTC反映） | 完了（`diag.c`の`diag_check`から発生回数・状態区分（NONE/ACTIVE/HISTORY）の更新ロジックを新規`dtc_status.c`へ切り出し（物理センサ・CAN通信の共通利用、`debounce.c`と同じ切り出し基準）、新規`can_diag.h`/`can_diag.c`で`CanLinkState`のLost/RecoveryをDTC相当の記録として持たせた。識別子（`SensorId`とCAN用の`CanMessageId`）は分けたまま記録ロジックだけを共有する設計とし、AUTOSAR Demのような完全統一は見送り保留中の候補へ記録した。`test/test_dtc_status.c`・`test/test_can_diag.c`による自動テスト、既存`test_diag.c`（リファクタリング後も検証内容不変）まで確認済み。`make run`ではCAN-DTCの表示形式・既存動作への影響が無いことを確認したが、Lost/Recovery自体の自然発生にはCAN用のFault Injection機構が必要と判明し、実行時の再現は対象外とした（Phase22で対応）） |
| Phase22 | CAN Fault Injection（通信故障の意図的な発生） | 完了（新規`can_fault.h`/`can_fault.c`を作成。`can.c`本体は変更せず送信関数（`can_send_engine_status`/`can_send_fault_status`）をラップし、`can_fault.txt`で指定したメッセージ・期間（プログラム起動からの経過時間）だけ意図的に送信をスキップしてTimeoutを再現する。実装時、`timer_get_elapsed_ms`がシステム起動からの経過時間を返す（プログラム起動からではない）ため期間指定が機能しないバグが`make run`で発覚し、基準時刻（`base_ms`）を持たせて差分を取る形に修正した。`test/test_can_fault.c`による自動テスト、`make run`でCANリンクのLost/Recoveryの実行時再現まで確認済み。拡張バックログとして、EngineStatusのInvalid Data注入（`MODE=CORRUPT`、送信専用の破損コピーのみ改ざんし実センサ値は変更しない設計）にも対応した。`test/test_can_fault.c`に4件追加（10→14件）、`make run`でEngineStatusのInvalid Data確定・DTC反映まで確認済み） |
| Phase23 | テストカバレッジ計測（gcov） | 完了（17テストターゲットそれぞれについて、既存のソース構成を再利用した`--coverage`付きビルド（`xxx_cov`）を追加し、担当モジュール1つのgcov実行行数割合を`make coverage`で一括表示できるようにした。プロジェクト全体を1つの数値に合算する`lcov`導入は、共有モジュールが複数ターゲットにまたがる場合に自動では合算されないことを実験で確認した上で、必要性が高まった場合の拡張候補として見送った。`main.c`はUnityとリンクできないため対象外のまま） |
| Phase24 | フォルダ構成の再編成（BSW/アプリ層分割） | 完了（配置の検討（タスク1）のみ実施。PC依存の処理はファイル単位ではなく各ファイル内の一部の関数に限られており、ファイル単位で層に振り分けると層の意味が崩れるため、`src/`の移動は行わず、ドキュメント類の`docs/`への集約のみ実施した） |
| Phase25 | Python自動検証（シナリオの結合テスト） | 着手中（`sensor_sim`に外から入力を与え、`docs/scenarios.md`の期待結果と照合する結合テストとして、検証対象のシナリオとテストの枠組み（`unittest`）を決定した） |
---

## 既知の制約

- 実車ECUや実CAN通信とは接続していない
- センサ値は学習用の簡易モデルであり、実車の物理モデルではない
- DTC永続化データの読み込みは、値の個数が正しければ読み込み成功と判定しており、個々の値が意味的にありえる範囲かまではチェックしていない
- 診断コマンドは`clear`／`clear <センサ名>`（speed/rpm/temp）のみに対応しており、大文字小文字を区別し前後の空白もトリムしない完全一致判定。拒否理由の区別も「不正なセンサ名」「イグニッションON中で受付不可」「それ以外の想定外コマンド」の3種類にとどまる。他のUDS診断サービスは再現していない
- 診断コマンドの受付タイミングはサンプルループ終了後の1回のみで変わっていない。イグニッションOFFへ遷移した瞬間を検出して受け付ける（実際の駐車中スキャンツール接続に近い挙動）わけではなく、ループ終了時点の状態がたまたまOFFかどうかで受付可否が決まる
- 設定ファイル（config.txt）の値は、validate.cでセンサごとの物理的な値域内か（`LOG_LEVEL`は0〜2の範囲内か）をチェックしており、範囲外ならそのキーを無視する。ただし`STATUS_SPEED_WARN`と`STATUS_SPEED_CRIT`のような、閾値同士の大小関係（WARNがCRITより大きい等の矛盾）はチェックしていない
- ログレベル（`LOG_LEVEL`）を上げても、`config.c`自身の読み込み結果ログ（設定ファイルの有無・読み込み完了の通知）は常に表示される。表示閾値はそのconfig読み込みの結果として決まるため、config読み込み自体のログをその閾値で制御できない
- 固定値注入ファイル（fixture.txt）の値も、config.txtと同様にvalidate.cで値域チェックされる。ただし値は実行中一定の単一固定値のみで、サンプルごとの推移（シーケンス）には対応していない
- cppcheckのMISRA C:2012アドオン（`--addon=misra`）が検出する21.6/21.10（標準入出力・time.h使用制限）と15.5（単一出口）には準拠していない。前者はPC上のシミュレータという設計前提そのもの（`printf`/`fopen`/`time`を使う）と、後者はガード節（早期return）による可読性重視の設計と衝突するため、意図的に不採用としている
- 故障確定（Debounce/Degraded/Recovery、`faultmgr.c`）の状態は、DTC記録（`persist.c`）とは異なり電源再投入をまたいで保存されない（プログラム起動のたびに未確定状態から再開する）。診断コマンド（`clear`）でDTCをクリアしても、Degraded状態自体はリセットされない（独立した状態として扱う）
- 起動時自己診断（POST）は`config.txt`/`dtc_data.txt`が読み込めたかだけを見ており、内容が意味的に正しいか（値域・整合性）までは判定しない。またmain.cはUnityのテストターゲットに含められない（main関数が重複しリンクできない）ため、POSTを含むmain.c内の処理全般は自動テストの対象外で、`make run`での実行確認のみで検証している
- DTC発生回数（`DtcEntry.count`、`uint8_t`）は上限（255）を超えると飽和させず0に巻き戻る。dtc_data.txtで永続化され複数回の起動をまたいで積み上がる値のため理論上は起こりうるが、通算256回以上の発生が必要であり現実的な発生頻度は低い
- `config.txt`/`fixture.txt`の数値パース（`sscanf`の`%d`）は、値がint型の範囲を超える場合の動作を保証していない（値域チェック`validate_in_range`が働く前の段階のため）。ローカルで自分が編集する前提のファイルであり、外部・未信頼な入力を想定した対策はしていない
- CAN通信（`can.c`）は実プロセス分離・実IPC（SocketCAN等）を使わず、単一プロセス内の共有メモリ（`CanBus`）でエンジンECU役・メーターECU役を表現している。実チップ間の物理的な通信遅延やバス調停の実態は再現していない（IDは実車の優先度慣例に合わせて値を割り当てているが、実際のアービトレーションには使っていない）
- CAN受信のTimeout判定は、受信タスクの呼び出し周期が対応する送信タスクの周期とほぼ一致している前提（例: ゲージは両方1000ms）で「前回チェック時から新しいフレームが来たか」を見ており、実車のような絶対時間（経過ms）のしきい値比較ではない。受信タスクの周期を送信タスクより短くすると、送信側は正常でも受信側のチェックが空振りを繰り返すだけで確定回数（3回）に達してしまい、実際より短い時間でTimeoutと誤判定する
- CAN通信リンクの状態（`CanLinkState`、Timeout/Invalid Data確定）は、DTC記録（`persist.c`）や故障確定（`faultmgr.c`）とは異なり、電源再投入をまたいだ保存はしていない（プログラム起動のたびに`CAN_LINK_OK`から再開する）。診断コード（DTC）への反映はPhase21（`can_diag.c`）で対応済みだが、この反映用の記録（`CanDtcRecord`）自体も同様に永続化はしていない
- CAN送信タスク・受信タスクは共にイグニッションON時のみ動作するため（OFF中はECU無通電という既存の前提に合わせる）、`can_fault.txt`が無い通常の`make run`ではCANリンクのLost/Recoveryが自然に発生しない（OFF中は送受信双方が止まり、ONに戻れば直ちに新しいフレームを受信するため）。`can_fault.txt`（Phase22）で意図的にTimeoutを発生させれば`make run`でも再現できる
- CAN Fault Injection（`can_fault.c`）はTimeout（送信スキップ、`MODE=DROP`）とInvalid Data（`MODE=CORRUPT`、EngineStatusのみ対応）に対応する。FaultStatusはビットフラグのみでInvalid Data判定ロジックが無いため、`MODE=CORRUPT`+`TARGET=FAULT_STATUS`は無効な組み合わせとして注入なし扱いになる。また1回の実行で注入できるのは1メッセージ（`TARGET`）・1種類（`MODE`）のみで、`START_MS`/`END_MS`の大小関係（矛盾した指定）もチェックしていない
- `timer_get_elapsed_ms`（`timer.c`）が返す経過時間は、システム起動（OSブート）からの`CLOCK_MONOTONIC`値であり、プログラム起動からの経過時間ではない。`can_fault.c`のように「起動からN秒後」を扱いたい場合は、基準時刻を別途記録し差分を取る必要がある（`timer_is_due`のような前回値との差分計算は、この基準時刻のズレの影響を受けない）
- 警告灯データ（`CAN_MSG_FAULT_STATUS`）はビットフラグのみのため、Invalid Data判定（`validate.c`）はゲージデータ（`CAN_MSG_ENGINE_STATUS`）にのみ適用しており、警告灯データはTimeout（未受信）のみを確定・復帰の対象にしている
- Scheduler（`SCHEDULER_MAX_TASKS`=4）は、サンプル周期タスク・CAN送信タスク・CAN受信タスク2個の計4個で上限に達している。今後さらに周期タスクを追加する場合は`SCHEDULER_MAX_TASKS`の見直しが必要になる
- `timer_get_elapsed_ms`が返す経過時間（`uint32_t`のミリ秒）は、約49.7日（2^32ミリ秒）で桁あふれする。本プロジェクトの実行時間（1回数十秒）では問題にならないが、実車ECUの連続稼働のような長時間動作は想定していない。またサンプル周期の待機は、コールバックによる自動起動ではなく短い間隔（10ms）でのポーリングのため、ポーリング間隔の分だけ周期にわずかな遅延が生じうる
- Scheduler（`scheduler.c`）は優先度・オーバーラン検出を持たず、登録数も固定上限（`SCHEDULER_MAX_TASKS`、4個）で周期0ms・重複登録への防御もない最小構成。実車OSEK/AUTOSAR OSが持つoffset（複数タスクの起動タイミングを意図的にずらし、CPU/バス負荷のスパイクを避ける仕組み）も、タスクが1個のみの現状では効果が無いため実装していない
- テストカバレッジ計測（`make coverage`）は17テストターゲットそれぞれについて担当モジュール1つのgcov結果を個別に表示するのみで、プロジェクト全体を1つの数値に合算する仕組み（`lcov`等）は導入していない。共有モジュール（`debounce.c`等）が担当ターゲット以外からも間接的に使われている場合、その分の実行は数値に反映されない。`main.c`はUnityとリンクできないため計測対象に含まれない
