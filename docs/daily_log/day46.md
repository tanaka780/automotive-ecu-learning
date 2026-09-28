# Day46

## 実施内容

- `include/can_fault.h`・`src/can_fault.c`を更新した（Phase22拡張バックログ、Invalid Data注入`MODE=CORRUPT`の追加。`CanFaultConfig.active`（bool）を`CanFaultMode`（NONE/DROP/CORRUPT）に置き換え、`can_fault_is_corrupted`を新設。`can_fault_send_engine_status`にCORRUPT分岐を追加し、送信専用の破損コピーを作って`can_send_engine_status`へ渡すようにした）
- `test/test_can_fault.c`：既存10件を`.mode`ベースに修正し、新規4件（`is_corrupted`の対象一致・境界、`MODE=CORRUPT`の正常系・`TARGET=FAULT_STATUS`無効系）を追加した（計14件）

## 確認内容

- 設計段階で、Invalid Dataを再現する方法を「送信後にCANフレームの`data`を直接上書きする案」から「送信専用の一時コピーを作って`can_send_engine_status`にそのまま渡す案」に訂正した理由が説明できるか
- CORRUPTの対象をEngineStatusのみに限定した理由（FaultStatusはビットフラグのみでInvalid Data判定ロジックが無い）が説明できるか
- `can_fault_is_corrupted`が`can_fault_is_dropped`と対になる純粋関数として実装されているか
- `make clean && make`が警告0件で通ること
- `make test`で全17ターゲットが通ること
- `make run`で、`MODE=CORRUPT`指定時にEngineStatusのInvalid Dataが実際に確定し、CAN-DTCに反映されるか

## 実行結果

- `make clean && make`：警告0件で通った
- `make test`：全17ターゲットPASS（`test_can_fault`は既存10件＋新規4件＝14件PASS、他16ターゲットは変更なし）
- `make run`（`can_fault.txt`：`MODE=CORRUPT`, `TARGET=ENGINE_STATUS`, `START_MS=0`, `END_MS=25000`）：3回実行し、いずれも`[CAN] EngineStatus link lost`→`[CAN-DTC] EngineStatus ACTIVE link lost occurrences: 1`を確認した。注入期間を`START_MS=3000`/`END_MS=6000`と短くした回では、イグニッションOFFのタイミングと重なり再現しなかった（DROP同様の既知の制約と一致）

## 判定

テストは全てPASSし、`make run`でも複数回の実行でInvalid Dataの確定・DTC反映が再現されたことから、Phase22拡張バックログの実装は期待通りに動作していると判断できる。設計段階で一度提示した案（フレーム直接上書き）を、実車CANの物理層CRCの実態およびフレーム構造の知識の重複という2つの理由から自分で訂正できたことは、設計レビューとして妥当な過程だったと考える。

## 今回の設計方針

- Invalid Dataの再現方法は、実センサ値（`sensor_data`/`effective_data`）とは別にCAN送信専用の破損コピーを作り、既存の`can_send_engine_status`にそのまま渡す方式にした。フレームのバイト配置の知識を`can.c`だけに閉じ込め、`can_fault.c`に重複させない設計判断
- CORRUPTの対象はEngineStatusのみとし、FaultStatusとの組み合わせは無効な指定として注入なし扱いにした（`can.c`本体を変更しないというPhase22の前提を維持するため）
- 壊す値はspeed/rpm/temp全フィールドを`0xFF`で固定した。DROPがメッセージ全体を対象にする粒度に合わせた

## 次回やること

- 次のテーマ候補として「テストカバレッジ計測（gcov/lcov）」「MISRA再チェック（Phase14以降の新規モジュールへの適用）」の2つが挙がっている。どちらもCの範囲で完結する小さいテーマのため、次回どちらから着手するか判断する
