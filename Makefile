# コンパイラの指定
CC = gcc

# コンパイルオプション
# -Wall -Wextra: 警告を多く出す (バグの早期発見に役立つ)
# -std=c11: C11 規格でコンパイルする
# -Iinclude: include/ フォルダを #include の検索パスに追加する
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

# コンパイル対象のソースファイル
# モジュールを追加したときはここに追記する
SRCS = src/main.c src/sensor.c src/stats.c src/alert.c src/status.c src/diag.c src/logger.c src/ignition.c src/persist.c src/cmd.c src/config.c src/fixture.c

# 生成する実行ファイルの名前
TARGET = sensor_sim

# diag.c の動作確認用テスト（固定値データ、main.c は使わない）
# test/test_common.c: test_diag.c / test_persist.c で共通のテスト補助関数（check/feedなど）
# test_common.c が内部でtest_default_config（config_init）を使うため src/config.c も含める
TEST_DIAG_SRCS = test/test_diag.c test/test_common.c src/status.c src/diag.c src/logger.c src/config.c
TEST_DIAG_TARGET = test_diag

# persist.c の動作確認用テスト（ファイルI/Oの正常系・異常系、main.c は使わない）
TEST_PERSIST_SRCS = test/test_persist.c test/test_common.c src/status.c src/diag.c src/logger.c src/persist.c src/config.c
TEST_PERSIST_TARGET = test_persist

# stats.c の動作確認用テスト（min/max/sum/countの更新、main.c は使わない）
# test_common.c が status_check/diag_check を参照するため status.c/diag.c も含める
TEST_STATS_SRCS = test/test_stats.c test/test_common.c src/status.c src/diag.c src/logger.c src/stats.c src/config.c
TEST_STATS_TARGET = test_stats

# alert.c の動作確認用テスト（標準出力キャプチャによる警告出力の確認、main.c は使わない）
# test_common.c が status_check/diag_check を参照するため status.c/diag.c も含める
TEST_ALERT_SRCS = test/test_alert.c test/test_common.c src/status.c src/diag.c src/logger.c src/alert.c src/config.c
TEST_ALERT_TARGET = test_alert

# ignition.c の動作確認用テスト（標準出力キャプチャによる遷移イベント出力の確認、main.c は使わない）
# test_common.c が status_check/diag_check を参照するため status.c/diag.c も含める
TEST_IGNITION_SRCS = test/test_ignition.c test/test_common.c src/status.c src/diag.c src/logger.c src/ignition.c src/config.c
TEST_IGNITION_TARGET = test_ignition

# cmd.c の動作確認用テスト（diag_clear・cmd_dispatchの確認、main.c は使わない）
# test_common.c が status_check/diag_check を参照するため status.c/diag.c も含める
TEST_CMD_SRCS = test/test_cmd.c test/test_common.c src/status.c src/diag.c src/logger.c src/cmd.c src/config.c
TEST_CMD_TARGET = test_cmd

# config.c の動作確認用テスト（config_loadのファイルパースの正常系・異常系の確認、main.c は使わない）
# test_common.c が status_check/diag_check を参照するため status.c/diag.c も含める
TEST_CONFIG_SRCS = test/test_config.c test/test_common.c src/status.c src/diag.c src/logger.c src/config.c
TEST_CONFIG_TARGET = test_config

# デフォルトターゲット: make だけ打つとこれが実行される
all: $(TARGET)

# 実行ファイルのビルドルール
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

# テスト用実行ファイルのビルドルール
$(TEST_DIAG_TARGET): $(TEST_DIAG_SRCS)
	$(CC) $(CFLAGS) $(TEST_DIAG_SRCS) -o $(TEST_DIAG_TARGET)

$(TEST_PERSIST_TARGET): $(TEST_PERSIST_SRCS)
	$(CC) $(CFLAGS) $(TEST_PERSIST_SRCS) -o $(TEST_PERSIST_TARGET)

$(TEST_STATS_TARGET): $(TEST_STATS_SRCS)
	$(CC) $(CFLAGS) $(TEST_STATS_SRCS) -o $(TEST_STATS_TARGET)

$(TEST_ALERT_TARGET): $(TEST_ALERT_SRCS)
	$(CC) $(CFLAGS) $(TEST_ALERT_SRCS) -o $(TEST_ALERT_TARGET)

$(TEST_IGNITION_TARGET): $(TEST_IGNITION_SRCS)
	$(CC) $(CFLAGS) $(TEST_IGNITION_SRCS) -o $(TEST_IGNITION_TARGET)

$(TEST_CMD_TARGET): $(TEST_CMD_SRCS)
	$(CC) $(CFLAGS) $(TEST_CMD_SRCS) -o $(TEST_CMD_TARGET)

$(TEST_CONFIG_TARGET): $(TEST_CONFIG_SRCS)
	$(CC) $(CFLAGS) $(TEST_CONFIG_SRCS) -o $(TEST_CONFIG_TARGET)

# 実行ターゲット: make run でビルド後に実行する
run: $(TARGET)
	./$(TARGET)

# テストターゲット: make test でtest_diag・test_persist・test_stats・test_alert・test_ignition・test_cmd・test_configをビルドして全て実行する
test: $(TEST_DIAG_TARGET) $(TEST_PERSIST_TARGET) $(TEST_STATS_TARGET) $(TEST_ALERT_TARGET) $(TEST_IGNITION_TARGET) $(TEST_CMD_TARGET) $(TEST_CONFIG_TARGET)
	./$(TEST_DIAG_TARGET)
	./$(TEST_PERSIST_TARGET)
	./$(TEST_STATS_TARGET)
	./$(TEST_ALERT_TARGET)
	./$(TEST_IGNITION_TARGET)
	./$(TEST_CMD_TARGET)
	./$(TEST_CONFIG_TARGET)

# クリーンターゲット: make clean で生成ファイルを削除する
clean:
	rm -f $(TARGET) $(TEST_DIAG_TARGET) $(TEST_PERSIST_TARGET) $(TEST_STATS_TARGET) $(TEST_ALERT_TARGET) $(TEST_IGNITION_TARGET) $(TEST_CMD_TARGET) $(TEST_CONFIG_TARGET)
