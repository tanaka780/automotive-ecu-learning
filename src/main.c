#include <stdio.h>
#include <stdbool.h>  /* bool を使うために必要 */
#include <stdlib.h>   /* srand(), rand() */
#include <time.h>     /* time() */
#include "sensor.h"
#include "stats.h"
#include "alert.h"
#include "status.h"
#include "diag.h"
#include "logger.h"
#include "ignition.h"
#include "persist.h"
#include "cmd.h"
#include "config.h"
#include "fixture.h"
#include "faultmgr.h"
#include "scheduler.h"
#include "can.h"
#include "can_diag.h"
#include "can_fault.h"
#include "meter.h"
#include "fan_ecu.h"

/* サンプルの周期 [ms]。以前はsleep(1)で固定1秒待っていたが、Timerによる経過時間ベースの
   周期判定（Phase18）を経て、周期待ち・タスク呼び出しをSchedulerに委譲した（Phase19） */
#define SAMPLE_PERIOD_MS 1000U

/* サンプル数: ここを変えるだけでループ回数を変えられる */
#define SAMPLE_COUNT 20

/* 警告灯データ(CAN_MSG_FAULT_STATUS)の送受信周期 [ms]。実車で異常系ほど高頻度に送る慣例に合わせ、
   ゲージデータ(SAMPLE_PERIOD_MS)より短くする（Phase20、Day43決定） */
#define CAN_FAULT_PERIOD_MS 200U

/* Schedulerに登録するサンプル周期タスク(run_sample_cycle)が参照する、main()のローカル変数への
   ポインタ一式。タスク関数はvoid *contextしか受け取れないため、必要なデータをここにまとめて渡す */
typedef struct {
    int               sample_index;    /* "[Sample XX]"表示・完了判定用。run_sample_cycle自身が更新する（Phase20） */
    Ignition          *ignition;
    bool              ignition_fixed;        /* fixture.txtのIGNITIONで固定する場合true（Phase25） */
    IgnitionState     ignition_fixed_state;  /* ignition_fixedがtrueのときに使う状態 */
    const FixtureIgnitionSteps *ignition_steps; /* fixture.txtのIGNITION@N行（途中で切り替える状態）。ignition_fixedのときだけ使う */
    bool              fixture_fixed;
    const FixtureTempSteps *temp_steps;      /* fixture.txtのTEMP@N行（途中で変える水温）。fixture_fixedのときだけ使う */
    VehicleSensorData *sensor_data;
    const ConfigData  *config;
    SensorStatus      *sensor_status;
    DtcRecord         *dtc;
    FaultManager      *fault_mgr;
    VehicleStats      *stats;
    CanBus            *can_bus;
    const CanFaultConfig *can_fault;   /* CAN Fault Injection設定（Phase22）。送信をラップする際に使う */
} SampleCycleContext;

/* エンジンECU役の警告灯データ送信タスク(run_can_send_fault_status)が参照するポインタ一式（Phase20）。
   もとはメーターECU役の受信タスクと同じ形のcontextを共有していたが、送信側からメーターECU役の
   受信監視状態が見えないよう、送信用・受信用に分けた（Phase26） */
typedef struct {
    const Ignition *ignition;
    CanBus         *can_bus;
    const FaultManager *fault_mgr;
    const CanFaultConfig *can_fault; /* Phase22 */
} EngineCanSendContext;

/* ファン制御ECU役のファン状態送信タスク(run_fan_send_fan_status)が参照するポインタ一式（Phase26）。
   FanEcuContextはbusを読むだけ(const)なので、busへ書く送信用はEngineCanSendContextと同じく分ける */
typedef struct {
    FanEcuContext        *fan;
    CanBus               *can_bus;
    const CanFaultConfig *can_fault;   /* 送信をラップしてファン状態を止める（Phase26） */
} FanCanSendContext;

/* 1サンプル分の処理（旧main.cのforループ本体）。Schedulerから周期(SAMPLE_PERIOD_MS)ごとに呼ばれる。
   CAN送受信タスク（200ms周期、Phase20）が同じSchedulerに混在する現在は、1回のscheduler_run_due呼び出し
   がこのタスクを実行するとは限らないため、sample_indexはmain()側ではなくここで自己インクリメントし、
   main()はsample_indexがSAMPLE_COUNTに達するまでscheduler_run_dueを呼び続ける形にする */
