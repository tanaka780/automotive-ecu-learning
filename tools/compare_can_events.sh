#!/bin/bash
# 変更前後で、CAN関連の「何がどの順で起きたか」が同じかを比べる（Day55の比較方法を作り直したもの、Day56）
#
# 使い方:
#   compare_can_events.sh capture <ソースのディレクトリ> <出力先> [実行回数(既定5)]
#       ソースをビルドし、5条件 x 実行回数だけsensor_simを実行して、正規化した出力を保存する。
#       同じ条件の実行どうしが一致するか（比較の土台になるか）もここで確かめる
#   compare_can_events.sh compare <基準の出力先> <比べる出力先>
#       条件ごとに正規化した出力を比べる。1つでも違えば終了コード1
#
# 正規化（Day55の最終形）:
#   - [CAN] / [CAN-DTC] / [METER] の行だけを残す
#   - 最後のサンプル（[Sample 20]以降）の[METER]行は除く（受信が出ない回があるため）
#   - EngineStatus・FaultStatus・[CAN-DTC]を別々に並べる（メッセージをまたぐ受信の前後は揺れるため）
#   - 同じ行が続く部分は1行にまとめる（1サンプル中の受信回数が4回・5回と揺れるため）
#   - サンプル番号は見ない（LOSTの出るサンプルが6・7とずれる回があるため）
#   - sensor_simの終了コードを先頭に書く（異常終了を一致と見誤らないため）
# 位置を見ないので、周期・登録順の誤りはこの比較では見つからない。そこはコードの差分で見る

set -u

CONDITIONS="drop_engine drop_fault corrupt normal ign_off"

write_inputs() {
    local cond="$1" dir="$2"
    local ign="ON"
    if [ "$cond" = "ign_off" ]; then
        ign="OFF"
    fi
    printf 'MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=95\nIGNITION=%s\n' "$ign" > "$dir/fixture.txt"

    case "$cond" in
        drop_engine) printf 'MODE=DROP\nTARGET=ENGINE_STATUS\nSTART_MS=5500\nEND_MS=10500\n' > "$dir/can_fault.txt" ;;
        drop_fault)  printf 'MODE=DROP\nTARGET=FAULT_STATUS\nSTART_MS=5500\nEND_MS=10500\n' > "$dir/can_fault.txt" ;;
        corrupt)     printf 'MODE=CORRUPT\nTARGET=ENGINE_STATUS\nSTART_MS=5500\nEND_MS=10500\n' > "$dir/can_fault.txt" ;;
        *) ;;
    esac
}

normalize() {
    local raw="$1" exit_code="$2"
    echo "exit=$exit_code"
    local filtered
    filtered=$(awk '/^\[Sample 20\]/ { last = 1 }
         /^\[(CAN|CAN-DTC|METER)\] / {
             if (last && $0 ~ /^\[METER\] /) next
             print
         }' "$raw")
    # メッセージごとに分けて並べる。EngineStatusとFaultStatusの受信の前後は
    # タスクの遅れで入れ替わる（Day56で確認）ため、メッセージをまたぐ順番は比べない
    for msg in EngineStatus FaultStatus; do
        echo "## $msg"
        printf '%s\n' "$filtered" | grep -E "^\[(CAN|METER)\] $msg" | uniq
    done
    echo "## CAN-DTC"
    printf '%s\n' "$filtered" | grep -E '^\[CAN-DTC\] '
}

capture() {
    local src="$1" out="$2" runs="${3:-5}"
    mkdir -p "$out"
    if ! make -s -C "$src" sensor_sim > "$out/build.log" 2>&1; then
        echo "ビルド失敗: $src（$out/build.log）"
        return 1
    fi

    local all_stable=0
    for cond in $CONDITIONS; do
        for i in $(seq 1 "$runs"); do
            local work
            work=$(mktemp -d)
            write_inputs "$cond" "$work"
            (cd "$work" && "$src/sensor_sim" < /dev/null > "$work/raw.txt" 2>&1)
            local code=$?
            cp "$work/raw.txt" "$out/${cond}_${i}.raw.txt"
            normalize "$work/raw.txt" "$code" > "$out/${cond}_${i}.txt"
            rm -rf "$work"
        done

        cp "$out/${cond}_1.txt" "$out/${cond}.txt"
        local stable="一致"
        for i in $(seq 2 "$runs"); do
            if ! cmp -s "$out/${cond}_1.txt" "$out/${cond}_${i}.txt"; then
                stable="不一致（${cond}_1 と ${cond}_${i}）"
                all_stable=1
            fi
        done
        echo "$cond: ${runs}回の実行どうし $stable"
    done
    return $all_stable
}

compare() {
    local base="$1" target="$2"
    local result=0
    for cond in $CONDITIONS; do
        if cmp -s "$base/$cond.txt" "$target/$cond.txt"; then
            echo "$cond: 一致"
        else
            echo "$cond: 不一致"
            diff "$base/$cond.txt" "$target/$cond.txt" | head -20
            result=1
        fi
    done
    return $result
}

case "${1:-}" in
    capture) shift; capture "$@" ;;
    compare) shift; compare "$@" ;;
    *) echo "使い方: $0 capture <src> <out> [runs] | compare <base> <target>"; exit 2 ;;
esac
