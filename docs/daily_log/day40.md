# Day40

## 実施内容

- 新規`include/timer.h`/`src/timer.c`を作成した：単調増加クロック（`CLOCK_MONOTONIC`）による経過時間取得（`timer_get_elapsed_ms`）と、周期判定（`timer_is_due`）を実装した
- `main.c`のサンプルループの`sleep(1)`を、`Timer`による`timer_is_due`のポーリングに置き換えた
- `test/test_timer.c`を新規作成し、自動テスト（経過時間の単調増加、周期判定の境界2パターン）を追加した
- Makefileに`timer.c`をSRCSに追加し、`test_timer`ターゲットを新設した

## 確認内容

- `make clean && make`で警告なくビルドできること
- `make test`で新規`test_timer`を含む全11ターゲットがPASSすること
- `make run`で、サンプルループが以前と同様に約1秒間隔で20サンプル分動作すること（`sleep(1)`から`timer_is_due`ポーリングに変えても、外から見た周期が変わらないこと）

## 実行結果

- `make`：成功、警告・エラーなし
- `make test`：11ターゲット全てPASS（`test_timer`は4件PASS）
- `make run`：実行時間 real 20.1秒（20サンプル×約1秒）、20サンプル分の出力を確認

## 判定

成功。`sleep(1)`の固定待ちをTimerベースの周期判定に置き換えても、既存の1秒間隔のサンプル処理という外部から見た動作は変わらないことを確認できた。

## 今回の設計方針

- Timerで実現する範囲は、経過時間取得と周期判定（`timer_is_due`）までとし、コールバックによる自動起動は見送った。既存の`main.c`の同期ループ構造を維持したまま、次のScheduler（タスク単位への整理）へ橋渡しできる最小限の機能に絞った
- 時刻取得APIは単調増加クロック（`CLOCK_MONOTONIC`）を採用した。壁時計（`time()`）と違い、システム時刻の変更（NTP同期等）の影響を受けないため
- 経過時間の型は`uint32_t`のミリ秒とした。約49.7日で桁あふれするが、本プロジェクトの実行時間（1回数十秒）では問題にならないため許容した
- `-std=c11`のままでは`clock_gettime`/`CLOCK_MONOTONIC`・`nanosleep`が見えず、POSIX機能テストマクロ（`_POSIX_C_SOURCE 200809L`）を`timer.c`/`main.c`/`test_timer.c`に追加した。当初`usleep`を使おうとしたが、POSIX.1-2008で非推奨化されておりこのマクロの下では宣言が見えなくなるため、後継の`nanosleep`に切り替えた

## 次回やること

- 次のScheduler（タスク単位への整理）を検討する
