# Day32

## 実施内容

- `test/test_validate.c`を新規作成し、`validate_in_range`/`validate_log_level`の境界値（speed/rpm/temp各センサの下限・上限・中間値・範囲外、不正な`SensorId`、LOG_LEVELの下限・上限・範囲外）を確認する自動テストを追加した
- `test/test_config.c`・`test/test_fixture.c`に、値域外の値がそのキーだけ無視され他の正常な値は反映されることを確認する異常系ケースを追加した
- `config.txt`/`fixture.txt`に範囲外の値（`ALERT_SPEED_MAX=200`、`SPEED=200`）を書いて`make run`を実行し、該当キーだけ無視され他のキーは反映されることを実行確認した（確認後、元の`config.txt`を復元し、確認用に作成した`fixture.txt`は削除した）
- cppcheck（`--enable=all`）を`src/`に対して実行し、指摘0件を確認した
- cppcheckのMISRA C:2012アドオン（`--addon=misra`）を試験的に実行し、単一出口(15.5)・if文の書き方(15.6/15.7)・戻り値未使用(17.7)・標準ライブラリ制限(21.6/21.10)・型/優先順位(10.4/12.1)等の指摘を確認した

## 確認内容

- `test_validate.c`追加後、`make clean && make`で警告・エラーが出ないこと
- `make test`で全テストがPASSし、既存テストへの影響が無いこと
- `config.txt`/`fixture.txt`に範囲外の値を書いた場合、そのキーだけ無視され他のキーは反映されることを`make run`で実行確認すること

## 実行結果

- `make`：成功、警告・エラーなし
- `make test`：全183件PASS（test_diag 33／test_persist 17／test_stats 28／test_alert 16／test_ignition 4／test_cmd 25／test_config 21／test_fixture 18／test_validate 21）
- `make run`：`AlertMax(speed=100,...)`（範囲外の`ALERT_SPEED_MAX=200`は無視されデフォルト値のまま）、`Speed: 0 km/h`（範囲外の`SPEED=200`は無視され初期値のまま）、`RPM: 3000`・`Temp: 60`（範囲内の値は反映）を確認した
- cppcheck（`--enable=all`）：指摘0件
- cppcheck（`--addon=misra`）：単一出口(15.5)等、複数ルールへの違反を確認（詳細はstudy_plan.md Phase14参照）

## 判定

成功。Phase12の残りタスク（タスク6・7）を予定通り完了できた。cppcheckのMISRA指摘は、想定通り通常のビルド確認・cppcheckの一般ルールでは検出できない軸であることを実際に確認できた。

## 今回の設計方針

- Unity/FreeRTOS/QP/C/Ceedling/cppcheckの5つのツールについて導入時期を検討した。cppcheckは非侵襲的（コードを変更せず外部から検査するだけ）なため独立Phase化せず随時実行する扱いとし、Unity（Phase13）・MISRA対応（Phase14）は独立Phaseとして切り出し、FreeRTOS・QP/Cはそれぞれ既存の候補テーマ「Scheduler」「State Machineの汎用化」の実現手段として将来検討する、Ceedlingは独立テーマではなくPhase13内でのUnity導入方法の選択肢として扱うことにした
- Unity試用（テストの書き方という動的検証の技術テーマ）とMISRA対応（静的解析による車載idiom準拠という別軸のテーマ）は、「テスト」という言葉でまとめられがちだが動的検証と静的解析は異なる軸であるため、Phase3（logger）・Phase6（標準出力キャプチャ）の前例に倣い、別々の独立Phaseとして扱うことにした
- MISRA対応（Phase14）は影響範囲が広く（単一出口ルールがほぼ全モジュールに影響する）、当初は1つのPhaseにまとめるべきか迷った。「1回の作業で扱うテーマを小さくする」というCLAUDE.mdの方針は「1つのPhaseの範囲」ではなく「1回の作業・1タスクの範囲」を指すと確認した上で、Phase3（logger導入をモジュールごとに段階的に移行した前例）に倣い、1つのPhase内でタスクを細かく分割する形にまとめることにした
- 既存プログラムのコーディング手法・イディオムの見直しは、プログラム全体が完成してから一括で行うのではなく、cppcheckの指摘のような具体的なきっかけが出たときに、その都度小さく対応する方針を確認した
- io層／アプリ層のようなフォルダレベルのレイヤ分割は、CAN通信・複数ECU化に着手する直前まで先送りする（study_plan.mdの既存の「ECU構造への再編成」の方針をそのまま踏襲する）

## 次回やること

- Phase13（Unity試用）に着手する：既存テストターゲット1つ（対象は着手時に判断）をUnityに移植し、自作`test_common.c`パターンと比較して採否を判断する
- Phase14（MISRA対応）はPhase13の後に着手する。まず指摘されたルールを1つずつ理解し、このプロジェクトで採用するかどうかを判断するところから始める
