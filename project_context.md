# プロジェクト状況

現在の実装状況とモジュール構成を記録する。学習が進むタイミングで更新する。

---

## 現在の状態

- Phase: Phase1
- 実装済み: センサシミュレーション基本実装、統計モジュール、アラートモジュール、センサ状態モジュール、固定幅整数型の導入（uint8_t / uint16_t / uint32_t）

---

## モジュール構成

| モジュール | 書く対象 | 読む対象 | 役割 |
| --- | --- | --- | --- |
| `main.c` | 制御フローのみ | — | 初期化・ループ制御 |
| `sensor.c` | `VehicleSensorData` | — | センサ値の更新・表示 |
| `stats.c` | `VehicleStats` | `VehicleSensorData`（読むのみ） | 統計値の更新・表示 |
| `alert.c` | —（書き込みなし） | `VehicleSensorData`（読むのみ） | 閾値チェック・警告表示 |
| `status.c` | `SensorStatus` | `VehicleSensorData`（読むのみ） | センサ値の状態分類（NORMAL / WARNING / CRITICAL）と表示 |

