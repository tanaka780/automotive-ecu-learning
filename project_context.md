# プロジェクト状況

現在の実装状況とモジュール構成を記録する。学習が進むタイミングで更新する。

---

## 現在の状態

- Phase: Phase3（logger実装完了。main.cに残っていたサンプル見出しの`printf`直呼び出しもlogger経由に統一し、全モジュールの移行が完了）。Phase4候補「学習の抜け漏れ補完」は優先順位を見直し、エラーハンドリングはDTCの永続化（ファイルI/O）、文字列操作は診断コマンド入力（UDS風）とそれぞれセットで実施する方針に変更。ビット演算は外部境界（通信・永続化）ができるまで保留（daily_log/day12.md参照）
- 実装済み: センサシミュレーション基本実装、統計モジュール、アラートモジュール、センサ状態モジュール、固定幅整数型の導入（uint8_t / uint16_t / uint32_t）、DTCモジュール（diag.c、CRITICALに入った瞬間をセンサ別に検出・配列で記録、状態区分ACTIVE/HISTORY、フリーズフレーム1件記録）、logger（logger.c、可変長引数を使わずsnprintfで文字列を組み立ててからタグなし/タグ付きの関数に渡す設計。sensor→status→stats→alert→diag→main（サンプル見出し）の順に移行）
- 設計変更（Day12）: `SensorStatus`を`speed_level`/`rpm_level`/`temp_level`の個別フィールドから`SensorLevel levels[SENSOR_COUNT]`という配列に変更した。`SensorId`はdiag.hからsensor.hに移動し、`SensorStatus.levels`と`DtcRecord.entries`が同じ添字（SensorId）を共有する設計にした。これによりdiag.cが個別フィールドを一時配列へ詰め替える回避コードが不要になった
- テスト: `test/test_diag.c`（固定値データで diag.c の動作を確認、`make test`で実行）。sensor.c/stats.c/alert.c/logger.cは未テスト
- 設計メモ: sensor.c / stats.c / alert.c / status.c / diag.c は、値の更新・判定と、表示用文字列の組み立て（`snprintf`）までを担当する。実際のコンソール出力（stdoutへの書き込み）はlogger.cに一本化した（Phase3で実施済み、main.cのサンプル見出しも含め完了）。各モジュールは完成した文字列を`log_print`（タグなし）または`log_print_tagged`（タグ付き）に渡すだけで、出力方法を変える場合はlogger.cのみを修正すればよい

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
| `test/test_diag.c` | — | `diag.c`・`status.c`（読むのみ） | 固定値データを使い、diag.cの発生回数・状態区分・フリーズフレームが期待通りかを確認する |

補足: `SensorId`（センサ種別を表す識別子）は`sensor.h`で定義しており、`SensorStatus.levels`と`DtcRecord.entries`の両方が添字として共有する。