static void run_sample_cycle(void *context) {
    SampleCycleContext *ctx = (SampleCycleContext *)context;
    ctx->sample_index++;

    char sample_line[16];   /* "[Sample 20]" が収まるサイズ */
    /* 表示幅は型・書式指定子で保証されており切り詰めは起こらないため、戻り値は(void)で明示的に無視する（MISRA 17.7） */
    (void)snprintf(sample_line, sizeof(sample_line), "[Sample %02d]", ctx->sample_index);
    log_print(sample_line);
    if (ctx->ignition_fixed) {
        /* IGNITION@N行があれば、このサンプルの状態に切り替える。当てはまる行が無ければIGNITION=の状態のまま */
        IgnitionState state = ctx->ignition_fixed_state;
        (void)fixture_ignition_at(ctx->ignition_steps, ctx->sample_index, &state);
        ignition_set(ctx->ignition, state);           /* fixture.txtで固定した状態で更新する (書く) */
    } else {
        ignition_update(ctx->ignition);               /* イグニッション状態を更新する (書く) */
    }
    ignition_print(ctx->ignition);                    /* イグニッション状態を表示する (読む) */
    ignition_check(ctx->ignition);                     /* 遷移した瞬間だけイベントを表示する (読む) */

    /* OFF中はECU自体が通電していない状態を再現し、センサ更新以降の処理を全てスキップする */
    if (ctx->ignition->current == IGNITION_ON) {
        if (!ctx->fixture_fixed) {
            sensor_update(ctx->sensor_data);              /* 固定値注入が無ければセンサ値を更新する (書く) */
        } else {
            /* TEMP@N行があれば、このサンプルで使う水温に差し替える。当てはまる行が無ければTEMP=の値のまま */
            (void)fixture_temp_at(ctx->temp_steps, ctx->sample_index, &ctx->sensor_data->temperature);
        }
        sensor_print(ctx->sensor_data);                   /* センサ値を表示する (読む) */
        status_check(ctx->sensor_status, ctx->sensor_data, ctx->config); /* 状態レベルを判定する */
        status_print(ctx->sensor_status);                 /* 状態レベルを表示する (読む) */
        diag_check(ctx->dtc, ctx->sensor_status, ctx->sensor_data); /* CRITICALに入った瞬間をDTCとして記録する */
        faultmgr_check(ctx->fault_mgr, ctx->sensor_status); /* 確定した異常(Degraded)・復帰(Recovery)を判定する */

        /* diag_check/status_checkは常にraw(sensor_data)を見て診断の正確性を保つ。
           Degraded中のセンサはeffective_data側だけフェイルセーフ値に差し替え、
           以降の警告・統計はeffective_dataを使って動作を継続する */
        VehicleSensorData effective_data;
        faultmgr_apply_safe_values(ctx->fault_mgr, ctx->sensor_data, &effective_data);
        alert_check(&effective_data, ctx->config);         /* 閾値超過の警告を表示する (読む) */
        stats_update(ctx->stats, &effective_data);         /* 統計データを更新する */

        /* ゲージデータ(CAN_MSG_ENGINE_STATUS)は、Degraded中のセンサをフェイルセーフ値に差し替えた
           effective_dataを送る。実車のダッシュボードもリンプホーム中は縮退後の値を表示するため。
           can_fault.txtでDrop対象・期間が指定されていれば、意図的に送信しない（Phase22） */
        can_fault_send_engine_status(ctx->can_fault, ctx->can_bus, &effective_data);
    }
}

/* エンジンECU役：警告灯データ(CAN_MSG_FAULT_STATUS)をCAN_FAULT_PERIOD_MS周期で送信する。
   ゲージデータと違いfault_mgrの状態(1000ms周期で更新)をそのまま再送するだけの周期タスク（Phase20） */
