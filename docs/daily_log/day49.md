# Day49

## 実施内容

- Phase24のフォルダ構成について、既存モジュールを層ごとに振り分けられるかを関数単位で検討し、`src/`の分割は行わないと判断した
- リポジトリ直下に並んでいたドキュメント類を`docs/`配下に集約した（README.md・CLAUDE.mdは直下に残す）

## 今回の設計方針

- PC依存の処理は、`timer.c`の`timer_get_elapsed_ms`、`logger.c`の`printf`2行、`sensor.c`・`ignition.c`の`rand`、`persist.c`・`config.c`のファイル読み書き、`cmd.c`の`cmd_read_line`、`scheduler.c`の`nanosleep`1行、`main.c`の`srand`だけだった。どのファイルもECUのロジックと同居しているため、ファイル単位の移動はやめた
- 当初の4層構成では、platformを「プラットフォーム非依存のインターフェース定義のみ」としていた。ただ、この定義だと`debounce.c`・`dtc_status.c`のようなOS非依存の共通ロジックの置き場所が無い。この点も、今フォルダを固めない理由になった
- フォルダは、実際に分ける必要が出た段階で1回だけ分ける。`fixture.c`・`can_fault.c`（PC上の検証専用）の`sim/`への分離はPython自動検証の着手時に、`src/`の共通部分とECUごとのフォルダへの分割はECU2の着手時に行う。`bsw_pc/`・`bsw_target/`は、PC依存の関数をHALとして切り出すとき（target対応時）に作る
- README.md（GitHubの入口）とCLAUDE.md（ルートでのみ自動で読み込まれる）は直下に残した

## 次回やること

- Python自動検証基盤（`docs/scenarios.md`のシナリオを投入し、期待結果と比較する結合テスト）に着手するため、Phaseとしての範囲を検討する
