# Day53

## 実施内容

- `fixture.c`・`fixture.h`・`can_fault.c`・`can_fault.h`を`git mv`で`sim/`へ移動した（中身は変更なし）
- `Makefile`の`SRCS`を`ECU_SRCS`（`src/`）と`SIM_SRCS`（`sim/`）に分け、`CFLAGS`に`-Isim`を追加した。`TEST_FIXTURE_SRCS`・`TEST_CAN_FAULT_SRCS`のパスも`sim/`に直した
- `fixture.txt`に`IGNITION=ON/OFF`を書くと、イグニッションを実行中ずっと固定できるようにした
  - `ignition.c`に`ignition_set`を追加した（指定した状態で更新し、直前の状態は`previous`に残す）
  - `sim/fixture.c`に`fixture_load_ignition`を追加した（同じファイルからIGNITION行だけを読む。既存の`fixture_apply`は変更なし）
  - `main.c`は起動時に`fixture_load_ignition`を呼び、trueなら`run_sample_cycle`で`ignition_update`の代わりに`ignition_set`を呼ぶ
  - `test_ignition.c`に2テスト、`test_fixture.c`に5テストを追加した
- `scenario_test/test_config_failsafe.py`の中にあった`sensor_sim`の実行関数を、`scenario_test/sensor_sim_runner.py`へ切り出した（指定したディレクトリで実行する`run_in_dir`を分け、`run_sensor_sim`はそれを一時ディレクトリで呼ぶ形にした）
- 切り出し後、リポジトリ直下から`python3 -m unittest scenario_test.test_config_failsafe`のようにモジュール名で実行すると`sensor_sim_runner`が見つからなくなっていたため、`test_config_failsafe.py`の先頭でこのフォルダを`sys.path`に加えるようにした（一度`scenario_test/__init__.py`で対応したが、引数なしの`python3 -m unittest`の挙動まで変わったため削除した）。READMEに、名前を指定して1テストだけ実行する方法（`discover -k`）を書き足した
- `scenario_test/test_fault_occurrence.py`を新規作成した（故障発生シナリオを、`fixture.txt`で水温だけCRITICAL・車速と水温が同時にCRITICALの2条件にして、イグニッションON固定で実行し、期待結果と照合する）。`docs/scenarios.md`の備考に、自動検証している範囲と対象外の項目を書き足した
- 故障発生のテストのうち`[METER]`の行を照合する2つに、Phase25の対象外（メーター表示）に当たること、3ECU化で見直しが必要なことを書き足した
- `scenario_test/test_power_cycle.py`を新規作成した（同じ一時ディレクトリで`sensor_sim`を3回続けて起動し、電源再投入シナリオの期待結果と照合する）
- `docs/scenarios.md`の電源再投入の期待結果に、「再起動後もCRITICALなら発生回数がもう1つ増える」「フリーズフレームは引き継がれ、上書きされない」の2行を追加した

## 確認内容

