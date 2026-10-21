"""再生画面（viewer/index.html）が使うデータを作り、index.htmlの中（DATA-STARTとDATA-ENDの間）に書き込む。

使い方（リポジトリ直下で）:
    make sensor_sim_virtual && python3 tools/make_viewer_data.py

場面ごとに作業用のフォルダを作り、fixture.txt・can_fault.txtなどを置いてsensor_sim_virtualを動かし、
その出力を1秒（1サンプル）ごとの画面の情報と、起動時・終了時の出来事にまとめる。
読み取りはここで行い、画面側は表示するだけにする。C言語の本体には手を入れない。

場面ごとに「期待する出来事」を持たせ、実際の出力と照らし合わせる。1つでも食い違えば、データを書き込まずに止める
（画面の「確認結果」は、この照らし合わせの結果）。乱数で動かす場面は、車で起きる場面ではなく、
期待とも照らし合わせられないため置かない。
"""
import json
import os
import re
import subprocess
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SENSOR_SIM_VIRTUAL = os.path.join(REPO_ROOT, "sensor_sim_virtual")
OUTPUT = os.path.join(REPO_ROOT, "viewer", "index.html")
DATA_START = "<!-- DATA-START"   # index.htmlの中で、データを書き込む場所の目印
DATA_END = "<!-- DATA-END -->"

# どの場面にも置く、正常な設定ファイルと、記録が0件の記録ファイル（起動時の自己点検を合格させるため）。
# 記録ファイルの並びはpersist.cのdtc_to_textと同じ（記録4つ×回数・状態区分、フリーズフレーム5つ。Phase28で11個から13個にした）
CONFIG_OK = "LOG_LEVEL=0\n"
DTC_EMPTY = "0\n" * 13
# 全行が反映されない設定ファイル（scenario_test/test_config_failsafe.pyのBROKEN_CONFIGと同じ）
BROKEN_CONFIG = "ALERT_SPEED_MAX=abc\nGARBAGE\nSTATUS_TEMP_CRIT=999\n"


def fixture(temp=70, speed=50, ign="ON", extra=""):
    return f"MODE=FIXED\nSPEED={speed}\nRPM=2000\nTEMP={temp}\nIGNITION={ign}\n{extra}"


def can_fault(mode, target):
    return f"MODE={mode}\nTARGET={target}\nSTART_MS=5500\nEND_MS=10500\n"


