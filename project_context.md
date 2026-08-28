# プロジェクト状況

現在の実装状況とモジュール構成を記録する。学習が進むタイミングで更新する。

---

## プロジェクトの完成目標

車両状態を模擬し、センサ異常を検出・診断・記録し、電源再投入後もDTC等の診断情報を保持できるECUソフトウェアシミュレータとして完成させることを目標とする。将来的にはPythonによる自動検証（結合テスト・SILS）を組み合わせ、正常走行／故障発生／電源再投入／将来的な通信故障といった一連の車両シナリオを自動検証できる状態を目指す。

Phase1〜9はこのための基礎実装・設計基盤の構築、Phase10以降は完成目標に向けた拡張（シナリオ定義・Python検証基盤・Timer/Scheduler・CAN・Watchdog等）として位置付ける。詳細はstudy_plan.mdを参照。

---

## 現在の状態

- Phase: Phase1〜12は完了（詳細はstudy_plan.md参照）
- 実装済み: センサシミュレーション、統計、アラート、センサ状態、固定幅整数型、DTC（記録・状態区分・フリーズフレーム）、logger、イグニッション状態管理、DTC永続化、診断コマンド（`clear`によるDTC全体クリア、`clear <センサ名>`によるセンサ単位クリア、想定外入力の理由別通知、イグニッションOFF時のみの受付制限）、閾値の設定ファイル化（`config.c`。`alert.h`の3閾値+`status.h`の6閾値（計9個）をKEY=VALUE形式のファイルから読み込み、`alert_check`/`status_check`に`const ConfigData *`として渡す）、ログレベル制御（`logger.c`の`log_print_leveled`/`logger_set_level`。`config.txt`の`LOG_LEVEL`（0=INFO/1=WARNING/2=ERROR）で実行時の表示閾値を切り替え、既存のログ呼び出しをほぼ`log_print_leveled`に置き換え済み。`config.c`自身の読み込み結果ログ2箇所のみ、表示閾値が確定する前に呼ばれるため`log_print_tagged`のまま対象外とした）、固定値注入（`fixture.c`。`fixture.txt`をKEY=VALUE形式（`MODE=FIXED/RANDOM`、`SPEED`/`RPM`/`TEMP`）で読み込み、`MODE=FIXED`なら`sensor_update`のランダム生成をセンサ固定値に差し替える。ファイル無し／`MODE=RANDOM`なら従来通りランダム動作のまま）、入力値域チェック（`validate.c`。`SensorId`をインデックスにした値域テーブル（speed 0〜120／rpm 0〜6000／temp 25〜100）とLOG_LEVEL用の範囲チェック（0〜2）を提供し、config.c・fixture.cがパース直後の値を反映する前に呼び出す。範囲外の値はそのキーだけ無視される。Phase12着手中、自動テストは未追加）（詳細はモジュール構成表・study_plan.md参照）
- シナリオ定義: docs/scenarios.mdに、正常走行／故障発生／診断コマンド（DTCクリア）／電源再投入／設定ファイル異常時のフェイルセーフ（リンプホームモード）／通信故障（将来項目）の6シナリオを「目的／流れ／期待結果」の形式で整理済み（Phase10）。固定値によるシナリオ再現の仕組みはPhase11で構築済み（fixture.c）
- テスト: `test/test_diag.c`（固定値データでdiag.cの動作を確認。`status_check`の境界値確認は直接呼び出し。非デフォルト`ConfigData`での分類変化も確認）、`test/test_persist.c`（固定値データ・意図的に壊したデータでpersist.cの正常系・異常系を確認）、`test/test_stats.c`（固定値データでstats.cのmin/max/sum/countの更新を確認）、`test/test_alert.c`（標準出力キャプチャでalert.cの警告出力を確認。`alert_check`の直接呼び出し。非デフォルト`ConfigData`での警告有無の変化も確認）、`test/test_ignition.c`（標準出力キャプチャでignition.cの遷移イベント出力を確認）、`test/test_cmd.c`（固定値データで`diag_clear`・`diag_clear_sensor`・`cmd_dispatch`（全体クリア／センサ単位クリア／不正なセンサ名／余分なトークン／空白のみ）の動作を確認。標準入力を扱う`cmd_read_line`は対象外）、`test/test_config.c`（`config_load`のファイルパースを確認。正常系：全9キーが反映される、異常系：未知のキー・値欠落・数値以外の行は無視される、異常系：値域外の値は無視される、異常系：キー重複時は後勝ち）、`test/test_fixture.c`（`fixture_apply`のファイルパースを確認。正常系：`MODE=FIXED`で全キーが反映される、異常系：未知のキー・値欠落・数値以外の行は無視される・値域外の値は無視される・キー重複時は後勝ち・`MODE=RANDOM`時はセンサ値を変更しない）、`test/test_validate.c`（`validate_in_range`/`validate_log_level`の境界値を確認。speed/rpm/temp各センサの下限・上限・範囲外、不正な`SensorId`、LOG_LEVELの下限・上限・範囲外）。いずれも`make test`で実行。結果判定・サマリ表示の共通補助関数は`test/test_common.h` / `.c`に切り出し済み（サンプル投入用の`test_feed`/`test_run_sample`は`DtcRecord`前提のため`test_diag.c`/`test_persist.c`/`test_cmd.c`で使用し、`test_stats.c`はローカルのヘルパーを使う。test_alert.c・test_ignition.c・test_config.c・test_fixture.cは標準出力キャプチャ用／ファイル書き込み用のヘルパーをそれぞれファイル内にローカルで定義している）。`test_common.c`は`test_default_config()`でデフォルトの`ConfigData`（`config_init`相当）も提供し、`test_diag.c`・`test_alert.c`・`test_config.c`の直接呼び出しからも共有される。sensor.c/logger.c/cmd.c（`cmd_read_line`のみ）/config.c（`config_print`のみ）は未テスト

