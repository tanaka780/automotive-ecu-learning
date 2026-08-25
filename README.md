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

Phase1〜9はこの完成目標に向けた基礎実装・設計基盤の構築期間として位置付ける。Phase10以降の方向性は study_plan.md を参照。代表的な車両シナリオ（正常走行／故障発生／診断コマンド／電源再投入／設定ファイル異常時のフェイルセーフ／通信故障）は docs/scenarios.md を参照。

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

テスト（diag.c・persist.c・stats.c・alert.c・ignition.c・cmd.c・config.c・fixture.c の動作確認、固定値データ使用）:

```bash
make test
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

---

## モジュール構成

| モジュール | 役割 |
| --- | --- |
| `main.c` | 初期化・ループ制御 |
| `sensor.c` | センサ値の更新・表示 |
| `stats.c` | 統計値の更新・表示 |
| `alert.c` | 閾値チェック・警告表示 |
| `status.c` | センサ値の状態分類（NORMAL / WARNING / CRITICAL）と表示 |
| `diag.c` | DTC（故障診断コード）の記録・表示、フリーズフレームの記録、全体クリア（`diag_clear`）とセンサ単位クリア（`diag_clear_sensor`） |
| `logger.c` | 出力の窓口の一元化。タグなし（`log_print`）・タグ付き（`log_print_tagged`）に加え、レベル付き（`log_print_leveled`）で`logger_set_level`が設定した表示閾値未満のログを抑制する |
| `ignition.c` | イグニッション状態（OFF/ON）の更新・遷移検出・表示 |
| `persist.c` | DTC記録のファイルへの保存・読み込み、成功/失敗のログ表示 |
| `cmd.c` | 標準入力から診断コマンド（`clear`／`clear <センサ名>`相当）を読み込み、解釈してDTC記録のクリア（全体／センサ単位）を要求する。想定外の入力は理由に応じて区別して通知する。イグニッションOFF時のみという受付条件を満たさない場合の通知（`cmd_notify_rejected`）も担う |
| `config.c` | 閾値9個（alert.c/status.c）とログレベル（`LOG_LEVEL`）の設定値を保持し、`KEY=VALUE`形式の設定ファイルから読み込む（ファイルが無ければデフォルト値のまま）。読み込んだ値はalert.c/status.cの判定、およびlogger.cの表示閾値に反映される |
| `fixture.c` | `fixture.txt`を`KEY=VALUE`形式で読み込む。`MODE=FIXED`なら`SPEED`/`RPM`/`TEMP`をセンサ値に反映し、`main.c`はそれ以降の`sensor_update`呼び出しをスキップする。`MODE=RANDOM`・ファイル無し・`MODE`未指定の場合は何もせず、従来通りランダムに動作する |
| `test/test_diag.c` | 固定値データによる diag.c の動作確認（`make test`で実行） |
| `test/test_persist.c` | 固定値データ・意図的に壊したデータによる persist.c の正常系・異常系の動作確認（`make test`で実行） |
| `test/test_stats.c` | 固定値データによる stats.c の動作確認（`make test`で実行）。サンプル投入用のヘルパーは test_common.c を使わずファイル内にローカルで定義 |
| `test/test_alert.c` | 標準出力キャプチャ（`freopen`＋`dup`/`dup2`）による alert.c の動作確認（`make test`で実行）。閾値境界・単独超過・複数同時超過時の警告出力を確認する |
| `test/test_ignition.c` | 標準出力キャプチャ（`freopen`＋`dup`/`dup2`）による ignition.c の動作確認（`make test`で実行）。OFF/ONの4パターン（遷移あり/なし）で、遷移した瞬間だけイベントが出力されることを確認する |
| `test/test_cmd.c` | 固定値データによる `diag_clear`・`diag_clear_sensor`・`cmd_dispatch`（全体クリア／センサ単位クリア／不正なセンサ名／余分なトークン／空白のみ等）の動作確認（`make test`で実行）。標準入力を扱う `cmd_read_line` は対象外（`make run`での実行確認で扱う） |
| `test/test_config.c` | 固定値データによる `config_load` のファイルパース動作確認（`make test`で実行）。正常系（全9キーの反映）、異常系（未知のキー・値欠落・数値以外の行は無視される、キー重複時は後勝ち）を確認する |
| `test/test_fixture.c` | 固定値データによる `fixture_apply` のファイルパース動作確認（`make test`で実行）。正常系（`MODE=FIXED`で全キーの反映）、異常系（未知のキー・値欠落・数値以外の行は無視される、キー重複時は後勝ち、`MODE=RANDOM`時はセンサ値を変更しない）を確認する |
| `test/test_common.c` | test_diag.c / test_persist.c / test_stats.c / test_cmd.c で共通のテスト補助関数（結果判定・サマリ表示）を提供する。サンプル投入用の関数（DtcRecord前提）は test_diag.c / test_persist.c / test_cmd.c のみで使用する。デフォルトの`ConfigData`を返す`test_default_config()`も提供し、test_diag.c・test_alert.c・test_config.cの直接呼び出しからも共有される |

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

---

## 既知の制約

- 実車ECUや実CAN通信とは接続していない
- センサ値は学習用の簡易モデルであり、実車の物理モデルではない
- DTC永続化データの読み込みは、値の個数が正しければ読み込み成功と判定しており、個々の値が意味的にありえる範囲かまではチェックしていない
- 診断コマンドは`clear`／`clear <センサ名>`（speed/rpm/temp）のみに対応しており、大文字小文字を区別し前後の空白もトリムしない完全一致判定。拒否理由の区別も「不正なセンサ名」「イグニッションON中で受付不可」「それ以外の想定外コマンド」の3種類にとどまる。他のUDS診断サービスは再現していない
- 診断コマンドの受付タイミングはサンプルループ終了後の1回のみで変わっていない。イグニッションOFFへ遷移した瞬間を検出して受け付ける（実際の駐車中スキャンツール接続に近い挙動）わけではなく、ループ終了時点の状態がたまたまOFFかどうかで受付可否が決まる
- 設定ファイル（config.txt）の値は、数値に変換できればそのまま型に代入しており、意味的に妥当な範囲か（例えば`uint8_t`の範囲を超えていないか）はチェックしていない。`LOG_LEVEL`も同様に0〜2以外の数値を範囲チェックしていない
- ログレベル（`LOG_LEVEL`）を上げても、`config.c`自身の読み込み結果ログ（設定ファイルの有無・読み込み完了の通知）は常に表示される。表示閾値はそのconfig読み込みの結果として決まるため、config読み込み自体のログをその閾値で制御できない
- 固定値注入ファイル（fixture.txt）の値も、config.txtと同様に範囲チェックをしていない（型の範囲を超えていないかはチェックしない）。また値は実行中一定の単一固定値のみで、サンプルごとの推移（シーケンス）には対応していない