# expectは(場所, 秒, 含まれる言葉)。場所は"sec"（その秒の出来事）・"start"（起動時）・"end"（終了時）・
# "not"（その秒の出来事に出ない。秒がNoneならどの秒にも出ない。Phase27）
SCENARIOS = [
    # 最初に開く場面。異常が無いときの普段の動きを見せ、ほかの場面と比べる基準にする
    {"id": "normal", "group": "main", "title": "正常な運転", "tagline": "異常が無ければ、表示するだけ",
     "situation": "速度50km/h・温度70℃で、普段どおり走っている。",
     "anomaly": "異常ではない（普段の動き）。",
     "response": "エンジンの見張り役は1秒ごとに速度・回転数・温度を確かめ、メーターに送る。どれも普段の範囲なので、警告も記録もしない。温度が低いのでファンは回さない。",
     "condition": "速度50km/h・回転数2000・温度70℃のまま、エンジンをかけて20秒動かす。",
     "files": {"fixture.txt": fixture()},
     "expect": [("start", None, "起動時の自己点検：合格"), ("sec", 1, "エンジンをかけた"),
                ("not", None, "警告"), ("not", None, "異常"), ("not", None, "ファンを回し始めた"),
                ("end", None, "温度センサーの記録：記録なし"), ("end", None, "オーバーヒートの記録：記録なし")]},
    {"id": "fan", "group": "main", "title": "冷却ファンの動き", "tagline": "熱ければ回し、冷えたら止める",
     "situation": "夏の渋滞で、エンジンの温度が上がったり下がったりする。",
     "anomaly": "異常ではない（普段の動き）。",
     "response": "95℃以上でファンを回し、90℃以下で止める。その間の温度では今の状態を保ち、細かくON/OFFを繰り返さない。",
     "condition": "温度を70℃→4秒目95℃→93℃→91℃→7秒目90℃→94℃→9秒目95℃に変える。",
     "files": {"fixture.txt": fixture(extra="TEMP@4=95\nTEMP@5=93\nTEMP@6=91\nTEMP@7=90\nTEMP@8=94\nTEMP@9=95\n")},
     "expect": [("sec", 4, "ファンを回し始めた"), ("sec", 7, "ファンを止めた"), ("sec", 9, "ファンを回し始めた")]},
    {"id": "sensor", "group": "main", "title": "温度センサーの異常", "tagline": "壊れても安全側に動く",
     "situation": "配線が切れて、温度センサーが、ありえない値（0℃）を出し続ける。",
     "anomaly": "温度センサーの値が、ありえない値（25〜130℃の範囲の外）のまま続く。",
     "response": "ありえない値になったら記録し、異常と判断するまでは直前の正しい温度を使う。3回続いたらセンサーを異常と判断して安全な値に置き換え、メーターは温度を「--」と表示する（本当の温度が分からないため）。安全のためファンを回し続ける。ありうる値が3回続けば回復と判断する。",
     "condition": "温度を70℃→4秒目100℃→7秒目に配線が切れる（0℃）→13秒目91℃→16秒目80℃に変える。",
     "files": {"fixture.txt": open(os.path.join(REPO_ROOT, "tools", "fan_run_example_fixture.txt")).read()},
     "expect": [("sec", 7, "温度がありえない値"), ("sec", 9, "温度のありえない値が3回続いた"), ("sec", 9, "温度を「--」と表示"),
                ("sec", 15, "温度がありうる値に戻った"), ("sec", 16, "ファンを止めた"), ("end", None, "温度センサーの記録：過去に異常")]},
    {"id": "overheat", "group": "main", "title": "オーバーヒート", "tagline": "本当の高温を知らせる",
     "situation": "重い荷物を積んで坂道を登り続け、エンジンが本当に熱くなる。温度センサーは壊れていない。",
     "anomaly": "温度が危険な高さ（110℃超え）になる。",
     "response": "ありうる値なので本当の温度として扱い、置き換えない。警告して記録し、メーターに高温の警告を出す。ファンは本当の温度で回す。温度が下がれば、高温の警告を消す。",
     "condition": "温度を70℃→4秒目100℃→7秒目118℃→13秒目91℃→16秒目80℃に変える。",
     "files": {"fixture.txt": fixture(extra="TEMP@4=100\nTEMP@7=118\nTEMP@13=91\nTEMP@16=80\n")},
     "expect": [("sec", 4, "ファンを回し始めた"), ("sec", 7, "温度が危険な値"), ("sec", 7, "メーター：高温の警告を表示"),
                ("sec", 13, "メーター：高温の警告を消した"), ("sec", 16, "ファンを止めた"), ("end", None, "オーバーヒートの記録：過去に異常")]},
    {"id": "comm", "group": "main", "title": "通信の途切れ", "tagline": "連絡が届かなくても安全側に動く",
     "situation": "エンジンの見張り役の不具合で、「速度・回転数・温度」の連絡だけが送られてこなくなる。",
     "anomaly": "「速度・回転数・温度」の連絡が届かない。",
     "response": "3回続けて届かなければ途切れと判断して記録する。メーターは最後に届いた値を「受け取れない」と示して表示し、ファン係は安全のためファンを回す。3回続けて届けば回復と判断する。",
     "condition": "温度は70℃のまま。5.5〜10.5秒の間、「速度・回転数・温度」の連絡を止める。",
     "files": {"fixture.txt": fixture(), "can_fault.txt": can_fault("DROP", "ENGINE_STATUS")},
     "expect": [("sec", 9, "メーター：速度・回転数・温度の連絡を受け取れなくなった"), ("sec", 9, "安全のためファンを回し始めた"),
                ("sec", 14, "メーター：速度・回転数・温度の連絡を受け取れるようになった"), ("sec", 14, "ファンを止めた"),
                ("end", None, "メーター：速度・回転数・温度の連絡を受け取れなかったこと 1回")]},
    {"id": "multi", "group": "other", "title": "複数のセンサーの異常",
     "situation": "共通の配線（電源やアース）の不具合で、速度センサーがおかしな値（止まっているのに110km/h）を、温度センサーがありえない値を、同時に出し続ける。",
     "anomaly": "速度が危険な値（100km/h超え）、温度がありえない値（25〜130℃の範囲の外）のまま続く。",
     "response": "センサーごとに別々に記録し、それぞれ異常と判断して安全な値に置き換える（速度は危険な値が3回、温度はありえない値が3回続いたとき）。",
     "condition": "速度を110km/hに、温度をショート（255℃）に固定する。",
     "files": {"fixture.txt": fixture(temp="SHORT", speed=110)},
     "expect": [("sec", 1, "速度が危険な値"), ("sec", 1, "温度がありえない値"),
                ("sec", 3, "速度の危険な値が3回続いた"), ("sec", 3, "温度のありえない値が3回続いた"),
                ("end", None, "速度の記録：今も異常"), ("end", None, "温度センサーの記録：今も異常")]},
    {"id": "overheat_fault", "group": "other", "title": "オーバーヒート中の温度センサーの異常",
     "situation": "エンジンが本当に熱くなっている途中で、温度センサーの配線がつながってしまい（ショート）、ありえない値（255℃）を出す。しばらくして直る。",
     "anomaly": "温度が危険な高さ（115℃）の途中で、温度センサーがありえない値になる。",
     "response": "高温の警告は、壊れる前の高温で判断して残す。センサーの異常の間、メーターは温度を「--」と表示し、ファンを回し続ける。ありうる値（115℃）が3回続けば、高温のままでも回復と判断する。",
     "condition": "温度を115℃→4秒目にショート（255℃）→12秒目115℃に変える。",
     "files": {"fixture.txt": fixture(temp=115, extra="TEMP@4=SHORT\nTEMP@12=115\n")},
     "expect": [("sec", 1, "メーター：高温の警告を表示"), ("sec", 4, "温度がありえない値"),
                ("sec", 6, "温度のありえない値が3回続いた"), ("sec", 6, "温度を「--」と表示"),
                ("sec", 14, "温度がありうる値に戻った"), ("not", None, "高温の警告を消した"), ("not", None, "ファンを止めた")]},
    {"id": "fault_from_start", "group": "other", "title": "最初から温度センサーの異常",
     "situation": "エンジンをかけたときから温度センサーの配線が切れていて、しばらくして直る。直ったときには、エンジンが熱くなっている。",
     "anomaly": "温度センサーが最初からありえない値（0℃）を出し、直ったときには100℃になっている。",
     "response": "3回続いたらセンサーの異常と判断して、安全のためファンを回す。直ったときは、新しい温度で判断できるまでファンを止めない。100℃なので、そのまま回し続ける。",
     "condition": "温度を、最初から配線が切れた状態（0℃）→8秒目100℃に変える。",
     "files": {"fixture.txt": fixture(temp="OPEN", extra="TEMP@8=100\n")},
     "expect": [("sec", 3, "温度のありえない値が3回続いた"), ("sec", 3, "安全のためファンを回し始めた"),
                ("sec", 10, "温度がありうる値に戻った"), ("not", None, "ファンを止めた")]},
    {"id": "faultlink", "group": "other", "title": "警告灯の連絡の途切れ",
     "situation": "エンジンの見張り役の不具合で、警告灯の連絡だけが送られてこなくなる。",
     "anomaly": "警告灯の連絡（0.2秒ごと）が届かない。",
     "response": "3回続けて届かなければ途切れと判断して記録し、ファン係は安全のためファンを回す。メーターは、届いた温度が本当の値か分からないので「--」と表示する。届くようになれば回復と判断する。",
     "condition": "温度は70℃のまま。5.5〜10.5秒の間、警告灯の連絡を止める。",
     "files": {"fixture.txt": fixture(), "can_fault.txt": can_fault("DROP", "FAULT_STATUS")},
     "expect": [("sec", 7, "メーター：警告灯の連絡を受け取れなくなった"), ("sec", 7, "安全のためファンを回し始めた"),
                ("sec", 12, "メーター：警告灯の連絡を受け取れるようになった"), ("sec", 12, "ファンを止めた")]},
    {"id": "corrupt", "group": "other", "title": "ありえない値の連絡",
     "situation": "送る側（エンジンの見張り役）の故障で、ありえない値が送られてくる。",
     "anomaly": "「速度・回転数・温度」の連絡に、ありえない値（範囲外）が入っている。",
     "response": "途切れと同じく異常として扱い、記録する。メーターはありえない値を表示せず、最後に届いた正しい値を表示する。",
     "condition": "温度は70℃のまま。5.5〜10.5秒の間、範囲外の値（すべて255など）を送る。",
     "files": {"fixture.txt": fixture(), "can_fault.txt": can_fault("CORRUPT", "ENGINE_STATUS")},
     "expect": [("sec", 9, "メーター：速度・回転数・温度の連絡を受け取れなくなった"), ("sec", 14, "メーター：速度・回転数・温度の連絡を受け取れるようになった")]},
    {"id": "fanlink", "group": "other", "title": "ファン係の連絡の途切れ",
     "situation": "ファン係のコンピュータの不具合で、ファンの状態がメーターに届かなくなる。",
     "anomaly": "ファンの状態の連絡が届かない。",
     "response": "メーターが、ファン係の異常として途切れを記録し、表示する。届くようになれば回復と判断する。",
     "condition": "温度は97℃のまま（ファンは回っている）。5.5〜10.5秒の間、ファンの状態の連絡を止める。",
     "files": {"fixture.txt": fixture(temp=97), "can_fault.txt": can_fault("DROP", "FAN_STATUS")},
     "expect": [("sec", 9, "メーター：ファンの状態の連絡を受け取れなくなった"), ("sec", 14, "メーター：ファンの状態の連絡を受け取れるようになった"),
                ("end", None, "メーター：ファンの状態の連絡を受け取れなかったこと 1回")]},
    {"id": "config", "group": "other", "title": "設定ファイルの異常",
     "situation": "整備での書き込みに失敗して、設定データが無くなっている。",
     "anomaly": "起動時に設定ファイルを読めない。",
     "response": "止まらずに、決めてある値で動き続ける。起動時の自己点検で、読めなかったことを知らせる。",
     "condition": "設定ファイル（config.txt）を置かずに起動する。",
     "files": {"fixture.txt": fixture(), "dtc_data.txt": DTC_EMPTY},
     "no_config": True,
     "expect": [("start", None, "設定ファイルが無い"), ("start", None, "自己点検：不合格")]},
    {"id": "config_broken", "group": "other", "title": "設定ファイルの中身の異常",
     "situation": "整備での書き込みミスで、設定データの中身がおかしくなっている。",
     "anomaly": "設定ファイルに、読めない行や、ありえない値（危険と判断する温度の境目を999℃にするなど）が入っている。",
     "response": "読めない行とありえない値だけを無視し、決めてある値で動き続ける。",
     "condition": "中身が壊れた設定ファイル（config.txt）を置いて起動する（シナリオテストと同じ中身）。",
     "files": {"fixture.txt": fixture(), "config.txt": BROKEN_CONFIG},
     "expect": [("start", None, "設定ファイルを読み込んだ"), ("start", None, "温度110℃"), ("start", None, "自己点検：合格")]},
    {"id": "first_boot", "group": "other", "title": "初めての起動（記録ファイルが無い）",
     "situation": "新しい車で、初めてエンジンをかける。",
     "anomaly": "前回の故障の記録が無い。",
     "response": "記録ゼロから始める。初めての起動か、記録が消えたのかは区別できないため、起動時の自己点検で記録が無いことを知らせる（止まらずに動く）。終了するときに記録を保存する。",
     "condition": "記録ファイル（dtc_data.txt）を置かずに起動する。",
     "files": {"fixture.txt": fixture()},
     "no_dtc": True,
     "expect": [("start", None, "前回の記録が無い"), ("start", None, "自己点検：不合格"), ("end", None, "故障の記録を保存した")]},
    {"id": "save_fail", "group": "other", "title": "記録の保存の失敗",
     "situation": "記録用の部品が壊れて、書き込めなくなっている。",
     "anomaly": "終了するときに、故障の記録を保存できない。",
     "response": "保存に失敗したことを検出して知らせる。止まったりせずに終了する。",
     "condition": "書き込み禁止にした記録ファイル（dtc_data.txt）を置き、温度115℃で動かす。",
     "files": {"fixture.txt": fixture(temp=115)},
     "readonly": ["dtc_data.txt"],
     "expect": [("start", None, "前回の記録を読み込んだ"), ("end", None, "保存に失敗")]},
    {"id": "record", "group": "other", "title": "記録ファイルの異常",
     "situation": "記録用のデータが壊れている。",
     "anomaly": "起動時に、前回の故障の記録を正しく読めない。",
     "response": "壊れた記録は使わず、記録ゼロから始める。起動時の自己点検で知らせる。",
     "condition": "中身が壊れた記録ファイル（dtc_data.txt）を置いて起動する。",
     "files": {"fixture.txt": fixture(), "dtc_data.txt": "broken\n"},
     "expect": [("start", None, "前回の記録が壊れている"), ("start", None, "自己点検：不合格")]},
    {"id": "restart", "group": "other", "title": "電源の入れ直し",
     "situation": "温度センサーの異常が出たままエンジンを切り、翌日またかける。",
     "anomaly": "異常ではない（記録が残るかの確認）。",
     "response": "終了するときに故障の記録を保存し、次に起動したときに読み込んで続きから数える。",
     "condition": "温度センサーの配線が切れたまま（0℃）1回動かして終了した後、同じ場所でもう1回起動する（この画面は2回目）。",
     "pre": {"fixture.txt": fixture(temp="OPEN")},
     "files": {"fixture.txt": fixture(temp="OPEN")},
     "keep_dtc": True,
     "expect": [("start", None, "前回の記録を読み込んだ"), ("end", None, "異常になった回数 2"), ("end", None, "故障の記録を保存した")]},
    {"id": "ignition", "group": "other", "title": "エンジンを切る・かけ直す",
     "situation": "走行中にエンジンを切り、少ししてからかけ直す。最後はエンジンを切って止める。",
     "anomaly": "異常ではない（電源の状態が変わったときの確認）。",
     "response": "エンジンを切っている間は何もしない。かけ直すと、3つのコンピュータとも判断をやり直す（記録は残す）。エンジンが切れているときだけ点検用のコマンドを受け付け、知らない命令は受け付けない。",
     "condition": "温度は97℃のまま。5秒目にエンジンを切り、8秒目にかけ直し、18秒目に切る。最後に知らない命令「hello」を入力する。",
     "files": {"fixture.txt": fixture(temp=97, extra="IGNITION@5=OFF\nIGNITION@8=ON\nIGNITION@18=OFF\n")},
     "stdin": "hello\n",
     "expect": [("sec", 5, "エンジンを切った"), ("sec", 8, "エンジンをかけた"), ("sec", 8, "ファンを回し始めた"),
                ("sec", 18, "エンジンを切った"), ("end", None, "知らない命令")]},
    {"id": "inspection", "group": "main", "title": "整備工場での点検", "tagline": "記録を残し、整備工場で消す",
     "situation": "修理の後、整備士が点検用の機械をつなぎ、故障の記録を消す。",
     "anomaly": "異常ではない（記録を消す操作の確認）。",
     "response": "エンジンが切れているときだけ点検用のコマンドを受け付け、「clear」で故障の記録を消す。",
     "condition": "温度センサーの配線が切れた状態（0℃）で1回動かして記録を残した後、エンジンを切ったまま起動し、最後に「clear」を入力する（この画面は2回目）。",
     "pre": {"fixture.txt": fixture(temp="OPEN")},
     "files": {"fixture.txt": fixture(ign="OFF")},
     "keep_dtc": True,
     "stdin": "clear\n",
     "dtc_after": [(4, "0", "温度センサーの記録は消えている")],
     "expect": [("start", None, "前回の記録を読み込んだ"), ("end", None, "受け付ける"), ("end", None, "故障の記録を消した")]},
    {"id": "inspection_one", "group": "other", "title": "点検でセンサー1つ分だけ消す",
     "situation": "修理したのは温度センサーだけなので、整備士がその記録だけを消す。",
     "anomaly": "異常ではない（センサーごとに記録を消す操作の確認）。",
     "response": "エンジンが切れているときだけ点検用のコマンドを受け付け、「clear temp」で温度の記録（温度センサーの故障とオーバーヒートの2つ）だけを消す。ほかのセンサーの記録は残す。",
     "condition": "速度110km/h・温度115℃（オーバーヒート）→4秒目に温度センサーの配線が切れる（0℃）で1回動かして3つの記録を残した後、エンジンを切ったまま起動し、最後に「clear temp」を入力する（この画面は2回目）。",
     "pre": {"fixture.txt": fixture(temp=115, speed=110, extra="TEMP@4=OPEN\n")},
     "files": {"fixture.txt": fixture(ign="OFF")},
     "keep_dtc": True,
     "stdin": "clear temp\n",
     # 終了時の一覧は消す前に表示されるので、消した結果は終了後の記録ファイルで確かめる
     # （並びはpersist.cのdtc_to_textと同じ：1行目が速度、5行目が温度センサーの故障、7行目がオーバーヒートの回数）
     "dtc_after": [(0, "1", "速度の記録は残っている"), (4, "0", "温度センサーの記録は消えている"),
                   (6, "0", "オーバーヒートの記録も消えている")],
     "expect": [("start", None, "前回の記録を読み込んだ"), ("end", None, "受け付ける"),
                ("end", None, "温度の記録だけを消した")]},
]

