# Day17

## 実施内容

- `test/test_alert.c`を新規作成し、alert.cの自動テストを追加した
- `alert_check`は判定結果を構造体に書き込まず標準出力に表示するだけのため、diag.c/persist.c/stats.cで使ってきた「構造体の中身をcheck()で確認する」手法がそのまま使えなかった。そこで標準出力キャプチャ（`freopen`＋`dup`/`dup2`）を導入し、出力された文字列そのものをcheck()で確認する方式にした
- 次の5パターンを自動テスト化した
  - speed/rpm/tempの3項目とも閾値ちょうど（境界値）では警告が出ないこと
  - speedのみ閾値超過のとき、Speedの警告だけが出ること
  - rpmのみ閾値超過のとき、RPMの警告だけが出ること
  - temperatureのみ閾値超過のとき、Tempの警告だけが出ること
  - 3項目同時に閾値超過したとき、3件とも警告が出ること
- キャプチャ用のヘルパー（`capture_alert_check`）はtest_common.h/.cを変更せず、test_stats.cの`feed()`と同じ方針でtest_alert.c内にローカルで定義した
- `Makefile`を更新し、`TEST_ALERT_SRCS`/`TEST_ALERT_TARGET`を追加、`test`ターゲットと`clean`ターゲットに`test_alert`を組み込んだ

## 確認・検証

| 確認観点 | 結果 |
| --- | --- |
| ビルド（-Wall -Wextra -std=c11） | 警告・エラーなし |
| テストビルド（make test：test_diag） | 31 passed, 0 failed |
| テストビルド（make test：test_persist） | 17 passed, 0 failed |
| テストビルド（make test：test_stats） | 28 passed, 0 failed |
| テストビルド（make test：test_alert） | 13 passed, 0 failed |
| 本番データへの影響確認 | `dtc_data.txt`の内容が今回の作業前後で変化していないことを確認した |
| キャプチャ用一時ファイルの後始末確認 | `test_alert_capture.txt`がテスト終了後に残っていないことを確認した |

## 今回の設計方針

- `freopen`だけでは標準出力を元の端末に戻す手段が残らないため、キャプチャ前に`dup(STDOUT_FILENO)`で元のfdを退避しておき、キャプチャ後に`dup2`で復元する設計にした。これにより、キャプチャ対象の`alert_check`呼び出し以外（PASS/FAIL表示やサマリ表示）は通常通り画面に出力される
- `freopen`が失敗した場合（規格上その時点でstdoutは既に閉じられる）は、先に`dup2`で復元してから空文字列扱いにする分岐を入れた
- 一時キャプチャファイル（`test_alert_capture.txt`）は、test_persist.cのようにmain()の最後でまとめて削除するのではなく、`capture_alert_check`の呼び出し内で毎回読み込み直後に削除する設計にした。1回のテストケースごとにファイルを使い捨てる構造のため、都度後片付けする方が自然と判断した
- Makefileのソース構成はtest_stats.cと同様、test_common.cがstatus_check/diag_checkを参照するため、直接は使わないstatus.c/diag.cもリンク対象に含めた

## 次回やること

- Phase6の残りとして、`test/test_ignition.c`を新規作成し、`ignition_check`が遷移した瞬間だけイベントを表示し、遷移していないときは表示しないことを確認する
