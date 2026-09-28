# Day27

## 実施内容

- `logger.h`に`LogLevel`（`LOG_INFO`/`LOG_WARNING`/`LOG_ERROR`）と`log_print_leveled`を新設し、`logger.c`に表示閾値を保持する仕組み（`logger_set_level`）を実装した
- `ConfigData`に`log_level`フィールドを追加し、`config_load`の`apply_line`に`LOG_LEVEL`キー（0/1/2の数値形式）を追加した
- `main.c`の`config_load`直後に`logger_set_level(config.log_level)`を配線した
- `alert.c`（`LOG_WARNING`）、`ignition.c`/`status.c`/`diag.c`/`cmd.c`（`LOG_INFO`）、`persist.c`（保存失敗・データ破損時のみ`LOG_ERROR`、それ以外`LOG_INFO`）、`config.c`の`config_print`（`LOG_INFO`）の既存ログ呼び出しを`log_print_leveled`に置き換えた

## 確認内容

- `config.txt`に`LOG_LEVEL`が無い場合、従来通り全てのログが表示されること
- `LOG_LEVEL`を上げると、それより下位のレベルのログ（`ALERT`等）が表示されなくなること
- 既存の自動テスト（7ファイル計137件）に置き換えの影響が出ていないこと
- `config.c`自身の読み込み結果ログが、設定した閾値に関わらず常に表示されること（意図した挙動）

## 実行結果

- `make`：成功、警告・エラーなし
- `make test`：既存137件すべてPASS
- `config.txt`に`LOG_LEVEL`無し：`ALERT`含め全ログが表示された
- `config.txt`に`LOG_LEVEL=2`を追加：`ALERT`が表示されなくなり、`CONFIG`の読み込み結果ログのみ変わらず表示された

## 判定

成功。
設定したログレベルに応じて出力が抑制・表示され、既存テストも全てPASSした。期待した動作（レベル未満のログの抑制、`config.c`自身のログだけは例外）と実際の結果が一致した。

## 今回の設計方針

- プロジェクトの完成目標を「車両状態を模擬し、異常を検出・診断・記録し、電源再投入後も診断情報を保持できるECUソフトウェアシミュレータ」として明文化した。新しいテーマを選ぶ基準を、Phase1〜9の「学習順序として妥当か」から、Phase10以降は「このゴールに本当に必要か」へ移し、Phase1〜9を基礎実装、Phase10以降（Python検証基盤・CAN・Watchdog等、詳細はstudy_plan.md）をこのゴールへ向けた拡張と位置付けた。CLAUDE.md/AI_workflow.mdには新機能追加時の判断順序（①ストーリー上の必要性→②既存責務への影響→③テスト方法→④Phase化）を追加した
- リポジトリ名（automotive-ecu-learning）と整合するよう、README.mdの「プロジェクトの方向性」を「C言語を学ぶために自動車テーマを選んだ」ではなく「自動車関連プログラムを作るために個々の機能を学習順序で作ってきた」という順序に修正した
- `config.c`の`config_load`内にある2つのログ（設定ファイル有無・読み込み結果の通知）は、その`log_level`自体を決めている最中に出力されるため、`log_print_leveled`化すると自分自身がこれから決める閾値で自分の出力可否を判定する循環になる。この2箇所だけ意図的に`log_print_tagged`（無条件）のまま残した
- `PERSIST`のログレベルは当初「保存/読み込みが目的を達成できなかった場合はERROR」という基準をそのまま適用し、「初回起動でファイルが無い」もERRORにしていたが、これは異常ではなく誰にでも起きる正常な状態のため、実装後にLOG_INFOへ修正した。重要度は「達成できたか」ではなく「実際に問題が起きたか」で判断する必要があると気づいた

## 次回やること

Phase9は完了。次に着手するPhase10以降のテーマは、study_plan.mdの判断順序（①ストーリー上の必要性→②既存責務への影響→③テスト方法→④Phase化）に沿って次回改めて検討する