SENSOR_NAMES = {"Speed": "速度", "RPM": "回転数", "Temp": "温度"}
# 記録の種類の表示名（diag.cのdtc_nameと同じ並び、Phase28）
DTC_NAMES = {"Speed": "速度", "RPM": "回転数", "TempSensor": "温度センサー", "Overheat": "オーバーヒート"}
MESSAGE_NAMES = {"EngineStatus": "速度・回転数・温度", "FaultStatus": "警告灯", "FanStatus": "ファンの状態"}
START_TEXTS = {
    "[CONFIG] No config file (using defaults)": "設定ファイルが無い → 決めてある値で動く",
    "[CONFIG] Loaded config file": "設定ファイルを読み込んだ",
    "[PERSIST] No saved data (first run)": "前回の記録が無い → 記録ゼロから始める",
    "[PERSIST] Saved data is corrupted, ignoring": "前回の記録が壊れている → 使わずに記録ゼロから始める",
    "[PERSIST] Loaded saved DTC data": "前回の記録を読み込んだ",
    "[POST] Self-check passed": "起動時の自己点検：合格",
    "[POST] Self-check did not pass, continuing with defaults": "起動時の自己点検：不合格 → 止まらずに動き続ける",
}
END_TEXTS = {
    "[CMD] Not accepted (ignition ON)": "点検用のコマンド：エンジンがかかっているため受け付けない",
    "Enter command (clear/Enter to skip):": "点検用のコマンドを受け付ける（エンジンが切れている）",
    "[DTC] Cleared": "点検用のコマンド「clear」→ 故障の記録を消した",
    "[CMD] Cleared records of all ECUs": "メーター・ファン係が残した連絡の途切れの記録も消した",
    "[CMD] Unknown command": "点検用のコマンド：知らない命令なので受け付けない",
    "[PERSIST] Saved DTC data": "故障の記録を保存した（次の起動で引き継ぐ）",
    "[PERSIST] Failed to save DTC data": "故障の記録の保存に失敗したことを検出した",
}


