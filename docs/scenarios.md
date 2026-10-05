# 車両シナリオ

ECUソフトウェアシミュレータとして完成させるために必要な代表シナリオを整理する。
各シナリオは「担当ECU／目的／流れ／期待結果」の4項目で書く。ファン制御ECUに関する内容は未実装。

---

## ECU構成

| ECU | 役割 | 現状 |
| --- | --- | --- |
| エンジン監視ECU | センサ値の更新・状態判定・DTC記録・Fail-safe、ゲージデータ・警告灯データの送信 | 実装済み（3ECU化でも無改修で流用する） |
| メーターECU | 受信・表示・通信異常の検知と記録 | 実装済み（エンジン監視ECUと同一プロセス内の「メーターECU役」） |
| ファン制御ECU | 受信した水温から冷却ファンのON/OFFを判断し、ファン状態を送信する | 未実装 |

---

## 正常走行

担当ECU：エンジン監視ECU・メーターECU・ファン制御ECU

目的：センサ値が正常範囲内なら、警告もDTCも発生しないことを確認する

流れ：
1. Ignition ON
2. センサ値が正常範囲で複数回更新される
3. Ignition OFF

期待結果：
- 状態表示は常にNORMAL
- 警告ログは出ない
- DTCの発生回数は0のまま
- メーターECUは`[METER] EngineStatus[OK] ...`で受信値を表示し、`[METER] FaultStatus[OK] speed=0 rpm=0 temp=0`（確定異常なし）を表示する
- ファン制御ECUは、水温がON閾値未満のままならファンOFFを維持する（未実装）

---

## 故障発生

担当ECU：エンジン監視ECU・メーターECU

目的：センサ値が閾値（CRITICAL）を超えたときに、警告・DTC記録・フリーズフレーム・故障確定（Degraded）が正しく行われることを確認する

流れ：
1. Ignition ON
2. センサ値が正常範囲で数回更新される
3. いずれかのセンサ値がCRITICALの閾値を超える（3回以上連続する）
4. センサ値が正常範囲に戻る（3回以上連続する）
5. Ignition OFF

期待結果：
- 閾値を超えた瞬間、`[ALERT]`警告ログが出力される
- 状態表示がCRITICALになる
- 該当センサのDTC発生回数が1件記録され、状態区分がACTIVEになる
- 最初にCRITICALが発生した瞬間の全センサ値がフリーズフレームとして1件記録される
- 複数センサが同時に閾値を超えた場合、両方ともDTCとして記録される（フリーズフレームの原因は配列の並び順(speed→rpm→temp)が早い方になる）
- CRITICALが3回連続すると`[FAULT] Degraded: <センサ名>`が出力され、以降の警告・統計・CAN送信値はそのセンサだけフェイルセーフ値（speed 0／rpm 800／temp 25）に差し替わる（状態判定・DTCは生の値のまま）
- Degraded中、メーターECUの`[METER] FaultStatus`で該当センサのビットが1になり、`[METER] EngineStatus`の該当値はフェイルセーフ値になる
- NORMALが3回連続すると`[FAULT] Recovered: <センサ名>`が出力され、差し替えが解除される（WARNING止まりでは復帰しない）

---

## 冷却ファン制御

担当ECU：ファン制御ECU・メーターECU（未実装）

目的：水温に応じてファンがON/OFFし、閾値付近でON/OFFを繰り返さない（ヒステリシス）ことを確認する

流れ：
1. Ignition ON
2. 水温が上昇し、ON閾値以上になる
3. 水温がON閾値とOFF閾値の間で上下する
4. 水温がOFF閾値以下に下がる

期待結果：
- ON閾値以上になった時点でファンONになる
- ON閾値とOFF閾値の間ではONを保持する（OFFに戻らない）
- OFF閾値以下になった時点でファンOFFになる
- ファン状態がメーターECUに送られ、メーターECUが表示する

---

## 水温センサ故障時の冷却ファン制御（ECU間フェイルセーフ）

担当ECU：エンジン監視ECU・ファン制御ECU（未実装）

目的：エンジン監視ECUが水温をフェイルセーフ値に差し替えても、ファン制御ECUがファンを止めないことを確認する

