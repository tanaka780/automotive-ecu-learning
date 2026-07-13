# プロジェクト状況

現在の実装状況とモジュール構成を記録する。学習が進むタイミングで更新する。

---

## 現在の状態

- Phase: Phase4（状態遷移＝イグニッション状態管理）に着手中。Phase1〜3は完了（study_plan.md参照）。「学習の抜け漏れ補完（エラーハンドリング／ビット演算／文字列操作）」はPhaseとして独立させず、それぞれ将来のテーマ（DTCの永続化、診断コマンド入力）とセットで実施する保留中の候補として整理した
- 実装済み: センサシミュレーション基本実装、統計モジュール、アラートモジュール、センサ状態モジュール、固定幅整数型の導入（uint8_t / uint16_t / uint32_t）、DTCモジュール（diag.c、CRITICALに入った瞬間をセンサ別に検出・配列で記録、状態区分ACTIVE/HISTORY、フリーズフレーム1件記録）、logger（logger.c、可変長引数を使わずsnprintfで文字列を組み立ててからタグなし/タグ付きの関数に渡す設計。sensor→status→stats→alert→diag→main（サンプル見出し）の順に移行）
- 設計変更（Day12）: `SensorStatus`を`speed_level`/`rpm_level`/`temp_level`の個別フィールドから`SensorLevel levels[SENSOR_COUNT]`という配列に変更した。`SensorId`はdiag.hからsensor.hに移動し、`SensorStatus.levels`と`DtcRecord.entries`が同じ添字（SensorId）を共有する設計にした。これによりdiag.cが個別フィールドを一時配列へ詰め替える回避コードが不要になった
- 設計追加: イグニッション状態管理を独立モジュール（ignition.h / ignition.c）として新規実装。`IgnitionState`（OFF/ON）を`Ignition`構造体（current/previous）で保持し、sensor.cのランダム更新パターンで状態を更新、diag.cのエッジ検出パターンで遷移した瞬間だけイベント表示する
- 連携方針（決定済み）: イグニッションOFF中は`sensor_update`/`status_check`/`diag_check`/`alert_check`/`stats_update`を全てスキップする（ECU自体が通電していない状態を再現）。main.cで`if (ignition.current == IGNITION_ON)`のブロックにまとめて実装。stats.cは呼び出された回数だけを`count`で数える設計のため、OFF中の未呼び出しは平均計算に影響しない（stats.c参照）
- 連携方針（決定済み）: OFF→ON→OFFのようにサイクルが変わっても、DTCのACTIVE/HISTORYと統計（stats）はどちらもリセットしない（通算で継続する）。DTCは実車でも電源断をまたいで保持されることが本質的な仕様であるため現状維持がそのまま適切、statsも今回は現状維持を選択した。この決定により追加のコード変更は無い（元々サイクルをまたいでも状態を保持する実装だったため）
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
| `ignition.c` | `Ignition` | — | イグニッション状態（OFF/ON）のランダム更新、前回との比較による遷移検出、状態の表示 |
| `test/test_diag.c` | — | `diag.c`・`status.c`（読むのみ） | 固定値データを使い、diag.cの発生回数・状態区分・フリーズフレームが期待通りかを確認する |

補足: `SensorId`（センサ種別を表す識別子）は`sensor.h`で定義しており、`SensorStatus.levels`と`DtcRecord.entries`の両方が添字として共有する。

