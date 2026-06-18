# コンパイラの指定
CC = gcc

# コンパイルオプション
# -Wall -Wextra: 警告を多く出す (バグの早期発見に役立つ)
# -std=c11: C11 規格でコンパイルする
# -Iinclude: include/ フォルダを #include の検索パスに追加する
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

# コンパイル対象のソースファイル
# モジュールを追加したときはここに追記する
SRCS = src/main.c src/sensor.c src/stats.c src/alert.c src/status.c

# 生成する実行ファイルの名前
TARGET = sensor_sim

# デフォルトターゲット: make だけ打つとこれが実行される
all: $(TARGET)

# 実行ファイルのビルドルール
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

# 実行ターゲット: make run でビルド後に実行する
run: $(TARGET)
	./$(TARGET)

# クリーンターゲット: make clean で生成ファイルを削除する
clean:
	rm -f $(TARGET)
