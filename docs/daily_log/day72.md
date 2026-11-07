# Day72

## 実施内容

- `include/sensor_types.h`を作り、センサの値の型（`VehicleSensorData`・`SensorId`）を`sensor.h`から移した。`sensor.h`はこれを読み、センサの処理の宣言だけを持つ
- `include/timer_clock.h`を作り、時計と待機の関数の宣言を`timer.h`から移した。`timer.h`はこれを読み、`src/timer_clock.c`・`sim/timer_clock_virtual.c`は`timer_clock.h`だけを読む
- `include/can.h`・`include/validate.h`が、`sensor.h`ではなく`sensor_types.h`を読むようにした
- `SensorId`の置き場所を書いていたコメント（`validate.c`・`status.h`・`faultmgr.h`・`can.h`・`validate.h`）を`sensor_types.h`に直した

## 確認内容

- 警告0件のままビルドでき、単体テストがすべて通るか
- 型と宣言を移しただけなので、仮想時間の版の出力の全文と保存ファイルが、分ける前（HEAD）と1文字も変わらないか
- 共通部品にする予定の`.c`が、共通部品のヘッダと`timer_clock.h`だけが見える状態でコンパイルできるか（分ける前はできず、分けた後はできるか）
- MISRAのチェックで、指摘が増えていないか
- シナリオ検証が、すべて通ったままか

## 実行結果

| 確認 | 結果 |
| --- | --- |
| `make clean && make` | 警告0件 |
| `make test` | 21ターゲット・209テスト全てPASS |
| 分ける前（HEAD）と後の比べ（仮想時間の版、`compare_can_events.sh`と同じ8条件＋点検コマンドの`clear`を入れた1条件） | 9条件とも、出力の全文と`dtc_data.txt`・`meter_dtc.txt`・`fan_dtc.txt`が一致。分ける前どうしで2回実行しても一致 |
| 共通部品の`.h`と`timer_clock.h`だけを別のフォルダに写し、そこだけを`-I`にして`gcc -fsyntax-only` | 分ける前：`can.c`・`can_diag.c`・`validate.c`の3つが通らない（`sensor.h`が見つからない）。分けた後：9つとも通り、`timer_clock.c`は`timer_clock.h`だけで通る |
| cppcheck（`--addon=misra`、`can.c`・`validate.c`・`timer.c`・`timer_clock.c`・`sensor.c`・`sim/timer_clock_virtual.c`） | 分ける前と同じ件数・同じルール |
| `make scenario` | 106テスト全てOK（約134秒） |

## 判定

期待通り。

- 移したのは型と宣言だけで、出力・保存ファイル・テストの結果は分ける前と変わらなかった。比べ方自体が揺れないことも、分ける前どうしの一致で確かめた
- 共通部品の`.c`は、分ける前は3つが通らず、分けた後は全部通った。共通部品がエンジン監視ECUのヘッダを読まなくなったことが、両方の向きで確かめられた
- `can_diag.c`が通らなかったのは、`can_diag.h`が`can.h`を読み、`can.h`が`sensor.h`を読んでいたため。直したのは`can.h`の1行だけで済んだ

## 今回の設計方針

- `sensor.h`と`timer.h`は、分けたヘッダを中で読むようにした。どちらかを読んでいたファイルは、`#include`を変えなくてもそのまま使える
- コンパイルの確かめは、今日はファイルを写して手で行った。`make check_deps`として残すのは、フォルダを分けた後にする

## 次回やること

- `.c`と`.h`を、ECUごとのフォルダ・`common/`・`mcal_pc/`へ移し、パスを直す
- 移した後も、仮想時間の版の9条件の出力の全文と保存ファイルが、移す前と変わらないことを確かめる