- 移動の前後で、ビルド・単体テスト・カバレッジ・MISRAチェック・シナリオ検証の結果が変わらないか
- `-Isim`が無いと何がビルドできなくなるか
- `make scenario`では通らない`fixture.c`・`can_fault.c`が、`sensor_sim`の中で移動前と同じように動くか
- `IGNITION=ON`で20サンプル全てONになり、遷移ログが最初の1回だけ出て、最後に標準入力を待たずに終わるか
- `IGNITION`の行が無いときは、今まで通りランダムに動くか。既存の`make scenario`の結果が変わらないか
- 追加したテストが、実装の誤りを本当に検出できるか
- 実行関数の切り出しの前後で、シナリオ検証のテスト名・結果と、実行の仕方（`make scenario`・ファイルの直接実行・モジュール名での実行・`discover -k`・引数なしの`python3 -m unittest`）ごとの結果が変わらないか
- 故障発生シナリオの期待結果（警告・CRITICAL表示・DTC・フリーズフレーム・3回連続でのDegraded・確定後のフェイルセーフ値・複数センサの同時発生）と、実際の出力が一致するか。CRITICALが続く間にRecoveredが出ないか
- 確定までの回数をわざと変えたCや、間違えた期待値を、テストが検出するか
- 電源再投入で、1回目に記録したDTC・フリーズフレームが2回目の起動直後に引き継がれているか。再起動後もCRITICALのとき発生回数がどうなるか、フリーズフレームが上書きされるか
- 保存しない・前回状態をNORMALに戻さない・フリーズフレームを毎回上書きする、の3通りに壊したCを、テストがそれぞれ検出するか

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
| 移動のコミット後のリポジトリを一時ディレクトリにcloneし、上記を全てやり直し | 警告0件、17ターゲット・108テスト全てPASS、カバレッジ・cppcheckの件数は上と同じ、`make scenario` OK。`fixture.txt`・`can_fault.txt`の確認も一時ディレクトリで同じ結果 |
| イグニッション固定の追加後、`make clean && make` | 警告0件 |
| 同、`make test` | 17ターゲット・115テスト全てPASS（`test_ignition` 6、`test_fixture` 10） |
| 同、`make coverage` | `ignition.c` 47.83%→70.37%、`fixture.c` 94.29%→96.83% |
| 同、cppcheck | 15.5（単一出口）だけ69→75件。追加した2関数の早期return分で、それ以外のルールの件数は変わらない |
| 同、`make scenario` | 11テスト全てOK |
| `fixture.txt`に`IGNITION=ON`を書いて実行 | `[FIXTURE] Ignition fixed: ON`、`[IGN] ON`が20回、`[IGN] OFF -> ON`が1回だけ、最後に`[CMD] Not accepted (ignition ON)`で終了 |
| `IGNITION=OFF`で実行 | `[IGN] OFF`が20回、遷移なし、最後は`Enter command`（標準入力はEOFなのでそのまま終了） |
| `IGNITION`の行なしで実行 | `[FIXTURE] Ignition fixed`は出ず、ON 9回・OFF 11回・遷移13回（今まで通りランダム） |
| コピーしたリポジトリで実装をわざと間違えてテスト | `ignition_set`が`previous`を更新しない→`test_ignition`で2件FAIL、小文字の`on`も受け付ける→1件FAIL、後勝ちではなく先勝ち→1件FAIL |
| 実行関数の切り出し後、`unittest`が見つけるテスト名の一覧 | 切り出し前（HEAD）と同じ11件。`sensor_sim_runner.py`はテストとして拾われていない |
| 同、`make scenario` | 11テスト全てOK |
| 同、コピーしたリポジトリで期待値の閾値を`speed=101`に書き換えて実行 | 2件FAIL（`DEFAULT_CONFIG_LINE`を使う2テスト）。実行関数を切り出しても、間違った期待値は検出される |
| 切り出しのコミット後にcloneして、`make scenario`以外の実行方法も確認 | ファイルを直接実行（`python3 scenario_test/test_config_failsafe.py`）はOK。リポジトリ直下からモジュール名で実行すると`ModuleNotFoundError: No module named 'sensor_sim_runner'`（切り出し前は動いていた） |
| `__init__.py`追加のコミット後にcloneして確認 | モジュール名での実行・ファイルの直接実行・`discover -k test_runs_to_the_end`（2テスト）がすべてOK。`make scenario`は11テストOK、テスト数は11件のままで`__init__.py`・`sensor_sim_runner.py`はテストとして拾われていない |
| 同、リポジトリ直下で引数なしの`python3 -m unittest` | 11件見つかるようになっていた（切り出し前は0件） |
| `__init__.py`を削除し、テストファイルの先頭で`sys.path`に足す形にしたコミット後、切り出し前（`90a0cf4`）と今を、それぞれcloneして比較 | テスト名11件が完全に一致。引数なしの`unittest`は両方0件、直接実行・モジュール名での実行・`discover -k`（2テスト）は両方OK。`make scenario`は11テストOK |
| `fixture.txt`（SPEED=50・RPM=2000・TEMP=95・`IGNITION=ON`）で実行 | サンプル1から`[ALERT] Temp`と`Temp: CRITICAL`、サンプル3で`[FAULT] Degraded: Temp`。`[ALERT]`はサンプル1・2だけ、メーターはサンプル3からtemp=25・FaultStatusのtemp=1。統計は`Temp : min= 25  max= 95  avg= 32 C`、DTCは`Temp ACTIVE 1`、フリーズフレームは生の値（95） |
| SPEED=110・TEMP=95で実行 | サンプル3で`Degraded: Speed`と`Degraded: Temp`、メーターは`speed=0 rpm=2000 temp=25`、DTCはSpeed・Tempの両方がACTIVE 1、フリーズフレームの原因は`Speed` |
| 故障発生テスト追加後、`make scenario` | 26テスト（既存11＋追加15）全てOK、約96秒 |
| 同、故障発生のテストだけを2回続けて実行 | 2回とも15テストOK |
| 同、モジュール名で実行（`python3 -m unittest scenario_test.test_fault_occurrence.MultiSensorFaultTest`） | OK |
| コピーしたリポジトリで`FAULTMGR_DEBOUNCE_COUNT`を3→2に変えて実行 | 5件FAIL（Degradedの時期・`[ALERT]`の出たサンプル・メーター表示・統計の平均）。rpmが生の値のままかを見るテストは、確定の時期に関係しないのでOK |
| 同、フリーズフレームの原因の期待値を`Temp`に書き換えて実行 | 該当の1件だけFAIL |
| 同じ一時ディレクトリで3回続けて実行（1回目：`config.txt`あり・水温95でON、2回目：OFF固定、3回目：SPEED=60・TEMP=99でON） | 1回目：`No saved data (first run)`・POSTは`did not pass`・`Temp ACTIVE 1`。2回目：`Loaded saved DTC data`・POSTは`passed`・`Temp ACTIVE 1`・フリーズフレームは1回目と同じ。3回目：`Temp ACTIVE 2`・フリーズフレームは1回目の50/95のまま |
| 電源再投入テスト追加後、`make scenario` | 34テスト（既存26＋追加8）全てOK、約153秒 |
| 同、モジュール名で実行（`python3 -m unittest scenario_test.test_power_cycle`） | OK。引数なしの`unittest`は0件のまま |
| コピーしたリポジトリで、保存処理の`fopen`を`NULL`に置き換えて実行 | 6件FAIL（保存・引き継ぎ・POST・発生回数・フリーズフレームに関わるもの全て） |
| 同、読み込み時に前回状態を`LEVEL_CRITICAL`にして実行 | `test_occurrence_counted_again_after_reboot`の1件だけFAIL |
| 同、フリーズフレームを記録済みでも上書きするようにして実行 | `test_freeze_frame_not_overwritten_after_reboot`の1件だけFAIL |
| 同、発生回数の期待値を2→1に書き換えて実行 | 該当の1件だけFAIL |

