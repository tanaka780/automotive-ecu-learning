# コマンド集

READMEの「実行方法」に載せていない、確かめと手入れのためのコマンドをまとめる。どれもリポジトリの一番上のフォルダで実行する。

## 依存の向きだけを確かめる

`make`は、実行ファイルを作る前に依存の向き（ECUのフォルダ → `common/` → `mcal_pc/`）も確かめ、決まりに反したヘッダを読んでいるとそこで止まる（Phase31）。依存の向きだけを確かめる:

```bash
make check_deps
```

## 単体テストの対象

テスト（diag.c・persist.c・stats.c・alert.c・ignition.c・cmd.c・config.c・fixture.c・validate.c・faultmgr.c・timer.c・scheduler.c・debounce.c・can.c・dtc_status.c・can_diag.c・can_fault.c・fan_ctrl.c・fan_ecu.c・uds.c・uds_tester.c・task_stall.c の動作確認、固定値データ使用。Phase13より[Unity](https://github.com/ThrowTheSwitch/Unity)形式に統一）:

```bash
make test
```

## テストカバレッジ

テストカバレッジ計測（Phase23。22テストターゲットそれぞれについて、担当モジュール1つのgcov実行行数割合を表示。`lcov`等によるプロジェクト全体の合算は未導入）:

```bash
make coverage
```

## シナリオ検証の詳細

シナリオ検証（Phase25。`sensor_sim`をPythonから実行し、[scenarios.md](scenarios.md)の期待結果と照合する。現在は設定ファイル異常時のフェイルセーフ・故障発生（Degraded確定まで）・オーバーヒート・電源再投入・冷却ファン制御・水温センサ故障時の冷却ファン制御・エンジンを切ってかけ直したときのファン制御・整備工場での診断（UDS）。`sensor_sim`の1回の実行に約20秒かかり、全体で7回実行するため約2分15秒かかる。冷却ファンの4条件（秒単位で照合するため）・診断（UDS）の4条件（送ったサンプルまで照合するため）と、故障発生のうち車速だけ以外の条件・電源再投入のうち水温の記録を3回続けて引き継ぐ条件以外（出力が毎回同じで速いため）は、`sensor_sim_virtual`で動かし、数ミリ秒で終わる）:

```bash
make scenario
```

## シナリオ検証の一部だけを動かす

シナリオ検証のうち、名前に指定した文字列を含むテストだけを実行する（例：`test_runs_to_the_end`。`make scenario`と違い`sensor_sim`・`sensor_sim_virtual`をビルドしないため、先に`make`と`make sensor_sim_virtual`を実行しておく）:

```bash
python3 -m unittest discover -s scenario_test -k test_runs_to_the_end
```

## 診断（UDS）の要求を送る

診断（UDS）の要求を送る（Phase30。作業用のフォルダに`fixture.txt`と`uds_request.txt`を置いて仮想時間版を動かし、スキャンツール役の表示と記録の消去だけを見る。この例では、3秒目に記録の件数を読み、5秒目に消してから読み直す）:

```bash
make sensor_sim_virtual && R=$(pwd) && W=$(mktemp -d) && printf 'MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=115\nIGNITION=ON\n' > $W/fixture.txt && printf 'REQ@3=19 01 FF\nREQ@5=14 FF FF FF\nREQ@5=19 01 FF\n' > $W/uds_request.txt && (cd $W && $R/sensor_sim_virtual < /dev/null) | grep -E '^\[(TESTER|DTC)\]'
```

## 「実際に動かした結果」の図の作り直し

「実際に動かした結果」の図（`docs/images/fan_run_example.svg`）の作り直し（`tools/fan_run_example_fixture.txt`の条件で仮想時間版を動かし、その出力から`tools/plot_fan_timeline.py`がSVGを描く。作業用のフォルダで動かすため、リポジトリ直下の記録ファイル（`dtc_data.txt`など）は上書きしない）:

```bash
make sensor_sim_virtual && R=$(pwd) && W=$(mktemp -d) && cp tools/fan_run_example_fixture.txt $W/fixture.txt && (cd $W && $R/sensor_sim_virtual < /dev/null) | python3 tools/plot_fan_timeline.py > docs/images/fan_run_example.svg
```

## 再生画面のデータの作り直し

再生画面（`viewer/index.html`）のデータの作り直し（`tools/make_viewer_data.py`が21の場面ごとに作業用のフォルダで仮想時間版を動かし、その出力を1秒ごとの画面の情報にまとめて`viewer/index.html`の中に書き込む。場面ごとの期待する出来事と1つでも食い違えば、書き込まずに止まる。リポジトリ直下の記録ファイル（`dtc_data.txt`など）は上書きしない）:

```bash
make sensor_sim_virtual && python3 tools/make_viewer_data.py
```

## 再生画面の画像の撮り直し

READMEの冒頭の再生画面の画像（`docs/images/viewer_comm_example.png`）の撮り直し（WSLから、Windowsに入っているEdgeを画面なしで動かして撮る。作業用のフォルダに再生画面のコピーを作り、開いたときの場面と秒だけを「通信の途切れ」の10秒目に書き換える。書き換えられなかったときは、撮らずに止まる）:

```bash
W=$(mktemp -d) && sed 's/var scene = 0, index = 0,/var scene = 4, index = 9,/' viewer/index.html > $W/shot.html && grep -q 'var scene = 4, index = 9,' $W/shot.html && "/mnt/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe" --headless=new --disable-gpu --hide-scrollbars --window-size=1000,1180 --screenshot="$(wslpath -w $W/shot.png)" "$(wslpath -w $W/shot.html)" && cp $W/shot.png docs/images/viewer_comm_example.png
```