def run_once(workdir, files, stdin, readonly=()):
    for name, text in files.items():
        with open(os.path.join(workdir, name), "w") as f:
            f.write(text)
    for name in readonly:
        os.chmod(os.path.join(workdir, name), 0o444)   # 書き込み禁止にして、保存の失敗を起こす
    result = subprocess.run([SENSOR_SIM_VIRTUAL], cwd=workdir, input=stdin, capture_output=True,
                            text=True, timeout=60, check=True)
    return result.stdout.splitlines()


def run_with_dtc(scenario):
    """作業用のフォルダで場面を動かし、標準出力の行と、終了後の記録ファイルの中身（無ければNone）を返す。
    preがあれば、先に1回動かして記録を残す。"""
    with tempfile.TemporaryDirectory() as workdir:
        base = {} if scenario.get("no_config") else {"config.txt": CONFIG_OK}
        if "pre" in scenario:
            run_once(workdir, {**base, "dtc_data.txt": DTC_EMPTY, **scenario["pre"]}, "")
        files = dict(base)
        if not (scenario.get("keep_dtc") or scenario.get("no_dtc")):
            files["dtc_data.txt"] = DTC_EMPTY
        files.update(scenario["files"])
        if "can_fault.txt" not in files and os.path.exists(os.path.join(workdir, "can_fault.txt")):
            os.remove(os.path.join(workdir, "can_fault.txt"))
        lines = run_once(workdir, files, scenario.get("stdin", ""), scenario.get("readonly", ()))
        dtc_path = os.path.join(workdir, "dtc_data.txt")
        dtc_text = open(dtc_path).read() if os.path.exists(dtc_path) else None
        return lines, dtc_text