static void run_can_send_fault_status(void *context) {
    EngineCanSendContext *ctx = (EngineCanSendContext *)context;
    if (ctx->ignition->current == IGNITION_ON) {
        can_fault_send_fault_status(ctx->can_fault, ctx->can_bus, ctx->fault_mgr);
    }
}

/* メーターECU役（meter.c）の受信処理をSchedulerのタスクとして呼ぶための変換。void *contextを
   MeterContext *に戻す処理をmain.c（ECU間の配線）に置き、meter.cは型付きで受け取る（Phase26） */
static void run_can_receive_fault_status(void *context) {
    meter_receive_fault_status((MeterContext *)context);
}

static void run_can_receive_engine_status(void *context) {
    meter_receive_engine_status((MeterContext *)context);
}

static void run_can_receive_fan_status(void *context) {
    meter_receive_fan_status((MeterContext *)context);
}

/* ファン制御ECU役（fan_ecu.c）の受信処理も、メーターECU役と同じ形で変換して呼ぶ（Phase26） */
static void run_fan_receive_fault_status(void *context) {
    fan_ecu_receive_fault_status((FanEcuContext *)context);
}

static void run_fan_receive_engine_status(void *context) {
    fan_ecu_receive_engine_status((FanEcuContext *)context);
}

/* ファン制御ECU役：ファン状態(CAN_MSG_FAN_STATUS)をSAMPLE_PERIOD_MS周期で送信する（Phase26）。
   何を送るかはfan_ecu.cが決め、busへの書き込みはエンジンECU役の送信と同じくここで行う。
   can_fault.txtでFAN_STATUSのDrop期間が指定されていれば、意図的に送信しない */
static void run_fan_send_fan_status(void *context) {
    FanCanSendContext *ctx = (FanCanSendContext *)context;
    bool fan_on = false;
    if (fan_ecu_get_fan_status(ctx->fan, &fan_on)) {
        can_fault_send_fan_status(ctx->can_fault, ctx->can_bus, fan_on);
    }
}

