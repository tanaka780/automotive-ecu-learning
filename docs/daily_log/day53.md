# Day53

## 実施内容

- `fixture.c`・`fixture.h`・`can_fault.c`・`can_fault.h`を`git mv`で`sim/`へ移動した（中身は変更なし）
- `Makefile`の`SRCS`を`ECU_SRCS`（`src/`）と`SIM_SRCS`（`sim/`）に分け、`CFLAGS`に`-Isim`を追加した。`TEST_FIXTURE_SRCS`・`TEST_CAN_FAULT_SRCS`のパスも`sim/`に直した

## 確認内容

- 移動の前後で、ビルド・単体テスト・カバレッジ・MISRAチェック・シナリオ検証の結果が変わらないか
- `-Isim`が無いと何がビルドできなくなるか
- `make scenario`では通らない`fixture.c`・`can_fault.c`が、`sensor_sim`の中で移動前と同じように動くか

## 実行結果

| コマンド | 結果 |
| --- | --- |
| `make clean && make` | 警告0件 |
| `make test` | 17ターゲット全てPASS（件数も移動前と同じ） |
| `make coverage` | 全17ターゲットの割合が移動前と同じ（`fixture.c` 94.29%、`can_fault.c` 75.58%）。`.gcno`のファイル名も`test_fixture_cov-fixture.gcno`のまま変わらなかった |
| cppcheck（`--addon=misra --enable=all --std=c11 -Iinclude -Isim src/ sim/`） | ルールごとの件数が移動前（`src/`のみ）と同じ（11.5×4・15.5×69・21.1×2・21.10×3・21.6×16・8.7×1） |
| `make scenario` | 11テスト全てOK、約57秒 |
| `-Isim`なしで`sim/fixture.c`だけをコンパイル | 通る |
| `-Isim`なしで`src/main.c`をコンパイル | `fixture.h: No such file or directory`で失敗 |
| 一時ディレクトリに`fixture.txt`（`MODE=FIXED`、SPEED=50）と`can_fault.txt`（`MODE=DROP`、ENGINE_STATUS）を置いて実行 | `[FIXTURE] Loaded fixture file (FIXED mode)`、センサ値は毎回`Speed: 50`、`[CAN] EngineStatus link lost`が出た |

| コミット後のリポジトリを一時ディレクトリにcloneし、上記を全てやり直し | 警告0件、17ターゲット・108テスト全てPASS、カバレッジ・cppcheckの件数は上と同じ、`make scenario` OK。`fixture.txt`・`can_fault.txt`の確認も一時ディレクトリで同じ結果 |

- `make scenario`の前後で、リポジトリ直下の`dtc_data.txt`の更新日時は変わらなかった
- 最初の`fixture.txt`・`can_fault.txt`の確認は、一時ディレクトリで実行したつもりがホームディレクトリで実行されていた（`wsl.exe`に渡したコマンドの`$変数`がWSL側に届く前に空に展開されていた）。WSL側でスクリプトとして実行する形に直し、cloneしたリポジトリと本物の一時ディレクトリでやり直した

## 判定

期待通り。移動前と比べられる項目はすべて一致し、移動した2つのモジュールも`sensor_sim`の中で動いていた。

## 今回の設計方針

- 「`.c`・`.h`をまとめて`sim/`へ移す」案が間違いだと仮定して、移動しない案・依存の向きを先に直す案（ビルドごとにラッパーと素通し版を差し替える）・`fixture.c`だけ移す案と比べ直した。依存を直す案は、差し替え先になるtarget向けビルドがまだ無く、構造の変更と動作の変更も混ざる。`fixture_apply`も`main.c`から毎回呼ばれているので、片方だけ移す理由も立たない
- `-Isim`は`CFLAGS`全体に入るので、`src/`のどのファイルからでも`sim/`のヘッダを読めてしまう。今回分けたのは置き場所だけで、依存はそのまま残っている（`main.c`が`sim/`の関数を直接呼ぶので、`sim/`を外すとビルドできない）。これを見える形にするため、Makefileで`ECU_SRCS`と`SIM_SRCS`を分けた
- `sim/fixture.c`の`#include "fixture.h"`が`-Isim`なしで通るのは、`"..."`形式のincludeがまずそのファイル自身のフォルダを探すから。`main.c`は`src/`にあるので見つからない
- `make scenario`は`fixture.txt`・`can_fault.txt`を使わないので、これだけでは移動したモジュールの動作を確かめられない。両方のファイルを置いて手で1回実行した

## 次回やること