def run(scenario):
    """作業用のフォルダで場面を動かし、標準出力の行を返す。"""
    return run_with_dtc(scenario)[0]


def fan_event_text(reason):
    if reason == "temp":
        return "温度が95℃以上 → ファンを回し始めた"
    if reason == "temp sensor fault":
        return "温度センサーの異常 → 安全のためファンを回し始めた"
    if reason == "link lost":
        return "連絡を受け取れない → 安全のためファンを回し始めた"
    return "ファンを回し始めた"


def parse(lines):
    """出力を1秒ごとの画面の情報（frames）、起動時の出来事（start）、終了時の記録（records）と出来事（end）にまとめる。

    メーターの値・状態は、前の秒から引き継ぎ、その秒に届いた行で上書きする（実際のメーターと同じく、
    届かない間は最後の値を表示する）。ファンを回している理由は、ファン係の判断（fan_ecu.c）と同じ順番で、
    ファン係が見ている連絡の途切れ → 警告灯の温度ビット → 温度、の順に表示用に読み替える
    （理由が変わってもファン係はログを出さないため）。"""
    frames, start, records, end = [], [], [], []
    # temp_unknownは、水温センサーの異常の間にメーターが温度を「--」と表示しているか（Phase27）。
    # lampsのoverheatは、警告灯の連絡の高温の通知（Phase27）
    meter = {"speed": None, "rpm": None, "temp": None, "temp_unknown": False,
             "lamps": {"speed": False, "rpm": False, "temp": False, "overheat": False},
             "links": {"EngineStatus": "OK", "FaultStatus": "OK", "FanStatus": "OK"}, "fan": None}
    fan_on = False
    fan_lost = set()
    prev_levels = {}
    frame = None
    phase = "start"
    for line in lines:
        m = re.match(r"\[Sample (\d+)\]", line)
        if m:
            phase = "samples"
            frame = {"sec": int(m.group(1)), "ign": None, "sensor": None, "events": []}
            frames.append(frame)
            continue
        # 終了時の出力は統計から始まる。エンジンが一度もONにならなかった実行では、統計の代わりに"No data"が出る
        if line.startswith("--- Stats") or line == "No data":
            phase = "end"
            continue
        if phase == "start":
            # 使う設定値の行。危険と判断する境目（StatusCrit）を出す。設定ファイルの中身が壊れていても、
            # 決めてある値のままなことが、この行で分かる
            m = re.search(r"StatusCrit\(speed=(\d+),rpm=(\d+),temp=(\d+)\)", line)
            if line.startswith("[CONFIG] AlertMax(") and m:
                start.append(f"危険と判断する境目：速度{m.group(1)}km/h・回転数{m.group(2)}・温度{m.group(3)}℃")
            elif line in START_TEXTS:
                start.append(START_TEXTS[line])
            continue
        if phase == "end":
            m = re.match(r"\[DTC\] Cleared: (\w+)$", line)
            if m:
                end.append(f"点検用のコマンド「clear {m.group(1).lower()}」→ {SENSOR_NAMES[m.group(1)]}の記録だけを消した")
            elif re.match(r"\[(DTC|CAN-DTC|FAN-DTC)\] ", line) and not line.startswith("[DTC] Cleared"):
                records.append(line)
            elif line in END_TEXTS:
                end.append(END_TEXTS[line])
            continue
        if line in ("[IGN] ON", "[IGN] OFF"):
            frame["ign"] = line[6:]
            if frame["ign"] == "OFF":
                # 電源OFFでファン係は止まり、ONに戻ると判断をやり直す（ログは出ない）
                fan_on = False
        elif line == "[IGN] OFF -> ON":
            frame["events"].append("エンジンをかけた")
            # かけ直すと、エンジン監視ECUは記録の前回の状態を戻し、異常が続いていても数え直す（main.cと同じ）
            prev_levels.clear()
            # ファン係も連絡の途切れの状態を戻す（fan_ecu.cのfan_ecu_is_poweredが受信監視状態の途絶の状態を戻すのと同じ。
            # 戻したときはログが出ないので、ここで消す）
            fan_lost.clear()
        elif line == "[IGN] ON -> OFF":
            frame["events"].append("エンジンを切った")
        elif line.startswith("Speed:"):
            v = re.findall(r"\d+", line)
            frame["sensor"] = {"speed": int(v[0]), "rpm": int(v[1]), "temp": int(v[2])}
        elif line.startswith("[STATUS] "):
            # 危険（CRITICAL）と、ありえない値（OUTRANGE、Phase27）。どちらかに入った瞬間を記録として数え、
            # 両者の間を移ったときは数え直さない（diag.cと同じ）
            for name, level in re.findall(r"(Speed|RPM|Temp): (\w+)", line):
                prev = prev_levels.get(name)
                was_bad = prev in ("CRITICAL", "OUTRANGE")
                if level == "CRITICAL" and prev != "CRITICAL":
                    # 警告（[ALERT]）は差し替え後の値で判断するので、危険に入っても出ないことがある（センサーの異常と
                    # 判断している間、Phase27）。文はその秒の[ALERT]の行を見てから決めるため、ここでは場所だけ取っておく
                    frame.setdefault("_critical", []).append((len(frame["events"]), name, was_bad))
                    frame["events"].append("")
                elif level == "OUTRANGE" and prev != "OUTRANGE":
                    frame["events"].append(f"{SENSOR_NAMES[name]}がありえない値（範囲外）→ "
                                           + ("" if was_bad else "異常として記録し、") + "直前の正しい値を使う")
                prev_levels[name] = level
        elif line.startswith("[ALERT] "):
            frame.setdefault("_alerted", set()).add(line.split()[1])   # "Speed"・"RPM"・"Temp"
        elif line.startswith("[FAULT] Degraded: "):
            name = line.split(": ")[1]
            # 温度は、ありえない値が続いたときだけ異常と判断する（危険な高さはオーバーヒートとして置き換えない、Phase27）
            what = "ありえない値" if name == "Temp" else "危険な値"
            frame["events"].append(f"{SENSOR_NAMES[name]}の{what}が3回続いた → センサーを異常と判断（安全な値に置き換え）")
        elif line.startswith("[FAULT] Recovered: "):
            name = line.split(": ")[1]
            what = "ありうる値" if name == "Temp" else "正常な値"
            frame["events"].append(f"{SENSOR_NAMES[name]}が{what}に戻った → 回復と判断")
        elif line.startswith("[METER] EngineStatus["):
            v = dict(re.findall(r"(\w+)=(\d+|--)", line))
            unknown = (v["temp"] == "--")
            if unknown != meter["temp_unknown"]:
                frame["events"].append("メーター：温度を「--」と表示（本当の温度が分からない）" if unknown
                                       else "メーター：温度の表示を再開")
            meter["speed"], meter["rpm"] = int(v["speed"]), int(v["rpm"])
            meter["temp"] = None if unknown else int(v["temp"])
            meter["temp_unknown"] = unknown
            meter["links"]["EngineStatus"] = "LOST" if "[LOST]" in line else "OK"
        elif line.startswith("[METER] FaultStatus["):
            v = dict(re.findall(r"(\w+)=(\d+)", line))
            lamps = {k: v[k] == "1" for k in ("speed", "rpm", "temp", "overheat")}
            if lamps["overheat"] != meter["lamps"]["overheat"]:
                frame["events"].append("メーター：高温の警告を表示" if lamps["overheat"] else "メーター：高温の警告を消した")
            meter["lamps"] = lamps
            meter["links"]["FaultStatus"] = "LOST" if "[LOST]" in line else "OK"
        elif line.startswith("[METER] FanStatus["):
            meter["fan"] = line.endswith("fan=1")
            meter["links"]["FanStatus"] = "LOST" if "[LOST]" in line else "OK"
        elif line.startswith("[CAN] "):
            msg = line.split()[1]
            # 途切れ（Timeout）と、ありえない値（Invalid Data）は、プログラムの中でも出力でも区別しないため、
            # どちらにも当てはまる「受け取れない」と書く
            state = "受け取れなくなった" if line.endswith("link lost") else "受け取れるようになった"
            frame["events"].append(f"メーター：{MESSAGE_NAMES[msg]}の連絡を{state}")
        elif line.startswith("[FAN] Fan ON"):
            fan_on = True
            frame["events"].append(fan_event_text(line[len("[FAN] Fan ON ("):-1]))
        elif line == "[FAN] Fan OFF":
            fan_on = False
            frame["events"].append("ファンを止めた")
        elif line.startswith("[FAN] "):
            msg = line.split()[1]
            if line.endswith("link lost"):
                fan_lost.add(msg)
                frame["events"].append(f"ファン係：{MESSAGE_NAMES[msg]}の連絡を受け取れなくなった")
            else:
                fan_lost.discard(msg)
                frame["events"].append(f"ファン係：{MESSAGE_NAMES[msg]}の連絡を受け取れるようになった")
        else:
            continue
        # 行を読むたびに、その時点の状態をフレームへ写す（その秒の最後の状態が残る）
        frame["meter"] = json.loads(json.dumps(meter))
        if fan_on:
            reason = "link lost" if fan_lost else ("temp sensor fault" if meter["lamps"]["temp"] else "temp")
        else:
            reason = None
        frame["fan"] = {"on": fan_on, "reason": reason}
        # ファン係が見ている2つの連絡の状態（画面の図で、見張り役→ファン係の線に使う）
        frame["fanlinks"] = {msg: "LOST" if msg in fan_lost else "OK" for msg in ("EngineStatus", "FaultStatus")}
    for f in frames:
        # 危険に入った瞬間の文を、その秒に警告が出たかで決める（記録は、ありえない値から移ったときは数え直さない）
        alerted = f.pop("_alerted", set())
        for i, name, was_bad in f.pop("_critical", []):
            text = f"{SENSOR_NAMES[name]}が危険な値 → "
            if name in alerted:
                text += "警告" if was_bad else "警告し、異常として記録"
            else:
                # 警告が出ない理由は2つありうる：センサーを異常と判断中（置き換えた値で判断する）か、
                # 警告の境目（ALERT_*）が危険の境目（STATUS_*_CRIT）より高く設定されているか
                degraded = f.get("meter") is not None and f["meter"]["lamps"][name.lower()]
                reason = "センサーの異常と判断中で、置き換えた値で判断するため" if degraded else "警告の境目を超えていないため"
                text += ("" if was_bad else "異常として記録し、") + f"警告は出さない（{reason}）"
            f["events"][i] = text
        if f["ign"] == "OFF":
            f["meter"] = None
            f["fan"] = {"on": False, "reason": None}
            f["fanlinks"] = None
        f.setdefault("meter", None)
        f.setdefault("fan", {"on": False, "reason": None})
        f.setdefault("fanlinks", None)
    return frames, start, records, end


