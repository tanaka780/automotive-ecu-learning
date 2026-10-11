# コンパイラの指定
CC = gcc

# コンパイルオプション
# -Wall -Wextra: 警告を多く出す (バグの早期発見に役立つ)
# -std=c11: C11 規格でコンパイルする
# -Iinclude: include/ フォルダを #include の検索パスに追加する
# -Isim: sim/ フォルダ（PC上の検証専用モジュール、Phase25）を #include の検索パスに追加する。
#        main.cがfixture.h/can_fault.hを読むため、ECU本体側にも必要になっている
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -Isim

# Unity本体はvendor/unity/に配置（公式ThrowTheSwitch/Unityリポジトリより取得、MITライセンス、Phase13）
# テストターゲットのみ、Unityのヘッダを検索できるよう-Ivendor/unityを追加したTEST_CFLAGSを使う
UNITY_DIR = vendor/unity
TEST_CFLAGS = $(CFLAGS) -I$(UNITY_DIR)

# カバレッジ計測用（Phase23）: TEST_CFLAGSに--coverageを追加しただけの専用フラグ。
# 通常のtest_xxx（TEST_CFLAGS）とは別の実行ファイル（xxx_cov）をビルドするため、既存のtest_xxxには影響しない
TEST_CFLAGS_COV = $(TEST_CFLAGS) --coverage

# コンパイル対象のソースファイル
# モジュールを追加したときはここに追記する
# ECU_SRCS: ECU本体（src/）、SIM_SRCS: PC上の検証専用（sim/、固定値注入・CAN Fault Injection、Phase25）。
# sensor_simは両方を合わせてビルドする。main.cがsim/の関数を直接呼んでいるため、現状はsim/を外すとビルドできない
ECU_SRCS = src/main.c src/sensor.c src/stats.c src/alert.c src/status.c src/diag.c src/logger.c src/ignition.c src/persist.c src/cmd.c src/config.c src/validate.c src/faultmgr.c src/timer.c src/scheduler.c src/debounce.c src/can.c src/dtc_status.c src/can_diag.c src/meter.c
SIM_SRCS = sim/fixture.c sim/can_fault.c
SRCS = $(ECU_SRCS) $(SIM_SRCS)

