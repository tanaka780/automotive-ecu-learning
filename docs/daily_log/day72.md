# Day72

## 実施内容

- `include/sensor_types.h`を作り、センサの値の型（`VehicleSensorData`・`SensorId`）を`sensor.h`から移した。`sensor.h`はこれを読み、センサの処理の宣言だけを持つ
- `include/timer_clock.h`を作り、時計と待機の関数の宣言を`timer.h`から移した。`timer.h`はこれを読み、`src/timer_clock.c`・`sim/timer_clock_virtual.c`は`timer_clock.h`だけを読む
- `include/can.h`・`include/validate.h`が、`sensor.h`ではなく`sensor_types.h`を読むようにした
- `SensorId`の置き場所を書いていたコメント（`validate.c`・`status.h`・`faultmgr.h`・`can.h`・`validate.h`）を`sensor_types.h`に直した
- `src/`の`.c`と`include/`の`.h`を、中身を変えずに`src/engine_ecu/`・`src/meter_ecu/`・`src/fan_ecu/`・`src/common/`・`src/mcal_pc/`と`src/`直下へ移した（`include/`は無くなった）
- ファイルのパスを書いていたコメント（`timer.c`・`timer_clock.h`・`sim/timer_clock_virtual.c`・`test/`の3つ）を、移した後のパスに直した

## 確認内容

- 警告0件のままビルドでき、単体テストがすべて通るか
- 型と宣言を移しただけなので、仮想時間の版の出力の全文と保存ファイルが、分ける前（Day71のコミット）と1文字も変わらないか
- 共通部品にする予定の`.c`が、共通部品のヘッダと`timer_clock.h`だけが見える状態でコンパイルできるか（分ける前はできず、分けた後はできるか）
- MISRAのチェックで、指摘が増えていないか
- シナリオ検証が、すべて通ったままか
- ファイルを移した後も、警告・単体テスト・カバレッジの割合・MISRAの指摘が、移す前（タスク2のコミット）と同じか
- 移した後も、仮想時間の版の出力の全文と保存ファイル、実時間の版のCANの出来事の並びが、移す前と変わらないか
- 依存の向きの確かめ（`make check_deps`）が今のコードで通り、確かめたファイルの数が種類ごとに出るか
- わざと決まりに反するものを入れると落ち、正しい書き方（ヘッダの中の`extern`）は止めないか
- 決まりに反した状態では`make`だけが止まり、`make test`は動くか

## 実行結果

ヘッダを分けたとき（タスク2）

| 確認 | 結果 |
| --- | --- |
| `make clean && make` | 警告0件 |
| `make test` | 21ターゲット・209テスト全てPASS |
| 分ける前（Day71のコミット）と後の比べ（仮想時間の版、`compare_can_events.sh`と同じ8条件＋点検コマンドの`clear`を入れた1条件） | 9条件とも、出力の全文と`dtc_data.txt`・`meter_dtc.txt`・`fan_dtc.txt`が一致。分ける前どうしで2回実行しても一致 |
| 共通部品の`.h`と`timer_clock.h`だけを別のフォルダに写し、そこだけを`-I`にして`gcc -fsyntax-only` | 分ける前：`can.c`・`can_diag.c`・`validate.c`の3つが通らない（`sensor.h`が見つからない）。分けた後：9つとも通り、`timer_clock.c`は`timer_clock.h`だけで通る |
| cppcheck（`--addon=misra`、`can.c`・`validate.c`・`timer.c`・`timer_clock.c`・`sensor.c`・`sim/timer_clock_virtual.c`） | 分ける前と同じ件数・同じルール |
| `make scenario` | 106テスト全てOK（約134秒） |

ファイルを移したとき（タスク3）

| 確認 | 結果 |
| --- | --- |
| `make clean && make` | 警告0件 |
| `make test` | 21ターゲット・209テスト全てPASS |
| `make coverage`（移す前と後） | 21ターゲットの割合が全て同じ（出力42行が一致） |
| cppcheck（`--addon=misra`、`src/`と`sim/`の全ての`.c`、移す前と後） | 同じ件数・同じルール |
| 移す前と後の比べ（仮想時間の版、上と同じ9条件） | 9条件とも、出力の全文と保存ファイル3つが一致。移す前どうしで2回実行しても一致 |
| `tools/compare_can_events.sh`（実時間の版、8条件×3回、移す前と後） | 8条件とも一致。同じ条件の3回どうしも一致 |
| `make scenario` | 106テスト全てOK（約134秒） |

依存の向きの確かめ（タスク4）

| 確認 | 結果 |
| --- | --- |
| `make clean && make`（今のコード） | `check_deps: mcal 2個、common 19個、ECU 3フォルダ 22個のファイルを確かめた`・`OK`の後に実行ファイルを作る。警告0件 |

