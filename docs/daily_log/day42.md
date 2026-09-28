# Day42

## 実施内容

- 新規`include/scheduler.h`/`src/scheduler.c`を作成した：関数ポインタ・context・`Timer`を1組にしたタスクを固定長配列（`SCHEDULER_MAX_TASKS`=4）で複数保持し、優先度を持たず登録順に周期判定・実行する（`scheduler_add_task`/`scheduler_run_due`）
- `main.c`のforループ本体（旧サンプル1サイクル分の処理）を`run_sample_cycle`として切り出し、Schedulerにタスク登録する形に変更した。`SAMPLE_COUNT`によるループ回数管理自体は変えていない
- `Makefile`に`src/scheduler.c`と`test_scheduler`ターゲットを追加した
- `test/test_scheduler.c`を新規作成した：初回呼び出しの即時実行、初回実行後の周期待ち、登録上限超過時の`false`返却の3点を確認する

## 確認内容

- 登録直後の1回目の呼び出しは周期を待たず即座にタスクが実行されること（offset=0相当）
- 初回実行後は、周期(`period_ms`)が経過するまで2回目が実行されないこと
- 登録数が上限（`SCHEDULER_MAX_TASKS`）に達している場合、追加登録が`false`を返すこと
- Scheduler導入前後で、外部から見たサンプルループの動作（20サンプル・1000ms間隔・初回即時実行）が変わらないこと

## 実行結果

- `make clean && make`：警告0件
- `make test`：既存11ターゲット＋`test_scheduler`（3件）を含む全テストがPASS
- `make run`：Sample 1が即時に表示され、以降1000ms間隔で20サンプル、全体で約19.1秒（1000ms×19間隔）

## 判定

確認内容と実行結果が一致しており、Scheduler導入によって外部から見た動作（サンプル間隔・サンプル数）に影響が出ていないことを確認した。

## 今回の設計方針

- タスク関数はcontext（`void *`）しか受け取れない一方、既存の`sensor_data`/`config`/`dtc`等はmain()のローカル変数のままにしたかったため、これらへのポインタをまとめた`SampleCycleContext`を1つ作り、それをcontextとして渡す形にした
- 初回実行のタイミングについて、実車のOSEK/AUTOSAR OSが持つoffset（周期とは別に、初回起動までの時間を設定できる仕組み）の存在を踏まえて検討した。複数の周期タスクがある実車ECUでは、全タスクのoffsetを0にすると起動タイミングが重なりCPU/バス負荷にスパイクが出るため、意図的にずらすのが実務の作法だと分かった。ただし今回はタスクが1個のみでその効果が無いため、offset機構自体は実装せず、初回は即座に実行する（offset=0固定）にとどめた

## 次回やること

- CAN通信の実装に着手する（Phase番号は着手時に採番する）
