# Day35

## 実施内容

- 21箇所：表示用snprintf（status.c/diag.c/ignition.c/sensor.c/stats.c/alert.c/config.c/persist.c/main.c）・printf（logger.c）・読み込みモードのfclose（persist.c/config.c/fixture.c）に`(void)`キャストを追加
- persist.c：`write_text_to_file`内で`fputs`/`fclose`の戻り値を確認し、どちらかが失敗したらfalseを返すよう修正（従来は戻り値を見ずに常にtrueを返していた）
- main.c：`config_load`・`persist_load_dtc`・`persist_save_dtc`の呼び出しを`if`評価に変更（本体は空、フェイルセーフとして継続する既存設計のまま動作は変えない）
- test/test_persist.c：`/dev/full`（Linux上で書き込みが必ず失敗する特殊デバイス）を使い、書き込み失敗時に`persist_save_dtc`がfalseを返すことを確認するテスト（`test_save_write_failure`）を追加

## 確認内容

- `make clean && make`で本体（`sensor_sim`）が警告なしでビルドできること
- `make test`で全9ターゲットがビルドでき、全テストがPASSすること（`/dev/full`テスト追加分でテスト関数が1件増えること）
- cppcheck（`--addon=misra --enable=all`）を再実行し、17.7の指摘が0件になること
- `make run`で通常動作（保存成功）の出力・終了コードが従来通りであること

## 実行結果

- `make`：成功、警告・エラーなし
- `make test`：9ターゲット全部ビルド成功。51テスト関数・0 Failures（day34時点の50件+`test_save_write_failure`の1件）。`test_save_write_failure`実行時は`[PERSIST] Failed to save DTC data`が実際に出力され、修正した失敗検出が機能していることを確認した
- cppcheck再実行：17.7は0件。残る21.6（12件）・21.10（1件）・12.1（4件）・18.4（1件）・15.5（41件）はday34時点から変化なし（未対応のまま）
- `make run`：終了コード0、DTC記録・保存まで従来通り正常動作

## 判定

成功。cppcheck指摘26件のうち、21件は`(void)`キャストで、残る5件はMISRA Directive 4.7を踏まえた`if`評価（うちpersist.cの1箇所は実質的な修正）で対応する方針を決め、実装した。build・test・cppcheck・実行確認の4段階いずれも期待通りで、動作・出力は変更していない。

## 今回の設計方針

- 17.7対応は、Rule 17.7（戻り値を使うか`(void)`で無視を明示する）とDirective 4.7（エラー情報は実際にテストすべき、cppcheckは検査しない）を区別して考えた。cppcheckが指摘しなくなることと、Directive 4.7の趣旨を満たすことは別の話だと気づき、26件を一括で`(void)`キャストするのではなく、値がこのプロジェクトの文脈で本当にエラー情報と言えるかを1つずつ判断した
- `config_load`・`persist_load_dtc`・`persist_save_dtc`の3箇所は、いずれも呼び出し先が既にログ出力済みで、main.c側の挙動を変える必要が無い（フェイルセーフとして継続する既存設計と一致する）ため、`if`で評価はするが本体は空、という統一した形にした
- 当初は`persist_save_dtc`の失敗時にmain()の終了コードを変える案（exit code）を検討したが、「プロセスの終了コード」自体が実車ECUのファームウェアには存在しない概念（PCシミュレータ特有）と気づき見送った。他の2箇所と扱いを揃え、動作を変えないことをPhase14全体の一貫性として優先した
- `persist.c`の`write_text_to_file`は、`fputs`/`fclose`の戻り値を見ずに常に成功扱いしていた実装上のギャップがあり、17.7対応の一環として実質的に修正した。修正した失敗検出パスを自動テストで裏付けるため、`/dev/full`を使ったテストを追加した。ただしこれは実車のNVM故障注入（実務ではドライバ/HAL層をモック化する）とは異なる、PC環境向けの簡易的な代用手段であることも認識した

## 次回やること

- 既存の設計（ガード節・標準出力・乱数生成）と衝突しうる残るルール（21.6/21.10標準ライブラリ制限・12.1括弧の明示・18.4ポインタ演算・15.5単一出口）を検討する。特に15.5（単一出口）は影響範囲が41件と広く、既存のガード節設計と衝突するため、採用・不採用・部分採用のどれにするか慎重に判断する