わざと決まりに反するものを、作業用のコピーに1つずつ入れて`tools/check_deps.sh`を動かした結果

| 入れたもの | 結果 |
| --- | --- |
| `common/can.c`で`status.h`を読む（共通部品からECU） | 落ちた（`status.h`が見つからない） |
| `meter_ecu/meter.c`で`fan_ctrl.h`を読む（ECUから別のECU） | 落ちた |
| `mcal_pc/timer_clock.c`で`timer.h`を読む（MCALから上の層） | 落ちた |
| `common/validate.h`で`config.h`を読む（共通部品のヘッダからECU） | 落ちた |
| `engine_ecu/uds.c`で`persist.h`を読む（ECUから`src/`直下） | 落ちた |
| `meter_ecu/meter.c`で`fixture.h`を読む（ECUから試験台） | 落ちた |
| `common/can.c`で、ヘッダを読まずに`status_print`を呼ぶ | 落ちた（宣言の無い呼び出しのエラー） |
| `common/can.c`に`extern`で`status_print`の宣言を書く | 落ちた（`grep`） |
| `common/logger.c`で`../engine_ecu/status.h`を読む | 落ちた（`grep`） |
| `src/brake/`（名前が`_ecu`で終わらない）を作り、`meter.h`を読む | 落ちた（ECUとして扱われた） |
| `src/mcal_dummy/`を作り、`logger.h`を読む | 落ちた（MCALとして扱われた） |
| `src/common/`の名前を変え、共通部品として探せなくする | 落ちた（`common 0個`の表示と、0個の種類があるという失敗） |
| `common/logger.h`に`extern`の変数の宣言を書く | 通った（止めない） |
| 2つの違反を同時に入れる | 2つとも表示してから落ちた |
| 1つ目の違反を入れたまま`make`と`make test` | `make`は止まり実行ファイルを作らない。`make test`は動き、209テスト全てPASS |

## 判定

期待通り。

- 移したのは型と宣言だけで、出力・保存ファイル・テストの結果は分ける前と変わらなかった。比べ方自体が揺れないことも、分ける前どうしの一致で確かめた
- 共通部品の`.c`は、分ける前は3つが通らず、分けた後は全部通った。共通部品がエンジン監視ECUのヘッダを読まなくなったことが、両方の向きで確かめられた
- `can_diag.c`が通らなかったのは、`can_diag.h`が`can.h`を読み、`can.h`が`sensor.h`を読んでいたため。直したのは`can.h`の1行だけで済んだ
- ファイルを移した後も、ビルド・テスト・カバレッジ・MISRA・出力の全文・CANの出来事の並び・シナリオ検証が、移す前と全て同じだった。移したのは置き場所だけで、動きは変わっていない
- 依存の向きの確かめは、今のコードで通り、入れた14通りのうち落ちるべき13通りが落ち、止めてはいけない1通り（ヘッダの`extern`）は通った。決まりに反しても単体テストは動き、`make`だけが止まった

## 今回の設計方針

- `sensor.h`と`timer.h`は、分けたヘッダを中で読むようにした。どちらかを読んでいたファイルは、`#include`を変えなくてもそのまま使える
- `#include`の行は1つも変えていない。ヘッダは名前で探すので、Makefileの`-I`に新しいフォルダを並べるだけで足りた
- 実時間の版の比べと`make scenario`は、同時に動かさず順に行った。CPUを取り合うと、実時間の版の出力が揺れるため
- 依存の向きは、読んでよいフォルダだけを`-I`に渡して、`.c`と`.h`を1つずつ`gcc -fsyntax-only`にかけて確かめる。どのヘッダを読んでいるかを、コンパイラ自身が判定するため
- 宣言の無い関数の呼び出しは`-Werror=implicit-function-declaration`でエラーにし、`.c`の中の`extern`と`../`の`#include`は、このプロジェクトで使わない書き方なので`grep`で止める。`extern`はヘッダに書くのは正しい書き方なので、`.c`だけを見る
- フォルダは、`common/`は共通部品、`mcal_`で始まるものはMCAL、それ以外は全部ECUとして扱う。ECUの名前を並べないので、フォルダを足しても確かめから漏れない。ECUではない層を足すと落ちるので、そのときに分け方を直すことになる
- `make`（`all`）の前に走らせ、`make test`には入れない。依存の向きはビルドの決まりで、決まりに反しても動きの確かめ（単体テスト）は動かせるようにするため
- 中身は`tools/check_deps.sh`に書き、Makefileは呼ぶだけにした。普通のシェルスクリプトとして読める
- 違反は全部表示してから最後に失敗で終わる。1つずつ直して打ち直さなくて済む

## 次回やること

- 次に取り組むテーマを決める（保留中の候補のWatchdogなど）
