# Day15

## 実施内容

- 「検証」と「test_persist.c作成」は別工程にせず、1ケースずつ「persist.hの契約から期待値を先に決める→実行して照合する」形で統合して進めた
- `test/test_persist.c`を新規作成し、次の4パターンを自動テスト化した
  - 保存→読み込みの往復一致（previousが保存対象外のためNORMALにリセットされることも含む）
  - ファイルが存在しない場合
  - 値の個数が足りない壊れたファイルの場合
  - 数値でない文字列で壊れたファイルの場合
- 「値の個数は揃っているが意味的に不正な値」は今回のテスト対象から除外し、README.mdの既知の制約に記録する方針にした
- テストが本番の`dtc_data.txt`を壊さないよう、テスト専用ファイル名`test_dtc_data.txt`を使い、テスト終了時に削除する設計にした
- `Makefile`を更新し、`make test`で`test_diag`と`test_persist`の両方をビルド・実行するようにした
- `test_diag.c`と`test_persist.c`で重複していた`check()` / `run_sample()` / `feed()` / pass-failカウントを`test/test_common.h` / `test/test_common.c`に切り出した（`test_check` / `test_run_sample` / `test_feed` / `test_summary`）

## 確認・検証

| 確認観点 | 結果 |
| --- | --- |
| ビルド（-Wall -Wextra -std=c11） | 警告・エラーなし |
| テストビルド（make test：test_diag） | 31 passed, 0 failed |
| テストビルド（make test：test_persist） | 17 passed, 0 failed |
| 本番データへの影響確認 | `dtc_data.txt`の内容が今回の作業前後で変化していないことを確認した |

## 今回の設計方針

- テスト専用ファイル名を本番ファイルと分け、テスト終了時に削除することで、テスト実行が本番の永続化データを破壊しない設計にした
- 読み込み失敗時に`dtc`が変更されないことを確認するため、`set_sentinel()`で見分けやすい値（count=42等）を設定してから`persist_load_dtc`を呼ぶ方式にした
- `check()` / `feed()`等の共通化は、`test_persist.c`を書き終えて重複が実際に目に見えてから行った（先に共通化しない）
- `test_common.c`内のpass/failカウンタは`static`のままとした。`test_diag`と`test_persist`は別々の実行ファイルなので、状態が混ざる心配はないと判断した

## 次回やること

- stats.cへの自動テスト追加（test/test_stats.c新規作成）に着手する
- alert.c/ignition.c/sensor.c/logger.cは今回は対象外とする