- `make scenario`の前後で、リポジトリ直下の`dtc_data.txt`の更新日時は変わらなかった
- 最初の`fixture.txt`・`can_fault.txt`の確認は、一時ディレクトリで実行したつもりがホームディレクトリで実行されていた（`wsl.exe`に渡したコマンドの`$変数`がWSL側に届く前に空に展開されていた）。WSL側でスクリプトとして実行する形に直し、cloneしたリポジトリと本物の一時ディレクトリでやり直した
- イグニッション固定のコミット後もcloneして全てやり直し、同じ結果になった。追加で、改行がCRLFの`fixture.txt`（Windowsで編集した場合）・`MODE=RANDOM`+`IGNITION=ON`・`IGNITION=on`（不正な値）でも想定通りに動くことを確認した。README・project_contextに「`sensor_sim`本体は未変更」という記述が残っていたので、設定ファイル異常のシナリオについての記述だと分かる形に直した

## 判定

期待通り。

- 移動：移動前と比べられる項目はすべて一致し、移動した2つのモジュールも`sensor_sim`の中で動いていた
- イグニッション固定：ON・OFF・行なしの3通りとも想定した動きになり、既存のシナリオ検証の結果も変わらなかった。追加したテストは、わざと入れた誤りを3つとも検出した
- 実行関数の切り出し：最終的な形（テストファイルの先頭で`sys.path`に足す）で、テスト名・結果・5通りの実行の仕方すべてが切り出し前と一致した。途中の`__init__.py`の形は、引数なしの`unittest`の挙動が変わっていたため期待通りではなく、取りやめた
- 故障発生：2条件とも期待結果と出力が一致し、確定までの回数を変えたCと間違えた期待値をどちらも検出した
- 電源再投入：3回の起動とも期待結果と出力が一致した。3通りに壊したCは、それぞれ対応するテストが検出した（前回状態とフリーズフレームの壊し方は、今回追加した期待結果のテストだけが落ちた）

