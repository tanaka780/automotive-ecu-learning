# プロジェクト状況

現在の実装状況とモジュール構成を記録する。学習が進むタイミングで更新する。

---

## 現在の状態

- Phase: Phase5（DTCの永続化＝ファイルI/O）に着手中。Phase1〜4は完了、Phase5は永続化の実装が完了、エラーハンドリングの検証とテスト整備が未完了（詳細はstudy_plan.md参照）
- 実装済み: センサシミュレーション、統計、アラート、センサ状態、固定幅整数型、DTC（記録・状態区分・フリーズフレーム）、logger、イグニッション状態管理、DTC永続化（詳細はモジュール構成表・study_plan.md参照）
- テスト: `test/test_diag.c`（固定値データでdiag.cの動作を確認、`make test`で実行）。sensor.c/stats.c/alert.c/logger.c/ignition.c/persist.cは未テスト（`test/test_persist.c`はstudy_plan.md Phase5のタスクとして未着手）

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

補足: `SensorId`（センサ種別を表す識別子）は`sensor.h`で定義しており、`SensorStatus.levels`と`DtcRecord.entries`の両方が添字として共有する。

