# コンパイラの指定
CC = gcc

# コンパイルオプション
# -Wall -Wextra: 警告を多く出す (バグの早期発見に役立つ)
# -std=c11: C11 規格でコンパイルする
# -Iinclude: include/ フォルダを #include の検索パスに追加する
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

# コンパイル対象のソースファイル
# モジュールを追加したときはここに追記する
SRCS = src/main.c src/sensor.c src/stats.c src/alert.c src/status.c src/diag.c src/logger.c

# 生成する実行ファイルの名前
TARGET = sensor_sim

# diag.c の動作確認用テスト（固定値データ、main.c は使わない）
TEST_SRCS = test/test_diag.c src/status.c src/diag.c src/logger.c
TEST_TARGET = test_diag

# デフォルトターゲット: make だけ打つとこれが実行される
all: $(TARGET)

# 実行ファイルのビルドルール
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

# テスト用実行ファイルのビルドルール
$(TEST_TARGET): $(TEST_SRCS)
	$(CC) $(CFLAGS) $(TEST_SRCS) -o $(TEST_TARGET)

# 実行ターゲット: make run でビルド後に実行する
run: $(TARGET)
	./$(TARGET)

# テストターゲット: make test でテスト用実行ファイルをビルドして実行する
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# クリーンターゲット: make clean で生成ファイルを削除する
clean:
	rm -f $(TARGET) $(TEST_TARGET)