# 全ビルドルールの依存関係に加えるヘッダファイル（Phase26）。.cだけを依存関係にしていると、
# .hだけを変えたときに古い実行ファイルのまま動いてしまう。どの.hがどの.cに読まれるかは区別せず、
# いずれかの.hが変われば全て作り直す（ビルドは数秒のため、正確さより単純さを優先する）
HEADERS = $(wildcard include/*.h sim/*.h test/*.h)

# 生成する実行ファイルの名前
TARGET = sensor_sim

# diag.c の動作確認用テスト（固定値データ、main.c は使わない）
# test/test_common.c: test_diag.c / test_persist.c / test_cmd.cで共通のテスト補助関数（test_feed/test_default_config）
# test_common.c が内部でtest_default_config（config_init）を使うため src/config.c も含める
TEST_DIAG_SRCS = test/test_diag.c test/test_common.c src/status.c src/diag.c src/logger.c src/config.c src/validate.c src/dtc_status.c $(UNITY_DIR)/unity.c
TEST_DIAG_TARGET = test_diag

# persist.c の動作確認用テスト（ファイルI/Oの正常系・異常系、main.c は使わない）
TEST_PERSIST_SRCS = test/test_persist.c test/test_common.c src/status.c src/diag.c src/logger.c src/persist.c src/config.c src/validate.c src/dtc_status.c $(UNITY_DIR)/unity.c
TEST_PERSIST_TARGET = test_persist

# stats.c の動作確認用テスト（min/max/sum/countの更新、main.c は使わない）
# stats.cはDtcRecordに依存せずtest_common.cのtest_feed/test_default_configも使わないため、stats.c+logger.cのみで足りる
TEST_STATS_SRCS = test/test_stats.c src/stats.c src/logger.c $(UNITY_DIR)/unity.c
TEST_STATS_TARGET = test_stats

# alert.c の動作確認用テスト（標準出力キャプチャによる警告出力の確認、main.c は使わない）
# test_common.c が status_check/diag_check を参照するため status.c/diag.c も含める
TEST_ALERT_SRCS = test/test_alert.c test/test_common.c src/status.c src/diag.c src/logger.c src/alert.c src/config.c src/validate.c src/dtc_status.c $(UNITY_DIR)/unity.c
TEST_ALERT_TARGET = test_alert

# ignition.c の動作確認用テスト（標準出力キャプチャによる遷移イベント出力の確認、main.c は使わない）
# ignition.cはtest_common.cのtest_feed/test_default_configを使わないため、ignition.c+logger.cのみで足りる
TEST_IGNITION_SRCS = test/test_ignition.c src/ignition.c src/logger.c $(UNITY_DIR)/unity.c
TEST_IGNITION_TARGET = test_ignition

# cmd.c の動作確認用テスト（diag_clear・cmd_dispatchの確認、main.c は使わない）
# test_common.c が status_check/diag_check を参照するため status.c/diag.c も含める
TEST_CMD_SRCS = test/test_cmd.c test/test_common.c src/status.c src/diag.c src/logger.c src/cmd.c src/config.c src/validate.c src/dtc_status.c $(UNITY_DIR)/unity.c
TEST_CMD_TARGET = test_cmd

# config.c の動作確認用テスト（config_loadのファイルパースの正常系・異常系の確認、main.c は使わない）
# test_common.c が status_check/diag_check を参照するため status.c/diag.c も含める
TEST_CONFIG_SRCS = test/test_config.c test/test_common.c src/status.c src/diag.c src/logger.c src/config.c src/validate.c src/dtc_status.c $(UNITY_DIR)/unity.c
TEST_CONFIG_TARGET = test_config

# fixture.c の動作確認用テスト（fixture_applyのファイルパースの正常系・異常系の確認、main.c は使わない）
# fixture.cはtest_common.cのtest_feed/test_default_configを使わないため、fixture.c+sensor.c(sensor_init用)+validate.c+logger.cのみで足りる
TEST_FIXTURE_SRCS = test/test_fixture.c src/sensor.c sim/fixture.c src/validate.c src/logger.c $(UNITY_DIR)/unity.c
TEST_FIXTURE_TARGET = test_fixture

# validate.c の動作確認用テスト（値域チェック関数自体の境界値確認、main.c は使わない）
# validate.cはlogger.hの定数(LOG_INFO/LOG_ERROR)を参照するだけで他モジュールの関数は呼ばないため、validate.cのみで足りる
TEST_VALIDATE_SRCS = test/test_validate.c src/validate.c $(UNITY_DIR)/unity.c
TEST_VALIDATE_TARGET = test_validate

# faultmgr.c の動作確認用テスト（Debounce/Degraded/Recoveryの遷移、フェイルセーフ値の差し替えの確認、main.c は使わない）
# faultmgr.cはSensorStatusを直接組み立てて呼ぶためstatus.c/config.cは不要。Debounce/Recoveryのカウント処理は
# src/debounce.cに切り出した（Phase20）ため、対象モジュール+logger.c+debounce.cで足りる
TEST_FAULTMGR_SRCS = test/test_faultmgr.c src/faultmgr.c src/logger.c src/debounce.c $(UNITY_DIR)/unity.c
TEST_FAULTMGR_TARGET = test_faultmgr

# timer.c の動作確認用テスト（経過時間の単調増加、周期判定の境界、main.c は使わない）
# timer.cは他モジュールに依存しないため、timer.cのみで足りる
TEST_TIMER_SRCS = test/test_timer.c src/timer.c $(UNITY_DIR)/unity.c
TEST_TIMER_TARGET = test_timer

# scheduler.c の動作確認用テスト（タスク登録・初回即時実行・周期判定の確認、main.c は使わない）
# scheduler.cはtimer.cに依存するため、timer.cも含める
TEST_SCHEDULER_SRCS = test/test_scheduler.c src/scheduler.c src/timer.c $(UNITY_DIR)/unity.c
TEST_SCHEDULER_TARGET = test_scheduler

# debounce.c の動作確認用テスト（Debounce/Recoveryの連続回数カウントの確認、main.c は使わない）
# debounce.cは他モジュールに依存しないため、debounce.cのみで足りる（Phase20）
TEST_DEBOUNCE_SRCS = test/test_debounce.c src/debounce.c $(UNITY_DIR)/unity.c
TEST_DEBOUNCE_TARGET = test_debounce

# can.c の動作確認用テスト（フレーム送受信の往復一致、Timeout/Invalid Dataの確定・復帰の確認、main.c は使わない）
# can.cはfaultmgr.c（送信データの組み立て元）・validate.c（Invalid Data判定）・timer.c（timestamp）・
# debounce.c（Timeout/Invalid Dataの確定・復帰）に依存する（Phase20）
TEST_CAN_SRCS = test/test_can.c src/can.c src/faultmgr.c src/debounce.c src/timer.c src/validate.c src/logger.c $(UNITY_DIR)/unity.c
TEST_CAN_TARGET = test_can

# dtc_status.c の動作確認用テスト（発生回数・状態区分(NONE/ACTIVE/HISTORY)の更新確認、main.c は使わない）
# dtc_status.cは他モジュールに依存しないため、dtc_status.cのみで足りる（Phase21）
TEST_DTC_STATUS_SRCS = test/test_dtc_status.c src/dtc_status.c $(UNITY_DIR)/unity.c
TEST_DTC_STATUS_TARGET = test_dtc_status

# can_diag.c の動作確認用テスト（CanLinkStateの遷移によるDTC相当の記録確認、main.c は使わない）
# can_diag.cはdtc_status.c（発生回数・状態区分の更新）とlogger.c（can_diag_print）に依存する（Phase21）
TEST_CAN_DIAG_SRCS = test/test_can_diag.c src/can_diag.c src/dtc_status.c src/logger.c $(UNITY_DIR)/unity.c
TEST_CAN_DIAG_TARGET = test_can_diag

# can_fault.c の動作確認用テスト（Drop判定・can_fault.txtのファイルパース確認、main.c は使わない）
# can_fault.cはcan.c（ラップ対象の送信関数）・faultmgr.c（警告灯データの送信元）・debounce.c/validate.c
# （can.cが依存）・timer.c（can.cのtimestamp取得）に依存する（Phase22）
TEST_CAN_FAULT_SRCS = test/test_can_fault.c sim/can_fault.c src/can.c src/faultmgr.c src/debounce.c src/timer.c src/validate.c src/logger.c $(UNITY_DIR)/unity.c
TEST_CAN_FAULT_TARGET = test_can_fault

# all/run/test/cleanは実ファイルを作らない疑似ターゲット。
# 特にtestはリポジトリ内の実在するtest/ディレクトリと名前が衝突するため、.PHONY宣言が無いと
# test/の更新日時がビルド済みテスト実行ファイルより新しい場合に「make: 'test' is up to date」と
# なり、テストが1つも実行されないまま終わってしまう
.PHONY: all run test clean coverage scenario

# デフォルトターゲット: make だけ打つとこれが実行される
all: $(TARGET)

# 実行ファイルのビルドルール
$(TARGET): $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

# テスト用実行ファイルのビルドルール（Unity(vendor/unity/)を使うためTEST_CFLAGSを使う。Phase13）
$(TEST_DIAG_TARGET): $(TEST_DIAG_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_DIAG_SRCS) -o $(TEST_DIAG_TARGET)

$(TEST_PERSIST_TARGET): $(TEST_PERSIST_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_PERSIST_SRCS) -o $(TEST_PERSIST_TARGET)

$(TEST_STATS_TARGET): $(TEST_STATS_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_STATS_SRCS) -o $(TEST_STATS_TARGET)

$(TEST_ALERT_TARGET): $(TEST_ALERT_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_ALERT_SRCS) -o $(TEST_ALERT_TARGET)

$(TEST_IGNITION_TARGET): $(TEST_IGNITION_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_IGNITION_SRCS) -o $(TEST_IGNITION_TARGET)

$(TEST_CMD_TARGET): $(TEST_CMD_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_CMD_SRCS) -o $(TEST_CMD_TARGET)

$(TEST_CONFIG_TARGET): $(TEST_CONFIG_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_CONFIG_SRCS) -o $(TEST_CONFIG_TARGET)

$(TEST_FIXTURE_TARGET): $(TEST_FIXTURE_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_FIXTURE_SRCS) -o $(TEST_FIXTURE_TARGET)

$(TEST_VALIDATE_TARGET): $(TEST_VALIDATE_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_VALIDATE_SRCS) -o $(TEST_VALIDATE_TARGET)

$(TEST_FAULTMGR_TARGET): $(TEST_FAULTMGR_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_FAULTMGR_SRCS) -o $(TEST_FAULTMGR_TARGET)

$(TEST_TIMER_TARGET): $(TEST_TIMER_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_TIMER_SRCS) -o $(TEST_TIMER_TARGET)

$(TEST_SCHEDULER_TARGET): $(TEST_SCHEDULER_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_SCHEDULER_SRCS) -o $(TEST_SCHEDULER_TARGET)

$(TEST_DEBOUNCE_TARGET): $(TEST_DEBOUNCE_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_DEBOUNCE_SRCS) -o $(TEST_DEBOUNCE_TARGET)

$(TEST_CAN_TARGET): $(TEST_CAN_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_CAN_SRCS) -o $(TEST_CAN_TARGET)

$(TEST_DTC_STATUS_TARGET): $(TEST_DTC_STATUS_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_DTC_STATUS_SRCS) -o $(TEST_DTC_STATUS_TARGET)

$(TEST_CAN_DIAG_TARGET): $(TEST_CAN_DIAG_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_CAN_DIAG_SRCS) -o $(TEST_CAN_DIAG_TARGET)

$(TEST_CAN_FAULT_TARGET): $(TEST_CAN_FAULT_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS) $(TEST_CAN_FAULT_SRCS) -o $(TEST_CAN_FAULT_TARGET)

# カバレッジ計測用のビルドルール（Phase23）。ソースは既存のTEST_X_SRCSをそのまま再利用し、
# --coverageを付けた別の実行ファイル（xxx_cov）としてビルドする
test_diag_cov: $(TEST_DIAG_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_DIAG_SRCS) -o test_diag_cov

test_persist_cov: $(TEST_PERSIST_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_PERSIST_SRCS) -o test_persist_cov

test_stats_cov: $(TEST_STATS_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_STATS_SRCS) -o test_stats_cov

test_alert_cov: $(TEST_ALERT_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_ALERT_SRCS) -o test_alert_cov

test_ignition_cov: $(TEST_IGNITION_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_IGNITION_SRCS) -o test_ignition_cov

test_cmd_cov: $(TEST_CMD_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_CMD_SRCS) -o test_cmd_cov

test_config_cov: $(TEST_CONFIG_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_CONFIG_SRCS) -o test_config_cov

test_fixture_cov: $(TEST_FIXTURE_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_FIXTURE_SRCS) -o test_fixture_cov

test_validate_cov: $(TEST_VALIDATE_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_VALIDATE_SRCS) -o test_validate_cov

test_faultmgr_cov: $(TEST_FAULTMGR_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_FAULTMGR_SRCS) -o test_faultmgr_cov

test_timer_cov: $(TEST_TIMER_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_TIMER_SRCS) -o test_timer_cov

test_scheduler_cov: $(TEST_SCHEDULER_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_SCHEDULER_SRCS) -o test_scheduler_cov

test_debounce_cov: $(TEST_DEBOUNCE_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_DEBOUNCE_SRCS) -o test_debounce_cov

test_can_cov: $(TEST_CAN_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_CAN_SRCS) -o test_can_cov

test_dtc_status_cov: $(TEST_DTC_STATUS_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_DTC_STATUS_SRCS) -o test_dtc_status_cov

test_can_diag_cov: $(TEST_CAN_DIAG_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_CAN_DIAG_SRCS) -o test_can_diag_cov

test_can_fault_cov: $(TEST_CAN_FAULT_SRCS) $(HEADERS)
	$(CC) $(TEST_CFLAGS_COV) $(TEST_CAN_FAULT_SRCS) -o test_can_fault_cov

# カバレッジ計測ターゲット: 17ターゲットそれぞれを実行し、担当モジュール（xxx_covに対応する1つの.c）の
# gcov結果（実行行数の割合）を表示する。ターゲット名と担当モジュール名の対応（1ユニット=1テストスイート）
COVERAGE_PAIRS = test_diag:diag test_persist:persist test_stats:stats test_alert:alert \
	test_ignition:ignition test_cmd:cmd test_config:config test_fixture:fixture \
	test_validate:validate test_faultmgr:faultmgr test_timer:timer test_scheduler:scheduler \
	test_debounce:debounce test_can:can test_dtc_status:dtc_status test_can_diag:can_diag \
	test_can_fault:can_fault

coverage: test_diag_cov test_persist_cov test_stats_cov test_alert_cov test_ignition_cov test_cmd_cov test_config_cov test_fixture_cov test_validate_cov test_faultmgr_cov test_timer_cov test_scheduler_cov test_debounce_cov test_can_cov test_dtc_status_cov test_can_diag_cov test_can_fault_cov
	@for pair in $(COVERAGE_PAIRS); do \
		target=$${pair%%:*}; \
		module=$${pair##*:}; \
		./$${target}_cov > /dev/null; \
		echo "--- $$target ($$module.c) ---"; \
		gcov $${target}_cov-$$module.gcno 2>&1 | grep "Lines executed"; \
	done

# 実行ターゲット: make run でビルド後に実行する
run: $(TARGET)
	./$(TARGET)

# テストターゲット: make test でtest_diag・test_persist・test_stats・test_alert・test_ignition・test_cmd・test_config・test_fixture・test_validate・test_faultmgr・test_timer・test_scheduler・test_debounce・test_can・test_dtc_status・test_can_diag・test_can_faultをビルドして全て実行する
test: $(TEST_DIAG_TARGET) $(TEST_PERSIST_TARGET) $(TEST_STATS_TARGET) $(TEST_ALERT_TARGET) $(TEST_IGNITION_TARGET) $(TEST_CMD_TARGET) $(TEST_CONFIG_TARGET) $(TEST_FIXTURE_TARGET) $(TEST_VALIDATE_TARGET) $(TEST_FAULTMGR_TARGET) $(TEST_TIMER_TARGET) $(TEST_SCHEDULER_TARGET) $(TEST_DEBOUNCE_TARGET) $(TEST_CAN_TARGET) $(TEST_DTC_STATUS_TARGET) $(TEST_CAN_DIAG_TARGET) $(TEST_CAN_FAULT_TARGET)
	./$(TEST_DIAG_TARGET)
	./$(TEST_PERSIST_TARGET)
	./$(TEST_STATS_TARGET)
	./$(TEST_ALERT_TARGET)
	./$(TEST_IGNITION_TARGET)
	./$(TEST_CMD_TARGET)
	./$(TEST_CONFIG_TARGET)
	./$(TEST_FIXTURE_TARGET)
	./$(TEST_VALIDATE_TARGET)
	./$(TEST_FAULTMGR_TARGET)
	./$(TEST_TIMER_TARGET)
	./$(TEST_SCHEDULER_TARGET)
	./$(TEST_DEBOUNCE_TARGET)
	./$(TEST_CAN_TARGET)
	./$(TEST_DTC_STATUS_TARGET)
	./$(TEST_CAN_DIAG_TARGET)
	./$(TEST_CAN_FAULT_TARGET)

# シナリオ検証ターゲット（Phase25）: sensor_simをPythonから実行し、docs/scenarios.mdの期待結果と照合する。
# 1条件につきsensor_simを最後まで動かす（約20秒）ため、make testには含めず別ターゲットにしている
scenario: $(TARGET)
	python3 -m unittest discover -s scenario_test -v

# クリーンターゲット: make clean で生成ファイルを削除する
clean:
	rm -f $(TARGET) $(TEST_DIAG_TARGET) $(TEST_PERSIST_TARGET) $(TEST_STATS_TARGET) $(TEST_ALERT_TARGET) $(TEST_IGNITION_TARGET) $(TEST_CMD_TARGET) $(TEST_CONFIG_TARGET) $(TEST_FIXTURE_TARGET) $(TEST_VALIDATE_TARGET) $(TEST_FAULTMGR_TARGET) $(TEST_TIMER_TARGET) $(TEST_SCHEDULER_TARGET) $(TEST_DEBOUNCE_TARGET) $(TEST_CAN_TARGET) $(TEST_DTC_STATUS_TARGET) $(TEST_CAN_DIAG_TARGET) $(TEST_CAN_FAULT_TARGET)
	rm -f *_cov *.gcno *.gcda *.gcov