def record_text(line):
    """終了時の記録の行を、画面に出す日本語にする。表示しない行はNoneを返す。"""
    # 記録は種類ごと（Phase28）。温度は、温度センサーの故障とオーバーヒートを別の記録にしている
    m = re.match(r"\[DTC\] (\w+)\s+(\w+)\s+occurrences: (\d+)", line)
    if m:
        state = {"NONE": "記録なし", "ACTIVE": "今も異常", "HISTORY": "過去に異常（今は正常）"}[m.group(2)]
        return f"{DTC_NAMES[m.group(1)]}の記録：{state}・異常になった回数 {m.group(3)}"
    m = re.match(r"\[DTC\] Freeze Frame \(trigger: (\w+)\) Speed:\s*(\d+) km/h RPM:\s*(\d+) Temp:\s*(\d+) C", line)
    if m:
        return (f"最初に異常になった瞬間の状態（{DTC_NAMES[m.group(1)]}）：速度 {m.group(2)}km/h・"
                f"回転数 {m.group(3)}・温度 {m.group(4)}℃")
    m = re.match(r"\[(CAN|FAN)-DTC\] (\w+)\s+(\w+)\s+link lost occurrences: (\d+)", line)
    if m and m.group(3) != "NONE":
        who = "メーター" if m.group(1) == "CAN" else "ファン係"
        return f"{who}：{MESSAGE_NAMES[m.group(2)]}の連絡を受け取れなかったこと {m.group(4)}回"
    return None


