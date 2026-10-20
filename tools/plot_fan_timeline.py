"""sensor_simの出力から、水温と冷却ファンの動きを時間軸で描いたSVGを作る（README・説明用）。

README用の図（docs/images/fan_run_example.svg）の作り直し方（リポジトリ直下で）:
    make sensor_sim_virtual
    R=$(pwd); W=$(mktemp -d); cp tools/fan_run_example_fixture.txt $W/fixture.txt
    (cd $W && $R/sensor_sim_virtual < /dev/null) | python3 tools/plot_fan_timeline.py > docs/images/fan_run_example.svg
作業用のフォルダで動かすのは、fixture.txtを置くためと、リポジトリ直下のdtc_data.txtを上書きしないため。
ほかの出力（ランダム・実時間版など）も、同じように標準入力へ渡せば描ける。

横軸は1秒単位（出力に時刻は無く、分かるのは何秒目のサンプルで起きたかだけ）。
図の中の言葉は、出力に出た出来事（ファンのON/OFF、水温センサーのありえない値・異常・回復）の秒に自動で置くので、
プログラムの動きが変われば、作り直した図も一緒に変わる。ありえない値（範囲外、Phase27）の秒は温度の線を描かず、
グラフの上の帯で示す（断線の0℃などをそのまま描くと、目盛りが崩れるため）。
config.txtのLOG_LEVELを上げると[IGN]・[FAN]の行が出なくなるため、既定（0）の出力を前提にする。
"""
import re
import sys

FONT = "'Hiragino Sans','Yu Gothic','Meiryo','Noto Sans JP',sans-serif"
TEXT = "#2C2C2A"
MUTED = "#5F5E5A"
GRID = "#B4B2A9"
TEMP_LINE = "#D85A30"
FAULT_FILL = "#FAECE7"
FAN_FILL = "#E1F5EE"
FAN_STROKE = "#0F6E56"
FAN_TEXT = "#085041"

FAN_ON_TEMP = 95   # fan_ctrl.hのFAN_ON_TEMPと同じ値（図の点線の位置だけに使う）

# 図の寸法。横はプロット部分を秒数で等分する
WIDTH = 720
LEFT = 130
RIGHT = 690
LABEL_TOP = 64       # 出来事の言葉を置く行の上端
LABEL_ROW = 22       # 言葉が重なるときに下へずらす幅
CHART_TOP = 118
CHART_BOTTOM = 258
FAN_TOP = 278
FAN_HEIGHT = 30
AXIS_Y = 330
HEIGHT = 400


def parse(lines):
    """出力を読み、サンプルごとの{"ign": ON/OFF, "temp": 水温, "events": [...]}のリストを返す。"""
    samples = []
    current = None
    for line in lines:
        line = line.rstrip("\n")
        if line.startswith("[Sample "):
            current = {"ign": None, "temp": None, "events": []}
            samples.append(current)
        elif line.startswith("--- Stats"):
            current = None
        elif current is None:
            continue
        elif line in ("[IGN] ON", "[IGN] OFF"):
            current["ign"] = line[len("[IGN] "):]
        elif line.startswith("Speed:"):
            m = re.search(r"Temp:\s*(\d+) C", line)
            if m:
                current["temp"] = int(m.group(1))
        elif line.startswith("[STATUS] "):
            # 温度センサーの値が、ありえない値（範囲外、Phase27）か。範囲外の値は線に描かない（目盛りが崩れるため）
            if re.search(r"Temp: OUTRANGE", line):
                current["invalid"] = True
                if not (len(samples) >= 2 and samples[-2].get("invalid")):
                    current["events"].append("outrange")
        elif line.startswith("[FAN] Fan ON"):
            current["events"].append("fan_on")
        elif line == "[FAN] Fan OFF":
            current["events"].append("fan_off")
        elif line == "[FAULT] Degraded: Temp":
            current["events"].append("degraded")
        elif line == "[FAULT] Recovered: Temp":
            current["events"].append("recovered")
    return samples


def fan_states(samples):
    """各サンプルの終わりのファン出力を、[FAN]の行と「電源OFFでOFFに戻る」決まりから組み立てる。
    電源OFF→ONでファン制御ECUが状態を戻すときはログが出ないので（fan_ecu.c）、ここでも同じ扱いにする。"""
    states = []
    on = False
    for s in samples:
        if s["ign"] == "OFF":
            on = False
        for e in s["events"]:
            if e == "fan_on":
                on = True
            elif e == "fan_off":
                on = False
        states.append(on)
    return states