---

## モジュール構成

| モジュール | 書く対象 | 読む対象 | 役割 |
| --- | --- | --- | --- |
| `main.c` | 制御フローのみ | — | 初期化・ループ制御。イグニッションOFF中は主要処理をスキップするのと同じ判断軸で、診断コマンドの受付もイグニッションOFF時のみに制限する（ON時は`cmd_notify_rejected`を呼ぶ） |
| `sensor.c` | `VehicleSensorData` | — | センサ値の更新・表示 |
| `stats.c` | `VehicleStats` | `VehicleSensorData`（読むのみ） | 統計値の更新・表示 |
| `alert.c` | —（書き込みなし） | `VehicleSensorData`・`ConfigData`（共に読むのみ） | 閾値チェック・警告表示。閾値は引数の`ConfigData`から受け取る（`alert.h`のマクロは`config_init`のデフォルト値としてのみ残る） |
| `status.c` | `SensorStatus` | `VehicleSensorData`・`ConfigData`（共に読むのみ） | センサ値の状態分類（NORMAL / WARNING / CRITICAL）と表示。閾値は引数の`ConfigData`から受け取る（`status.h`のマクロは`config_init`のデフォルト値としてのみ残る） |
| `diag.c` | `DtcRecord` | `SensorStatus`・`VehicleSensorData`（読むのみ） | 前回/今回のSensorStatusを比較し、CRITICALに入った瞬間をセンサ別に配列で記録・表示。状態区分（ACTIVE/HISTORY）と、最初のCRITICAL発生時のフリーズフレーム（全センサ値）も記録する。診断コマンド（`clear`相当）によるクリア要求を受けて、記録を初期状態に戻す（`diag_clear`、内部で`diag_init`を呼び出す）。センサ単位のクリア要求（`clear <センサ名>`相当）を受けて、指定センサ1件分のエントリだけを初期状態に戻す（`diag_clear_sensor`）。フリーズフレームは、その原因が指定センサと一致する場合だけ合わせてリセットし、別センサが原因なら保持する |
| `logger.c` | —（書き込みなし。表示閾値を`static`変数で保持） | —（渡された文字列のみ受け取る） | 他モジュールが組み立てた文字列をコンソールに出力する窓口。タグなし（`log_print`）、タグ付き（`log_print_tagged`）に加え、レベル付き（`log_print_leveled`）を提供する。`logger_set_level`で設定した閾値未満の`log_print_leveled`呼び出しは出力しない |
| `ignition.c` | `Ignition` | — | イグニッション状態（OFF/ON）のランダム更新、前回との比較による遷移検出、状態の表示 |
| `persist.c` | `DtcRecord`（読み込み時のみ書く） | `DtcRecord`（保存時は読むのみ） | `DtcRecord`とテキストの相互変換、ファイルへの保存・読み込み、成功/失敗のログ表示。`previous`は保存対象に含めない |
| `cmd.c` | —（`DtcRecord`の変更は`diag_clear`／`diag_clear_sensor`経由） | 標準入力の文字列 | 標準入力から1行読み込み末尾の改行を除去する（`cmd_read_line`）。読み込んだ文字列を`strcmp`/`strncmp`で厳密に判定し、`"clear"`と完全一致なら`diag_clear`、`"clear "`に続く残りが空白を含まない1トークン（speed/rpm/temp）なら`diag_clear_sensor`を呼び出す。前後の空白はトリムしないため、`" clear"`や`"clear "`のような入力も一致せず、それ以外の想定外の入力（不正なセンサ名、余分なトークンを含む）とあわせて`[CMD]`タグで理由に応じたログ表示のみ行う（`cmd_dispatch`）。センサ名の文字列→`SensorId`変換はcmd.c内のローカル関数（`parse_sensor_name`）が担う。受付条件（イグニッションOFF時のみ）を満たさない場合に`[CMD] Not accepted (ignition ON)`を表示する`cmd_notify_rejected`も提供する（受付可否の判定自体はmain.cが行い、cmd.cはメッセージ内容のみを持つ） |
| `config.c` | `ConfigData` | 設定ファイルの文字列、`alert.h`/`status.h`のマクロ（デフォルト値として） | 閾値9個（alert.hの3つ+status.hの6つ）とログレベル（`log_level`、デフォルト`LOG_INFO`）を初期化し（`config_init`）、`KEY=VALUE`形式の設定ファイルがあれば読み込んで上書きする（`config_load`）。各キーの値は`validate.c`（`validate_in_range`/`validate_log_level`）で値域チェックしたうえで反映し、範囲外ならそのキーだけ無視する。ファイルが無い場合はデフォルト値のまま変更しない。読み込んだ値を1行でログ出力する（`config_print`、`log_print_leveled`を使用）。`main.c`が読み込んだ`ConfigData`を`alert_check`/`status_check`に渡すことで閾値判定に、`config.log_level`を`logger_set_level`に渡すことで表示閾値に反映する。`config_load`自身の読み込み結果ログ2箇所は、その時点でまだ表示閾値が確定していないため`log_print_tagged`のまま（レベル制御の対象外） |
| `fixture.c` | `VehicleSensorData`（`MODE=FIXED`の時のみ書く） | 固定値注入ファイルの文字列 | `fixture.txt`を`KEY=VALUE`形式（`MODE`/`SPEED`/`RPM`/`TEMP`）で読み込む（`fixture_apply`）。`MODE=FIXED`なら指定されたセンサ値を`validate.c`（`validate_in_range`）で値域チェックしたうえで`VehicleSensorData`に反映してtrueを返す（未指定のキー・値域外のキーは呼び出し時点の値を保持）。ファイルが無い場合・`MODE=RANDOM`の場合・`MODE`行が無い場合はfalseを返し値を変更しない。`main.c`はtrueが返れば以降`sensor_update`を呼ばない（1回の実行中は値が変化しない単一固定値のみに対応、サンプルごとの推移は対象外） |
| `validate.c` | —（書き込みなし） | —（渡された値のみ受け取る） | `SensorId`（sensor.h）をインデックスにした値域テーブル（speed 0〜120／rpm 0〜6000／temp 25〜100）を持ち、値がセンサごとの物理的な範囲内かを判定する（`validate_in_range`）。LOG_LEVEL用に0〜2の範囲チェック（`validate_log_level`）も別途提供する（SensorIdファミリーに属さないため値域テーブルとは分離）。`config.c`・`fixture.c`の両方から、パース直後のint値（uint8_t/uint16_tへキャストする前）を渡して呼ばれる |
| `test/test_diag.c` | — | `diag.c`・`status.c`（読むのみ） | 固定値データを使い、diag.cの発生回数・状態区分・フリーズフレームが期待通りかを確認する。`status_check`の境界値確認は`test_common.c`経由ではなく直接呼び出しており、`test_common.c`の`test_default_config()`を使う。非デフォルトの`ConfigData`（閾値を変更したもの）を渡したときに分類が変わることを確認するケースも直接呼び出しで持つ |
| `test/test_persist.c` | — | `persist.c`・`diag.c`・`status.c`（読むのみ） | 固定値データと意図的に壊したデータを使い、persist.cの保存・読み込みが正常系・異常系（ファイル無し、値不足、数値以外）で期待通りかを確認する。本番の`dtc_data.txt`とは別のテスト専用ファイル名を使う |
| `test/test_stats.c` | — | `stats.c`（読むのみ） | 固定値データを使い、stats.cのmin/max/sum/countの更新（初期値、1サンプル、複数サンプルでのmin/max更新、平均計算）が期待通りかを確認する。サンプル投入用のヘルパーはtest_common.cを使わずファイル内にローカルで定義する |
| `test/test_alert.c` | — | `alert.c`（読むのみ） | alert_checkは判定結果を構造体に書き込まず標準出力に表示するだけのため、標準出力キャプチャ（`freopen`＋`dup`/`dup2`）で出力文字列を取得し、閾値境界・単独超過・複数同時超過時の警告出力が期待通りかを確認する。キャプチャ用ヘルパーはtest_common.cを使わずファイル内にローカルで定義する。`capture_alert_check`は`ConfigData`を引数で受け取り、通常のケースでは`test_common.c`の`test_default_config()`を、閾値変更時の反映確認では非デフォルトの`ConfigData`を渡す |
| `test/test_ignition.c` | — | `ignition.c`（読むのみ） | ignition_checkも判定結果を構造体に書き込まず標準出力に表示するだけのため、test_alert.cと同じ標準出力キャプチャの手法で、OFF/ONの4パターン（遷移あり/なし）が期待通りかを確認する。キャプチャ用ヘルパーはtest_alert.cのものとほぼ同じ形だが、共通化はせずファイル内にローカルで定義する（重複が2箇所・15行程度に留まり、共通化には型不一致を回避する仕組みが別途必要になるため） |
| `test/test_cmd.c` | — | `diag.c`・`cmd.c`（読むのみ） | 固定値データを使い、`diag_clear`／`diag_clear_sensor`による`DtcRecord`のリセット（センサ単位クリア時のフリーズフレームの扱いを含む）と、`cmd_dispatch`の`"clear"`／`"clear <センサ名>"`／不正なセンサ名／余分なトークン／想定外文字列／空文字列／空白のみ／前後に空白が付いた入力の判定が期待通りかを確認する。標準入力を扱う`cmd_read_line`自体は対象外とし、`make run`での実行確認で扱う（入力読み取りと文字列解釈を分離した設計により、標準入力のキャプチャなしでロジックだけをテストできる） |
| `test/test_config.c` | — | `config.c`（読むのみ） | 固定値のテキストファイルを直接書き込み（`config_load`を経由せず、任意の内容のファイルを再現する）、`config_load`が正常系（全9キーを含むファイルで全フィールドに反映される）・異常系（未知のキー・値欠落・数値以外の行は無視される、値域外の値が無視される、キー重複時は後に書かれた値が採用される）で期待通りに動作するかを確認する。本番の`config.txt`とは別のテスト専用ファイル名（`test_config.txt`）を使う |
| `test/test_fixture.c` | — | `fixture.c`（読むのみ） | 固定値のテキストファイルを直接書き込み（`fixture_apply`を経由せず、任意の内容のファイルを再現する）、`fixture_apply`が正常系（`MODE=FIXED`で全キーが反映される）・異常系（未知のキー・値欠落・数値以外の行は無視される、値域外の値が無視される、キー重複時は後勝ち、`MODE=RANDOM`時はfalseを返しセンサ値を変更しない）で期待通りに動作するかを確認する。本番の`fixture.txt`とは別のテスト専用ファイル名（`test_fixture.txt`）を使う |
| `test/test_validate.c` | — | `validate.c`（読むのみ） | 固定値で`validate_in_range`・`validate_log_level`を直接呼び出し、speed/rpm/temp各センサの下限・上限・中間値・範囲外、不正な`SensorId`（`SENSOR_COUNT`）、LOG_LEVELの下限・上限・範囲外を確認する。ファイルI/Oを使わず関数を直接呼ぶ点が`test_config.c`/`test_fixture.c`と異なる |
| `test/test_common.c` | — | `config.c`（`config_init`のみ読むのみ） | `test_diag.c` / `test_persist.c` / `test_stats.c` / `test_cmd.c`で共通のテスト補助関数（結果判定`test_check`、サマリ表示`test_summary`）を提供する。サンプル投入用の`test_feed`/`test_run_sample`は`DtcRecord`前提のため`test_diag.c`/`test_persist.c`/`test_cmd.c`で使用する。デフォルトの`ConfigData`を1回だけ生成して返す`test_default_config()`も提供し、`test_run_sample`内部に加え`test_diag.c`・`test_alert.c`・`test_config.c`の直接呼び出しからも共有される |

補足: `SensorId`（センサ種別を表す識別子）は`sensor.h`で定義しており、`SensorStatus.levels`と`DtcRecord.entries`の両方が添字として共有する。