int main(void) {
    /* srand: time(NULL) を種にすることで実行ごとに異なる乱数列を生成する */
    srand((unsigned int)time(NULL));

    VehicleSensorData sensor_data;   /* センサ値（各サンプルごとに上書き更新） */
    sensor_init(&sensor_data);
    /* fixture.txtがあればセンサ値を固定値に差し替える。trueならループ内でsensor_updateを呼ばない */
    bool fixture_fixed = fixture_apply(&sensor_data, FIXTURE_FILENAME);
    /* fixture.txtにIGNITION行があればイグニッションも固定する。falseならループ内でignition_updateを使う（Phase25） */
    IgnitionState fixed_ignition = IGNITION_OFF;
    bool ignition_fixed = fixture_load_ignition(&fixed_ignition, FIXTURE_FILENAME);
    /* fixture.txtにTEMP@N行があれば、N秒目以降の水温を変える。行が無ければcount=0で何も変えない */
    FixtureTempSteps temp_steps;
    (void)fixture_load_temp_steps(&temp_steps, FIXTURE_FILENAME);
    /* fixture.txtにIGNITION@N行があれば、N秒目以降のイグニッションを切り替える（IGNITION=で固定したときだけ使う） */
    FixtureIgnitionSteps ignition_steps;
    (void)fixture_load_ignition_steps(&ignition_steps, FIXTURE_FILENAME);

    VehicleStats stats;              /* 統計データ（全サンプル分を集計） */
    stats_init(&stats);

    ConfigData config;                /* 閾値の設定値（alert_check/status_checkに渡す） */
    config_init(&config);
    bool config_ok = config_load(&config, CONFIG_FILENAME);
    /* 読み込み失敗時もconfig_initのデフォルト値のまま継続する（フェイルセーフ）。詳細はconfig.c内のログ参照 */
    logger_set_level(config.log_level);   /* config_load直後に呼ぶ: 以降のlog_print_leveled呼び出しに反映させる */
    config_print(&config);

    SensorStatus sensor_status;     /* 状態レベル（status_check が毎回上書き、初期化不要） */

    DtcRecord dtc;                  /* DTC記録（センサ別のCRITICAL発生回数、前回状態を内部に保持） */
    diag_init(&dtc);
    bool dtc_ok = persist_load_dtc(&dtc, PERSIST_DTC_FILENAME);
    /* 保存ファイルが無い/壊れている場合もdiag_initの初期値のまま継続する（フェイルセーフ）。詳細はpersist.c内のログ参照 */

    /* 起動時自己診断（POST）：キャリブレーションデータ（config.txt）と診断メモリ（dtc_data.txt）が
       両方読み込めたかをまとめて記録する。失敗時もconfig_init/diag_initの初期値で動作を継続するため
       処理の流れは変えず、自己診断の結果をログに残すだけにとどめる */
    if (config_ok && dtc_ok) {
        log_print_leveled(LOG_INFO, "POST", "Self-check passed");
    } else {
        log_print_leveled(LOG_INFO, "POST", "Self-check did not pass, continuing with defaults");
    }

    Ignition ignition;               /* イグニッション状態（現在・前回を内部に保持） */
    ignition_init(&ignition);

    FaultManager fault_mgr;           /* センサ別のDebounce/Degraded/Recovery状態（永続化はしない） */
    faultmgr_init(&fault_mgr);

    /* CAN通信（Phase20）：実プロセス分離は行わず、busを介してエンジンECU役(送信)・メーターECU役(受信)
       をSchedulerタスクとして表現する（Day43決定） */
    CanBus can_bus;
    can_bus_init(&can_bus);
    CanMonitor can_monitor;           /* メーターECU役の受信監視状態（Timeout/Invalid Dataの確定・復帰） */
    can_monitor_init(&can_monitor);
    CanDtcRecord can_dtc;              /* CAN通信リンクのDTC相当の記録（発生回数・状態区分、Phase21） */
    can_diag_init(&can_dtc);
    /* ファン制御ECU役の受信監視状態とDTC相当の記録（Phase26）。途絶は検知したECUごとに持つため、
       メーターECU役のものとは分ける */
    CanMonitor fan_can_monitor;
    can_monitor_init(&fan_can_monitor);
    CanDtcRecord fan_can_dtc;
    can_diag_init(&fan_can_dtc);
    FanEcuState fan_state;             /* ファン制御ECU役の判断の状態（水温・要求・出力） */
    fan_ecu_init(&fan_state);

    /* CAN Fault Injection（Phase22）：can_fault.txtがあれば、指定したメッセージ・期間だけ
       意図的に送信をスキップする（Timeoutの実行時再現）。ファイルが無い/MODE=NORMALならinactiveのまま */
    CanFaultConfig can_fault_cfg;
    can_fault_init(&can_fault_cfg);
    (void)can_fault_load(&can_fault_cfg, CAN_FAULT_FILENAME);

    SampleCycleContext ctx = {
        .sample_index  = 0,
        .ignition      = &ignition,
        .ignition_fixed       = ignition_fixed,
        .ignition_fixed_state = fixed_ignition,
        .ignition_steps       = &ignition_steps,
        .fixture_fixed = fixture_fixed,
        .temp_steps    = &temp_steps,
        .sensor_data   = &sensor_data,
        .config        = &config,
        .sensor_status = &sensor_status,
        .dtc           = &dtc,
        .fault_mgr     = &fault_mgr,
        .stats         = &stats,
        .can_bus       = &can_bus,
        .can_fault     = &can_fault_cfg,
    };

    /* 警告灯データ送信タスク(エンジンECU役)用。fault_mgrを読むだけで書き換えない */
    EngineCanSendContext can_send_fault_ctx = {
        .ignition    = &ignition,
        .can_bus     = &can_bus,
        .fault_mgr   = &fault_mgr,
        .can_fault   = &can_fault_cfg,
    };
    /* 受信タスク(メーターECU役)用。警告灯・ゲージ・ファン状態の受信タスクで共有する */
    MeterContext can_receive_ctx = {
        .ignition    = &ignition,
        .can_bus     = &can_bus,
        .can_monitor = &can_monitor,
        .can_dtc     = &can_dtc,
    };
    /* 受信タスク(ファン制御ECU役)用。警告灯・ゲージの両受信タスクで共有する（Phase26） */
    FanEcuContext fan_ctx = {
        .ignition    = &ignition,
        .can_bus     = &can_bus,
        .can_monitor = &fan_can_monitor,
        .can_dtc     = &fan_can_dtc,
        .state       = &fan_state,
    };
    /* 送信タスク(ファン制御ECU役)用（Phase26） */
    FanCanSendContext fan_send_ctx = {
        .fan       = &fan_ctx,
        .can_bus   = &can_bus,
        .can_fault = &can_fault_cfg,
    };

    Scheduler scheduler;                /* サンプル周期タスク・CAN送受信タスクを管理する */
    scheduler_init(&scheduler);
    /* SCHEDULER_MAX_TASKS(8)に対し登録は8個（サンプル1・CAN送信1・メーターECU受信2・ファン制御ECU受信2・
       ファン制御ECU送信1・メーターECUのファン状態受信1）で、上限ちょうど。いずれも失敗は想定していない
       （MISRA 17.7）。Phase26で足したタスクは既存4タスクの後に登録し、既存の登録順は変えない。
       メーターECUのファン状態受信は、同じ周期のファン状態送信の後に登録する */
    (void)scheduler_add_task(&scheduler, run_sample_cycle, &ctx, SAMPLE_PERIOD_MS);
    (void)scheduler_add_task(&scheduler, run_can_send_fault_status, &can_send_fault_ctx, CAN_FAULT_PERIOD_MS);
    (void)scheduler_add_task(&scheduler, run_can_receive_fault_status, &can_receive_ctx, CAN_FAULT_PERIOD_MS);
    (void)scheduler_add_task(&scheduler, run_can_receive_engine_status, &can_receive_ctx, SAMPLE_PERIOD_MS);
    (void)scheduler_add_task(&scheduler, run_fan_receive_fault_status, &fan_ctx, CAN_FAULT_PERIOD_MS);
    (void)scheduler_add_task(&scheduler, run_fan_receive_engine_status, &fan_ctx, SAMPLE_PERIOD_MS);
    (void)scheduler_add_task(&scheduler, run_fan_send_fan_status, &fan_send_ctx, SAMPLE_PERIOD_MS);
    (void)scheduler_add_task(&scheduler, run_can_receive_fan_status, &can_receive_ctx, SAMPLE_PERIOD_MS);

    /* main.c は処理の順序制御のみ。各処理の詳細はモジュールに書く。
       CAN送受信タスク（200ms周期）がsample_cycle（1000ms周期）より高頻度で混在するため、
       1回のscheduler_run_dueがsample_cycleを実行するとは限らない。sample_indexが
       SAMPLE_COUNTに達する（run_sample_cycleがSAMPLE_COUNT回実行される）までポーリングし続ける */
    while (ctx.sample_index < SAMPLE_COUNT) {
        scheduler_run_due(&scheduler);   /* 周期が来ているタスク（サンプル・CAN送受信）を実行する */
    }

    stats_print(&stats);
    diag_print(&dtc);                                 /* DTC一覧を表示する */
    can_diag_print(&can_dtc, "CAN-DTC");              /* メーターECU役のCAN通信リンクのDTC相当の記録を表示する（Phase21） */
    can_diag_print(&fan_can_dtc, "FAN-DTC");          /* ファン制御ECU役の記録を表示する（Phase26） */

    /* 実車でスキャンツールが駐車中(イグニッションOFF)に接続される状況を再現し、OFF時のみコマンドを受け付ける */
    if (ignition.current == IGNITION_OFF) {
        log_print("Enter command (clear/Enter to skip):"); /* 診断ツールからのコマンド入力を受け付ける */
        char command_line[CMD_BUF_SIZE];
        if (cmd_read_line(command_line, sizeof(command_line))) {
            cmd_dispatch(command_line, &dtc);
        }
    } else {
        cmd_notify_rejected();
    }

    if (!persist_save_dtc(&dtc, PERSIST_DTC_FILENAME)) {   /* 次回起動時のためにDTC記録を保存する */
        /* 保存失敗時も特別な処理は行わない。詳細はpersist.c内のログ参照 */
    }

    return 0;
}