def fault_flags(samples):
    """各サンプルで、水温センサーが異常と判断されている間ならTrueを返す（確定した秒から、回復した秒の前まで）。"""
    flags = []
    degraded = False
    for s in samples:
        if "degraded" in s["events"]:
            degraded = True
        if "recovered" in s["events"]:
            degraded = False
        flags.append(degraded)
    return flags


def runs(values):
    """[(開始の添字, 終わりの添字(含まない), 値), ...]に、同じ値が続く区間をまとめる。"""
    result = []
    start = 0
    for i in range(1, len(values) + 1):
        if i == len(values) or values[i] != values[start]:
            result.append((start, i, values[start]))
            start = i
    return result


def text(x, y, s, size=13, color=TEXT, anchor="start", weight="normal"):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" fill="{color}" text-anchor="{anchor}" '
            f'font-weight="{weight}" dominant-baseline="central">{s}</text>')


def text_width(s, size):
    """文字の幅の目安。全角は文字の大きさと同じ、半角はその0.6倍とみなす（余裕を持たせるため大きめ）。"""
    return sum(size if ord(c) > 0x7F else size * 0.6 for c in s)


def render(samples):
    n = len(samples)
    step = (RIGHT - LEFT) / n

    def x_at(i):
        return LEFT + i * step

    temps = [s["temp"] for s in samples if s["temp"] is not None and not s.get("invalid")]
    lo = min(temps + [FAN_ON_TEMP]) - 10
    hi = max(temps + [FAN_ON_TEMP]) + 5

    def y_at(t):
        return CHART_BOTTOM - (t - lo) * (CHART_BOTTOM - CHART_TOP) / (hi - lo)

    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" '
           f'viewBox="0 0 {WIDTH} {HEIGHT}" font-family="{FONT}">',
           f'<rect width="{WIDTH}" height="{HEIGHT}" fill="#FFFFFF"/>',
           text(24, 28, "実際に動かした結果（温度センサーに異常が起きた場合）", size=16, weight="bold")]

    # 水温センサーが異常と判断されていた区間
    for start, end, flag in runs(fault_flags(samples)):
        if flag:
            out.append(f'<rect x="{x_at(start):.1f}" y="{CHART_TOP}" width="{x_at(end) - x_at(start):.1f}" '
                       f'height="{CHART_BOTTOM - CHART_TOP}" fill="{FAULT_FILL}"/>')
            out.append(text((x_at(start) + x_at(end)) / 2, CHART_BOTTOM - 14, "センサー異常と判断中",
                            size=12, color=TEMP_LINE, anchor="middle"))

    # 温度センサーの値が、ありえない値だった区間（グラフの上の帯。線は描かない）
    for start, end, flag in runs([bool(s.get("invalid")) for s in samples]):
        if flag:
            values = sorted({samples[i]["temp"] for i in range(start, end)})
            out.append(f'<rect x="{x_at(start):.1f}" y="{CHART_TOP}" width="{x_at(end) - x_at(start):.1f}" '
                       f'height="20" fill="{GRID}"/>')
            out.append(text((x_at(start) + x_at(end)) / 2, CHART_TOP + 10,
                            "ありえない値（" + "・".join(f"{v}℃" for v in values) + "）",
                            size=12, color="#FFFFFF", anchor="middle"))

    # ファンを回し始める温度の点線
    y80 = y_at(FAN_ON_TEMP)
    out.append(f'<line x1="{LEFT}" y1="{y80:.1f}" x2="{RIGHT}" y2="{y80:.1f}" stroke="{GRID}" '
               f'stroke-width="1" stroke-dasharray="5 4"/>')

    # 水温（センサーが測った値）。電源OFFの秒と、ありえない値の秒は、線を切る
    out.append(text(24, CHART_TOP + 8, "温度", size=13, color=MUTED))
    segment = []
    for i, s in enumerate(samples):
        if s["temp"] is None or s.get("invalid"):
            if segment:
                out.append(f'<polyline points="{" ".join(segment)}" fill="none" stroke="{TEMP_LINE}" '
                           f'stroke-width="3" stroke-linejoin="round"/>')
            segment = []
            continue
        y = y_at(s["temp"])
        segment += [f"{x_at(i):.1f},{y:.1f}", f"{x_at(i + 1):.1f},{y:.1f}"]
    if segment:
        out.append(f'<polyline points="{" ".join(segment)}" fill="none" stroke="{TEMP_LINE}" '
                   f'stroke-width="3" stroke-linejoin="round"/>')
    # 値のラベルは、既に置いたラベルと縦に近すぎるものは飛ばす（ランダムに動かすと値が多く、重なって読めなくなるため）
    placed = []
    for i, s in enumerate(samples):
        t = s["temp"]
        if t is not None and not s.get("invalid") and (i == 0 or samples[i - 1]["temp"] != t):
            y = y_at(t)
            if all(abs(y - p) >= 18 for p in placed):   # 12pxの日本語フォントは、行の高さが約17pxになる
                placed.append(y)
                out.append(text(LEFT - 8, y, f"{t}℃", size=12, color=TEMP_LINE, anchor="end"))

    # 冷却ファン
    out.append(text(24, FAN_TOP + FAN_HEIGHT / 2, "冷却ファン", size=13, color=MUTED))
    out.append(f'<rect x="{LEFT}" y="{FAN_TOP}" width="{RIGHT - LEFT}" height="{FAN_HEIGHT}" rx="4" '
               f'fill="none" stroke="{GRID}" stroke-width="1"/>')
    # 帯の中の言葉は、収まる長さ（3秒以上）の区間にだけ書く。短い区間に書くと隣の言葉と重なるため
    for start, end, on in runs(fan_states(samples)):
        mid = (x_at(start) + x_at(end)) / 2
        if on:
            out.append(f'<rect x="{x_at(start):.1f}" y="{FAN_TOP}" width="{x_at(end) - x_at(start):.1f}" '
                       f'height="{FAN_HEIGHT}" rx="4" fill="{FAN_FILL}" stroke="{FAN_STROKE}" stroke-width="1"/>')
        if end - start >= 3:
            out.append(text(mid, FAN_TOP + FAN_HEIGHT / 2, "回っている" if on else "止まっている",
                            color=FAN_TEXT if on else MUTED, anchor="middle"))

    # 出来事の言葉。出来事が起きた秒に置き、前の言葉と重なるときは下の行へずらす
    labels = {"fan_on": "熱い → ファンON",
              "outrange": "ありえない値になった",
              "degraded": "3回続く → センサー異常と判断",
              "recovered": "センサー回復",
              "fan_off": "冷えた → ファン停止"}
    row_ends = []
    for i, s in enumerate(samples):
        for e in s["events"]:
            label = labels[e]
            x = x_at(i) + step / 2
            width = text_width(label, 12)
            if x + width > RIGHT:
                x = RIGHT - width
            row = 0
            while row < len(row_ends) and row_ends[row] > x - 8:
                row += 1
            if row == len(row_ends):
                row_ends.append(0)
            row_ends[row] = x + width
            y = LABEL_TOP + row * LABEL_ROW
            out.append(f'<line x1="{x_at(i) + step / 2:.1f}" y1="{y + 8:.1f}" x2="{x_at(i) + step / 2:.1f}" '
                       f'y2="{FAN_TOP + FAN_HEIGHT:.1f}" stroke="{GRID}" stroke-width="1" stroke-dasharray="2 3"/>')
            out.append(f'<circle cx="{x_at(i) + step / 2:.1f}" cy="{y:.1f}" r="3" fill="{TEXT}"/>')
            out.append(text(x + 6, y, label, size=12))

    # 横軸（秒）
    out.append(f'<line x1="{LEFT}" y1="{AXIS_Y}" x2="{RIGHT}" y2="{AXIS_Y}" stroke="{GRID}" stroke-width="1"/>')
    for i in range(n):
        number = i + 1
        if number == 1 or number % 5 == 0:
            out.append(text(x_at(i) + step / 2, AXIS_Y + 14, f"{number}", size=12, color=MUTED, anchor="middle"))
    out.append(text(RIGHT, AXIS_Y + 34, "経過時間（秒）", size=12, color=MUTED, anchor="end"))
    # 点線の説明。温度の値のラベルと重ならないよう、左の列ではなく図の下に書く
    out.append(f'<line x1="24" y1="{AXIS_Y + 34}" x2="48" y2="{AXIS_Y + 34}" stroke="{GRID}" '
               f'stroke-width="1" stroke-dasharray="5 4"/>')
    out.append(text(54, AXIS_Y + 34, f"ファンを回し始める温度（{FAN_ON_TEMP}℃）", size=12, color=MUTED))
    out.append(text(24, AXIS_Y + 56, "※ 車のコンピュータをパソコン上で再現したプログラムの出力から作図（温度は試験用に設定した値）",
                    size=11, color=MUTED))
    out.append("</svg>")
    return "\n".join(out) + "\n"


def main():
    samples = parse(sys.stdin)
    if not samples:
        sys.exit("sensor_simの出力（[Sample NN]の行）がありません")
    if all(s["ign"] is None for s in samples):
        sys.exit("[IGN]の行がありません。config.txtのLOG_LEVELを0にして動かしてください")
    if all(s["temp"] is None for s in samples):
        sys.exit("イグニッションONのサンプルがなく、描く水温がありません")
    sys.stdout.write(render(samples))


if __name__ == "__main__":
    main()