流れ：
1. Ignition ON
2. 水温がCRITICALを3回連続で超え、エンジン監視ECUで水温センサがDegradedに確定する
3. 送信される水温がフェイルセーフ値（常温相当）に差し替わり、警告灯データで水温センサの確定異常が通知される

期待結果：
- ファン制御ECUは水温センサの確定異常の通知を受けてファンONにする（差し替え後の水温ではOFFにしない）
- 水温センサがRecoveredになり通知が解除されたら、受信した水温による通常の判断に戻る

---

## 診断コマンド（DTCクリア）

担当ECU：エンジン監視ECU

目的：イグニッションOFF時に診断コマンドでDTC記録をクリアできることを確認する

流れ：
1. Ignition ONの間にいずれかのセンサでCRITICALが発生し、DTCが記録される
2. Ignition OFF
3. 診断コマンド（`clear`または`clear <センサ名>`）を入力する

期待結果：
- `clear`で全DTC・フリーズフレームが初期状態に戻る
- `clear <センサ名>`で指定センサ1件だけが初期状態に戻る（フリーズフレームは、原因が指定センサと一致する場合だけ連動してリセットされる）
- 想定外の入力（不正なセンサ名／余分なトークン／空文字列など）ではDTC記録が変化しない
- Ignition ON時はコマンドを受け付けず、`[CMD] Not accepted (ignition ON)`が表示される

---

## 電源再投入

担当ECU：エンジン監視ECU

目的：DTC記録が電源再投入（プログラムの再起動）をまたいで保持されることを確認する

流れ：
1. 1回目の起動：Ignition ONの間にいずれかのセンサでCRITICALが発生し、DTCが記録される
2. プログラム終了時にDTC記録が`dtc_data.txt`へ保存される
3. プログラムを再度起動する（2回目の起動。1回目との間に`clear`は行わない）
4. 起動時に`dtc_data.txt`からDTC記録が読み込まれる

期待結果：
- 2回目の起動直後、1回目で記録されたDTCの発生回数・状態区分が引き継がれている
- エッジ検出用の前回状態（previous）は、2回目の起動時点でNORMALにリセットされている（保存対象外のため）
- そのため、2回目の起動でも同じセンサがCRITICALのままなら、異常に入った瞬間として発生回数がもう1つ増える（実車でも、発生回数はイグニッションON〜OFFの1サイクルごとに数える）
- フリーズフレームも引き継がれ、2回目の起動でCRITICALが発生しても上書きされない（最初に発生した瞬間の値を残す）
- 保存ファイルが無い場合（初回起動）は、DTC0件の初期状態から開始する
- 起動時自己診断は、`config.txt`と`dtc_data.txt`が両方読み込めた場合のみ`[POST] Self-check passed`、`config.txt`が無い場合、または`dtc_data.txt`が無い・壊れている場合は`[POST] Self-check did not pass, continuing with defaults`となり、どちらの場合も動作を継続する（`config.txt`は内容が壊れていても、ファイルが開ければ読み込めた扱いになる）
- メーターECUの通信異常の記録（`can_diag.c`）は永続化していない。3ECU構成ではメーターECU側のデータになるため、ECU間通信の実現方式を決めた後に扱う

---

## 設定ファイル異常時のフェイルセーフ（リンプホームモード）

担当ECU：エンジン監視ECU

目的：config.txtが無い、または壊れている場合に、閾値・ログレベルがデフォルト値のまま動作を継続することを確認する

流れ：
1. config.txtが存在しない、または不正な内容の状態でプログラムを起動する
2. 通常通りセンサ値の更新・判定・DTC記録が行われる

期待結果：
- 閾値はalert.h/status.hのデフォルト値（`config_init`相当）のまま動作する
- ログレベルはデフォルト（LOG_INFO、全ログ表示）のまま動作する
- プログラムが異常終了せず、サンプルループが最後まで実行される
- config.txtが無い場合、`[CONFIG] No config file (using defaults)`と`[POST] Self-check did not pass, continuing with defaults`が表示される
- config.txtはあるが内容が壊れている場合、`[CONFIG] Loaded config file`が表示され、解釈できない行・値域外の値は無視される。起動時自己診断はファイルが読み込めたかだけを見るため、`dtc_data.txt`も読み込めていれば`[POST] Self-check passed`になる

---

## 通信故障