def check(scenario, frames, start, records, end, dtc_text):
    """期待する出来事が、実際の出力から作った情報に含まれているかを照らし合わせる。食い違えば止める。
    dtc_afterがあれば、終了後の記録ファイルの行も照らし合わせる。"""
    results = []
    dtc_lines = (dtc_text or "").splitlines()
    for row, value, label in scenario.get("dtc_after", []):
        actual = dtc_lines[row] if row < len(dtc_lines) else None
        if actual != value:
            raise SystemExit(f"[{scenario['id']}] 期待と食い違った：終了後の記録ファイル {row + 1}行目が{value}（実際：{actual}）")
        results.append(f"終了後の記録ファイル：{label}")
    for where, sec, word in scenario["expect"]:
        if where == "not":
            # 出てはいけない出来事（例：復帰の瞬間にファンを止めない）。secがNoneなら、どの秒にも出ないこと
            texts = [t for f in frames if sec is None or f["sec"] == sec for t in f["events"]]
            label = ("どの秒にも" if sec is None else f"{sec}秒目に") + f"「{word}」が出ない"
            if any(word in t for t in texts):
                raise SystemExit(f"[{scenario['id']}] 期待と食い違った：{label}")
            results.append(label)
            continue
        if where == "sec":
            texts = next((f["events"] for f in frames if f["sec"] == sec), [])
            label = f"{sec}秒目：{word}"
        elif where == "start":
            texts, label = start, f"起動時：{word}"
        else:
            texts, label = records + end, f"終了時：{word}"
        if not any(word in t for t in texts):
            raise SystemExit(f"[{scenario['id']}] 期待と食い違った：{label}（実際：{texts}）")
        results.append(label)
    # 画面はどの場面にも「確かめ済み」と出すので、期待を持たない場面は置かない
    if not results:
        raise SystemExit(f"[{scenario['id']}] 期待する出来事がない")
    return results


