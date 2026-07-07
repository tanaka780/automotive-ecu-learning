# プロジェクト状況

現在の実装状況とモジュール構成を記録する。学習が進むタイミングで更新する。

---

## 現在の状態

- Phase: Phase2（拡張バックログ完了、次はPhase3の着手判断待ち）
- 実装済み: センサシミュレーション基本実装、統計モジュール、アラートモジュール、センサ状態モジュール、固定幅整数型の導入（uint8_t / uint16_t / uint32_t）、DTCモジュール（diag.c、CRITICALに入った瞬間をセンサ別に検出・配列で記録、状態区分ACTIVE/HISTORY、フリーズフレーム1件記録）
- テスト: `test/test_diag.c`（固定値データで diag.c の動作を確認、`make test`で実行）
- 設計メモ: sensor.c / stats.c / alert.c / status.c / diag.c は、いずれも値の更新・判定などの本来の責務に加えて、コンソール表示（`*_print()`）も自分で担っている。表示（出力）を独立した責務として切り出すかはPhase3候補「logger」で検討する（study_plan.md参照）

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
| `test/test_diag.c` | — | `diag.c`・`status.c`（読むのみ） | 固定値データを使い、diag.cの発生回数・状態区分・フリーズフレームが期待通りかを確認する |

