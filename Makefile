# コンパイラの指定
CC = gcc

# コンパイルオプション
# -Wall -Wextra: 警告を多く出す (バグの早期発見に役立つ)
# -std=c11: C11 規格でコンパイルする
# -Iinclude: include/ フォルダを #include の検索パスに追加する
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

# コンパイル対象のソースファイル
# モジュールを追加したときはここに追記する
SRCS = src/main.c src/sensor.c src/stats.c src/alert.c src/status.c src/diag.c src/logger.c src/ignition.c src/persist.c

# 生成する実行ファイルの名前
TARGET = sensor_sim

# diag.c の動作確認用テスト（固定値データ、main.c は使わない）
# test/test_common.c: test_diag.c / test_persist.c で共通のテスト補助関数（check/feedなど）
TEST_DIAG_SRCS = test/test_diag.c test/test_common.c src/status.c src/diag.c src/logger.c
TEST_DIAG_TARGET = test_diag

# persist.c の動作確認用テスト（ファイルI/Oの正常系・異常系、main.c は使わない）
TEST_PERSIST_SRCS = test/test_persist.c test/test_common.c src/status.c src/diag.c src/logger.c src/persist.c
TEST_PERSIST_TARGET = test_persist

# stats.c の動作確認用テスト（min/max/sum/countの更新、main.c は使わない）
# test_common.c が status_check/diag_check を参照するため status.c/diag.c も含める
TEST_STATS_SRCS = test/test_stats.c test/test_common.c src/status.c src/diag.c src/logger.c src/stats.c
TEST_STATS_TARGET = test_stats

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

# 実行ターゲット: make run でビルド後に実行する
run: $(TARGET)
	./$(TARGET)

# テストターゲット: make test でtest_diag・test_persist・test_statsをビルドして全て実行する
test: $(TEST_DIAG_TARGET) $(TEST_PERSIST_TARGET) $(TEST_STATS_TARGET)
	./$(TEST_DIAG_TARGET)
	./$(TEST_PERSIST_TARGET)
	./$(TEST_STATS_TARGET)

# クリーンターゲット: make clean で生成ファイルを削除する
clean:
	rm -f $(TARGET) $(TEST_DIAG_TARGET) $(TEST_PERSIST_TARGET) $(TEST_STATS_TARGET)