担当ECU：エンジン監視ECU（送信）・メーターECU（受信）・ファン制御ECU（送受信、未実装）

目的：CAN通信の途絶（Timeout）・異常データ（Invalid Data）を検出し、DTC相当の記録（`can_diag.c`）に反映できることを確認する

流れ（Phase20で実装済みの範囲）：
1. エンジンECU役がゲージデータ（`CAN_ID_ENGINE_STATUS`、1000ms周期）・警告灯データ（`CAN_ID_FAULT_STATUS`、200ms周期）をCAN通信でメーターECU役へ送信する
2. `can_fault.txt`で指定した期間だけ、意図的に送信を止める（Timeout、`MODE=DROP`、Phase22）、またはEngineStatusの送信データを値域外に差し替える（Invalid Data、`MODE=CORRUPT`、Phase22拡張バックログ、EngineStatusのみ対応）
3. メーターECU役が3回連続でTimeout/Invalid Dataを検知すると、`CanLinkState`が`CAN_LINK_LOST`に確定する
4. 送信が再開/正常な値に戻り、メーターECU役が3回連続で正常受信すると`CAN_LINK_OK`に復帰する

期待結果：
- Timeout/Invalid Dataが3回連続発生すると`[CAN] ... link lost`のログが出力される
- 3回連続で正常受信すると`[CAN] ... link recovered`のログが出力される
- Timeout/Invalid Dataの確定・復帰は、DTC相当の記録（`can_diag.c`、発生回数・状態区分NONE/ACTIVE/HISTORY）にも反映される（Phase21）。ただし電源再投入をまたいだ永続化はしていない（`CanLinkState`・DTC相当の記録とも、プログラム起動のたびに初期状態から再開する）
- ファン制御ECUは、ゲージデータ・警告灯データのどちらかの途絶が確定するとファンONにして記録し、正常受信に復帰すると通常の判断に戻る（未実装）
- メーターECUは、ファン状態の途絶が確定するとファン制御ECUの異常として記録・表示する（未実装）

備考：`can_fault.txt`が無ければ、イグニッションON/OFFの仕組み上送受信が同時に止まり・復帰するためTimeout/Invalid Dataは自然発生しない（Phase22のFault Injectionで初めて`make run`上で再現できる）。Invalid Data注入（`MODE=CORRUPT`）はEngineStatusのみ対応する。FaultStatusはビットフラグのみでInvalid Data判定ロジックが無いため対象外で、`MODE=CORRUPT`+`TARGET=FAULT_STATUS`は無効な組み合わせとして注入なし扱いになる（Phase22拡張バックログ）

---

## 備考：固定値による再現

正常走行／故障発生／診断コマンド／電源再投入は、study_plan.mdのPhase11（固定値注入によるシナリオ再現の仕組み構築）で、config.txtと同様のKEY=VALUE形式によりセンサ固定値（speed/rpm/temperature）を注入できるようにしてから、上記の期待結果を実際に確認する。診断コマンドのシナリオでは、注入したセンサ値でDTCを発生させた後、`cmd_dispatch`に直接コマンド文字列を渡すことで（test/test_cmd.cと同じやり方で）再現する。

故障発生の期待結果は、`make scenario`（Phase25）で自動検証している。fixture.txtは1回の実行中ずっと同じ値しか注入できないため、1サンプル目から最後までCRITICALが続く形（イグニッションは`IGNITION=ON`で固定）で、Degraded確定までを確認する。流れのうち正常範囲での更新・正常範囲に戻る・Ignition OFFと、期待結果のうちRecoveredは自動検証の対象外。

電源再投入の期待結果も、`make scenario`（Phase25）で自動検証している。同じディレクトリで3回続けて起動し（水温CRITICALでON→OFF→値を変えたCRITICALでON）、2回目はOFFのままDTCが更新されないことを利用して、起動直後に読み込まれた内容を確認する。期待結果のうちメーターECUの通信異常の記録は、Phase25で通信故障を対象外としたため自動検証していない。

設定ファイル異常時のフェイルセーフは、センサ値の注入ではなくconfig.txtファイル自体の有無・内容を変えることで再現する（test/test_config.cの`write_raw`と同じやり方）。Phase11の対象には含めない。このシナリオの期待結果は、`make scenario`（Phase25）で自動検証している。
