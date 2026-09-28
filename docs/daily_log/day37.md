# Day37

## 実施内容

- `include/faultmgr.h`/`src/faultmgr.c`を作成し、センサ別にDebounce（CRITICALが3回連続で確定）・Degraded mode（確定後はフェイルセーフ値に差し替えて動作継続）・Recovery（NORMALが3回連続で復帰）を実装した
- `main.c`を修正：`diag_check`の後に`faultmgr_check`を呼び、`faultmgr_apply_safe_values`で作った`effective_data`を`alert_check`/`stats_update`に渡すよう配線した
- `test/test_faultmgr.c`を新規作成し、Debounce/Recoveryの境界・連続の途切れによるカウント数え直し・複数センサの独立性・raw非破壊を確認するテストを追加

## 確認内容

- `make clean && make`で警告なしにビルドできること
- `make test`で全10ターゲット・61テスト関数がPASSすること
- cppcheck（`--addon=misra`）でfaultmgr.cを確認し、新規ルールの指摘が無いこと
- `make run`で、Degraded確定後に`[ALERT]`が出なくなる（フェイルセーフ値により閾値を下回る）ことを実際の動作で確認すること

## 実行結果

- `make`：成功、警告・エラーなし
- `make test`：10ターゲット全部ビルド成功、61テスト関数、0 Failures（`test_faultmgr`は新規10件を含む）
- cppcheck（`--addon=misra`）：新規に10.4（`uint8_t`のカウンタと`int`リテラルの比較）を検出したため、`FAULTMGR_DEBOUNCE_COUNT`/`FAULTMGR_RECOVERY_COUNT`を符号なしリテラル（`3U`）に修正して解消した。残る15.5（単一出口）・21.6（標準入出力）は既存モジュールと同じ理由で不採用のまま
- `make run`：一時的に`fixture.txt`でSpeedを110固定にし、デフォルト閾値のもとで確認した。3サンプル目からCRITICALが継続し、Debounce確定のタイミングで`[FAULT] Degraded: Speed`が出力され、以降`[ALERT] Speed`が出なくなることを確認した。確認後、一時作成した`fixture.txt`は削除し、リポジトリに存在していた既存の`config.txt`（低い閾値が書かれたローカルファイル）は退避→復元して元の状態に戻した

## 判定

成功。期待通りDegraded確定後に`[ALERT]`が止まる動作を自動テスト・`make run`の両方で確認でき、実際の挙動と一致した。

## 今回の設計方針

- フェイルセーフ値は`diag_check`より後・`alert_check`/`stats_update`より前でraw値のコピーにのみ適用し、`status_check`/`diag_check`のDTC判定はraw値のまま保つ設計にした
- 将来のCAN異常処理への転用を見据えた汎用形にするため、縮退動作は`diag.c`ではなく新規`faultmgr.c`に切り出した

## 次回やること

- 保留中の候補（起動時自己診断／DTO整理／Timer等）から次のテーマを判断する