def main():
    data = []
    for s in SCENARIOS:
        lines, dtc_text = run_with_dtc(s)
        frames, start, raw_records, end = parse(lines)
        records = [t for t in (record_text(r) for r in raw_records) if t]
        checks = check(s, frames, start, records, end, dtc_text)
        # 終了時の一覧は点検用のコマンドより前に表示されるので、コマンドの後の結果は記録ファイルの中身で見せる
        end += [c for c in checks if c.startswith("終了後の記録ファイル：")]
        data.append({key: s[key] for key in ("id", "group", "title", "situation", "anomaly", "response", "condition")}
                    | {"tagline": s.get("tagline", ""), "frames": frames, "start": start, "records": records, "end": end, "checks": checks})
    # 別ファイル（data.js）にすると、開き方によっては読み込めないため、index.htmlの中に書き込む。
    # 文字列の中に"</"があると<script>が途中で閉じるので、"<\/"にしておく
    text = json.dumps(data, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
    with open(OUTPUT) as f:
        html = f.read()
    start = html.index(DATA_START)
    start = html.index("\n", start) + 1
    end = html.index(DATA_END)
    html = html[:start] + "<script>window.REPLAY_DATA = " + text + ";</script>\n" + html[end:]
    with open(OUTPUT, "w") as f:
        f.write(html)
    print(f"{OUTPUT} に {len(data)} 場面を書いた（期待との照らし合わせ {sum(len(d['checks']) for d in data)} 項目、食い違いなし）")


if __name__ == "__main__":
    main()
