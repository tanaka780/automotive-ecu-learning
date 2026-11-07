# Day71

## 実施内容

- フォルダ構成の再編成（ECUごとの分割）をPhase31とし、5つのタスクを決めた
- 各フォルダが何を表すかと、置くファイルを決めた
  - ECUのフォルダ（そのECUだけが使う処理）：`engine_ecu/`（`sensor`・`status`・`alert`・`stats`・`diag`・`faultmgr`・`config`・`uds`）、`meter_ecu/`（`meter`）、`fan_ecu/`（`fan_ecu`・`fan_ctrl`）
  - `common/`（2つ以上のECUが使い、特定のECUのデータの形を知らない部品）：`can`・`can_diag`・`debounce`・`dtc_status`・`validate`・`ignition`・`logger`・`timer`・`scheduler`
  - `mcal_pc/`（PCで動かすときのいちばん下のドライバ。実機ではマイコン用に差し替える）：`timer_clock`
  - `src/`直下（ECUをまたぐ配線と、ECUごとに分け切れていないファイル）：`main.c`・`cmd.c`・`persist.c`
  - `sim/`（ECUの外にある試験台。実車には無い）：今のまま
- ヘッダは`.c`と同じフォルダに置き、`test/`は今のままにする
- センサの値の型（`VehicleSensorData`・`SensorId`）を`common/sensor_types.h`に、時計の関数（`timer_get_elapsed_ms`・`timer_wait_ms`）を`mcal_pc/timer_clock.h`に分ける
- 依存は、ECUのフォルダ → `common/` → `mcal_pc/`の一方向にし、読めるフォルダを絞って`gcc -fsyntax-only`にかける`make check_deps`で確かめる（`src/`直下は対象にしない）
- Phase24で決めた名前（application・platform・bsw_pc・bsw_target）は使わず、`bsw_pc/`に当たるフォルダは`mcal_pc/`にする
- target環境への移植（Renode）は今は行わず、3段階で進める候補にした

## 今回の設計方針

- 分け方を決める前に、各ファイルの`#include`を一覧にした。ECUどうしはお互いのヘッダを読んでおらず、`uds.h`は`diag.h`と共通のヘッダだけ、`meter.h`・`fan_ecu.h`は共通のヘッダと自分のフォルダのヘッダだけを読む。共通部品がECUのヘッダを読むのは、`can.h`と`validate.h`が`sensor.h`を読む2か所だけだった
- `can.h`が`sensor.h`を読むのは、ゲージデータを送る関数の引数のためで、CANの信号の型そのもの（`CanEngineStatus`）は`can.h`にある。`validate.h`は、受信側の値域の確認に`SensorId`を使う。どちらも型だけなので、型を分ければ一方向にできる
- `timer.h`には、周期判定（`timer.c`）と、`timer_clock.c`・`sim/timer_clock_virtual.c`が中身を作る時計の関数が同居している。ドライバのヘッダはドライバの層が持ち、上の層がそれを読む（AUTOSARと同じ向き）ように分ける
- 先にECUごとに分け、ECUのフォルダには層の名前（`app/`など）を付けない。`engine_ecu/`に入る`diag.c`・`uds.c`は、AUTOSARではアプリではなくBSWのサービス層（Dem・Dcm）に当たる。一方で、エンジン監視ECUの記録の形を知っているので、共通部品にも置けない。本来は「どのECUにも使える処理」と「ECUごとの設定」に分かれるものが1つのファイルに入っている今は、層の名前を付けると中身と合わなくなる。`common/`も、`validate.c`（エンジン監視ECUの信号の値域）と`ignition.c`（半分は車の模型）を含むので、BSWとは呼べない
- ヘッダは`.c`と同じフォルダに置く。フォルダを開けば、そのECUの部品がそろって見える。`sim/`も同じ置き方をしている
- `mcal_pc/`は、ECUのソフトの一部なので`src/`の中に置く
- Makefileの`-I`には全部のフォルダを並べるので、ほかのフォルダのヘッダを読んでもビルドは通る
- `cmd.c`は、READMEとシナリオで点検コマンドとして説明しているので、`src/`直下に置く
- `test/`は、テストとモジュールが1対1で対応しているので平らのまま
- コードの側でパスを直すのは、Makefile（`src/`を含む27行と、`-I`・`HEADERS`の行）とコメント4か所。`tools/compare_can_events.sh`とシナリオ検証は実行ファイルしか見ず、カバレッジの`.gcno`の名前はファイル名だけで決まる
- 型を分ける（タスク2）のとファイルを移す（タスク3）は分ける。`sensor.h`は中身を変えたうえで移るので、別々にコミットしないと、gitが名前の変更として見つけにくい

## 次回やること

- センサの値の型を`sensor_types.h`に、時計の関数を`timer_clock.h`に分け、仮想時間の版の9条件の出力の全文と保存ファイルが、分ける前と変わらないことを確かめる