## 今回の設計方針

- 「`.c`・`.h`をまとめて`sim/`へ移す」案が間違いだと仮定して、移動しない案・依存の向きを先に直す案（ビルドごとにラッパーと素通し版を差し替える）・`fixture.c`だけ移す案と比べ直した。依存を直す案は、差し替え先になるtarget向けビルドがまだ無く、構造の変更と動作の変更も混ざる。`fixture_apply`も`main.c`から毎回呼ばれているので、片方だけ移す理由も立たない
- `-Isim`は`CFLAGS`全体に入るので、`src/`のどのファイルからでも`sim/`のヘッダを読めてしまう。今回分けたのは置き場所だけで、依存はそのまま残っている（`main.c`が`sim/`の関数を直接呼ぶので、`sim/`を外すとビルドできない）。これを見える形にするため、Makefileで`ECU_SRCS`と`SIM_SRCS`を分けた
- `sim/fixture.c`の`#include "fixture.h"`が`-Isim`なしで通るのは、`"..."`形式のincludeがまずそのファイル自身のフォルダを探すから。`main.c`は`src/`にあるので見つからない
- `make scenario`は`fixture.txt`・`can_fault.txt`を使わないので、これだけでは移動したモジュールの動作を確かめられない。両方のファイルを置いて手で1回実行した
- イグニッション固定の入口は、`fixture.txt`のキーにする案のほか、乱数のシードを固定する案、サンプルごとの並びを指定する案、Pythonが標準入力で1サンプルずつ送る案、乱数で状態を決める処理を`sim/`へ移してから固定を足す案と比べた。シード固定は「なぜこのシードで全部ONになるのか」がテストから読めず、`rand`の呼び出しが1つ増えるだけで壊れる。並びの指定と標準入力は、今回のシナリオ（Degraded確定まで・電源再投入）には要らない。乱数を`sim/`へ移す案は、`sensor.c`の`rand`を残したまま`ignition`だけ移すことになり、Day49の「PC依存の関数を切り出すのはtarget対応時」とも合わない
- `ignition_set`は検証のための関数ではなく、実車のECUがイグニッション信号を入力として受け取る処理に近い。乱数で決める`ignition_update`の方がPC上のシミュレーション側の処理なので、target対応時に`ignition_update`を`sim/`へ移しても`ignition_set`はそのまま使える。今は`ignition_update`に手を入れていないので、`previous`を更新する2行が重複している
- `IGNITION`は`MODE`に関係なく、行があれば有効にした。`MODE=FIXED`の時だけにすると、新しい関数も`MODE`を読む必要があり、`fixture_apply`との重複が増える
- 今回のシナリオでは、イグニッションを固定すると、ループ終了時がONなので標準入力を待つ処理に入らない。Day51で挙げた「止まる」問題は、ONに固定する限り起きない
- 実行関数は最初「故障発生ではコピーし、電源再投入（3つ目）で共通化する」としていた。この判断が間違いだと仮定して比べ直すと、先送りしても、電源再投入の時に共通化（既存ファイルの変更）とテスト追加が同じタイミングに重なるだけだった。3つ目の使い方（同じディレクトリで2回実行する）も今日の計画で分かっているので、先に切り出しだけを行い、既存の11テストが同じ名前でOKのままなのを確かめてから故障発生のテストを足す順にした
- 切り出しで動かなくなったモジュール名での実行は、各テストファイルの先頭で`sys.path`に足す案・パッケージにして`discover -t .`に変える案・直さずに`discover -k`の使い方だけ書く案と比べた。各ファイルに書く案は、新しいテストファイルで書き忘れても`make scenario`では気付けない。パッケージにする案は、試すと直接実行が逆に動かなくなった。`__init__.py`の1か所で足せば、切り出し前に動いていた実行方法がすべて元に戻り、新しいテストファイルでは何も書かなくてよい、と考えて一度は`__init__.py`にした
- ところが`__init__.py`を置くと、リポジトリ直下で引数なしの`python3 -m unittest`がシナリオ検証を拾うようになった（0件→11件）。「前に動いていたものが動くか」だけを確かめて、「前に動いていなかったものが変わっていないか」を確かめていなかった。`__init__.py`を選んだ理由は「構造の変更で挙動を変えない」だったので、その基準を満たす、各テストファイルで足す形に戻した。書き忘れたときに壊れるのはモジュール名での実行だけで、READMEに書いた`make scenario`と`discover -k`には影響しない
- 故障発生は、シナリオの流れ（正常→CRITICAL→正常に戻る→OFF）のうち一部しか再現できない。全部を検証しているように見えないよう、テストのdocstringと`docs/scenarios.md`の備考に対象外の項目を書いた。そのうえで、CRITICALが続く間にRecoveredが出ないことは確認した
- 複数センサの条件は、1回の実行にまとめれば約20秒短くなる。ただ、シナリオの本筋（センサ1つ）とテストが対応しなくなり、落ちたときにどの期待結果の問題か切り分けにくいので、2回に分けた
- センサ値とイグニッションを固定すると、サンプルごとの出力も毎回同じになる。`[Sample NN]`ごとに行を分け、「3サンプル目でDegraded」「`[ALERT]`はサンプル1・2だけ」のように、何サンプル目に出たかまで照合した。統計の平均（95×2＋25×18）÷20＝32も、確定後にフェイルセーフ値が使われている証拠になる
- 閾値の判定は`>`なので、閾値ちょうど（temp=90）ではCRITICALにならない。水温は95にした
- 電源再投入の検討中に、Phase25の範囲（Day51）を読み直して、「3ECU化で作り直しになるメーター表示・通信故障は対象外」と決めていたことに気付いた。故障発生のテストのうち、`[METER]`の行を照合する2つはこれに当たる。作ったときも2回の再確認でも、範囲と照らし合わせていなかった。削除するとエンジン監視ECUが送るCANの値（フェイルセーフ値・故障ビット）を確かめる手段が無くなるので、範囲外と承知したうえで残し、3ECU化で見直すことをテストの説明に書いた
- 電源再投入の期待結果「2回目の起動直後、DTCが引き継がれている」は、DTCの一覧が最後にしか表示されないので直接は見えない。`main.c`に起動直後の表示を足す案は、検証のためにECU本体の出力を変えることになるので採らなかった。OFFの間は`diag_check`が呼ばれずDTCが変わらないので、2回目をOFF固定にして、最後の表示を起動直後の内容として扱った。CRITICALの再実行だけで確かめる案（発生回数が2なら引き継がれている）は1回分速いが、状態区分（ACTIVE）がCRITICALで設定し直されるので、引き継がれた証拠にならない
- 3回続けて実行して、「再起動後もCRITICALなら発生回数が2になる」「フリーズフレームは上書きされない」という、シナリオに書かれていない動きが見つかった。実装に合わせて仕様を書き足すと、実装の誤りを仕様として固めてしまうおそれがあるので、先に妥当かを確かめた。発生回数は、実車でもイグニッションON〜OFFの1サイクルごとに数えるのが一般的。フリーズフレームは、故障発生の期待結果「最初にCRITICALが発生した瞬間の値」と合う。どちらも妥当と判断してから、scenarios.mdに追加した
- 期待結果の「メーターECUの通信異常の記録は永続化していない」は、1回目に`can_fault.txt`を置けば確かめられる。ただ、これはPhase25で対象外とした通信故障に当たるうえ、機能ではなく「まだ永続化していない」という制約なので、テストにすると将来直す予定のものを固めてしまう。検証しないことをテストの説明に書いた
- 3回の実行を別々のクラスに分ける案（巻き込みが無い代わりに4回実行・約80秒）とも比べた。電源再投入は「前の起動の結果を次の起動が引き継ぐ」こと自体を見るシナリオなので、1つのクラスで続けて実行した

## 次回やること

