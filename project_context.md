# プロジェクト状況

現在の実装状況とモジュール構成を記録する。学習が進むタイミングで更新する。

---

## 現在の状態

- Phase: Phase1〜6は完了（詳細はstudy_plan.md参照）
- 実装済み: センサシミュレーション、統計、アラート、センサ状態、固定幅整数型、DTC（記録・状態区分・フリーズフレーム）、logger、イグニッション状態管理、DTC永続化（詳細はモジュール構成表・study_plan.md参照）
- テスト: `test/test_diag.c`（固定値データでdiag.cの動作を確認）、`test/test_persist.c`（固定値データ・意図的に壊したデータでpersist.cの正常系・異常系を確認）、`test/test_stats.c`（固定値データでstats.cのmin/max/sum/countの更新を確認）、`test/test_alert.c`（標準出力キャプチャでalert.cの警告出力を確認）、`test/test_ignition.c`（標準出力キャプチャでignition.cの遷移イベント出力を確認）。いずれも`make test`で実行。結果判定・サマリ表示の共通補助関数は`test/test_common.h` / `.c`に切り出し済み（サンプル投入用の`test_feed`/`test_run_sample`は`DtcRecord`前提のため`test_diag.c`/`test_persist.c`のみで使用し、`test_stats.c`はローカルのヘルパーを使う。test_alert.c・test_ignition.cは標準出力キャプチャ用のヘルパーをそれぞれファイル内にローカルで定義している）。sensor.c/logger.cは未テスト

---

## モジュール構成

| モジュール | 書く対象 | 読む対象 | 役割 |
| --- | --- | --- | --- |
| `main.c` | 制御フローのみ | — | 初期化・ループ制御 |
| `sensor.c` | `VehicleSensorData` | — | センサ値の更新・表示 |
| `stats.c` | `VehicleStats` | `VehicleSensorData`（読むのみ） | 統計値の更新・表示 |
| `alert.c` | —（書き込みなし） | `VehicleSensorData`（読むのみ） | 閾値チェック・警告表示 |
| `status.c` | `SensorStatus` | `VehicleSensorData`（読むのみ） | センサ値の状態分類（NORMAL / WARNING / CRITICAL）と表示 |
| `diag.c` | `DtcRecord` | `SensorStatus`・`VehicleSensorData`（読むのみ） | 前回/今回のSensorStatusを比較し、CRITICALに入った瞬間をセンサ別に配列で記録・表示。状態区分（ACTIVE/HISTORY）と、最初のCRITICAL発生時のフリーズフレーム（全センサ値）も記録する |
| `logger.c` | —（書き込みなし） | —（渡された文字列のみ受け取る） | 他モジュールが組み立てた文字列をコンソールに出力する窓口。タグなし（`log_print`）とタグ付き（`log_print_tagged`）の2つを提供する |
| `ignition.c` | `Ignition` | — | イグニッション状態（OFF/ON）のランダム更新、前回との比較による遷移検出、状態の表示 |
| `persist.c` | `DtcRecord`（読み込み時のみ書く） | `DtcRecord`（保存時は読むのみ） | `DtcRecord`とテキストの相互変換、ファイルへの保存・読み込み、成功/失敗のログ表示。`previous`は保存対象に含めない |
| `test/test_diag.c` | — | `diag.c`・`status.c`（読むのみ） | 固定値データを使い、diag.cの発生回数・状態区分・フリーズフレームが期待通りかを確認する |
| `test/test_persist.c` | — | `persist.c`・`diag.c`・`status.c`（読むのみ） | 固定値データと意図的に壊したデータを使い、persist.cの保存・読み込みが正常系・異常系（ファイル無し、値不足、数値以外）で期待通りかを確認する。本番の`dtc_data.txt`とは別のテスト専用ファイル名を使う |
| `test/test_stats.c` | — | `stats.c`（読むのみ） | 固定値データを使い、stats.cのmin/max/sum/countの更新（初期値、1サンプル、複数サンプルでのmin/max更新、平均計算）が期待通りかを確認する。サンプル投入用のヘルパーはtest_common.cを使わずファイル内にローカルで定義する |
| `test/test_alert.c` | — | `alert.c`（読むのみ） | alert_checkは判定結果を構造体に書き込まず標準出力に表示するだけのため、標準出力キャプチャ（`freopen`＋`dup`/`dup2`）で出力文字列を取得し、閾値境界・単独超過・複数同時超過時の警告出力が期待通りかを確認する。キャプチャ用ヘルパーはtest_common.cを使わずファイル内にローカルで定義する |
| `test/test_ignition.c` | — | `ignition.c`（読むのみ） | ignition_checkも判定結果を構造体に書き込まず標準出力に表示するだけのため、test_alert.cと同じ標準出力キャプチャの手法で、OFF/ONの4パターン（遷移あり/なし）が期待通りかを確認する。キャプチャ用ヘルパーはtest_alert.cのものとほぼ同じ形だが、共通化はせずファイル内にローカルで定義する（重複が2箇所・15行程度に留まり、共通化には型不一致を回避する仕組みが別途必要になるため） |
| `test/test_common.c` | — | — | `test_diag.c` / `test_persist.c` / `test_stats.c`で共通のテスト補助関数（結果判定`test_check`、サマリ表示`test_summary`）を提供する。サンプル投入用の`test_feed`/`test_run_sample`は`DtcRecord`前提のため`test_diag.c`/`test_persist.c`のみで使用する |

補足: `SensorId`（センサ種別を表す識別子）は`sensor.h`で定義しており、`SensorStatus.levels`と`DtcRecord.entries`の両方が添字として共有する。

