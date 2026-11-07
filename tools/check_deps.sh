#!/bin/bash
# 依存の向き（ECUのフォルダ → common/ → mcal_*/）が守られているかを確かめる（Phase31）
#
# 使い方: リポジトリの一番上で bash tools/check_deps.sh（make のたびに Makefile から呼ばれる）
#
# src/ の下のフォルダを3種類に分け、読んでよいフォルダだけを -I に渡して、.c と .h を1つずつ
# コンパイルにかける（gcc -fsyntax-only。文法とヘッダの読み込みだけを確かめ、実行ファイルは作らない）。
#   mcal_*/  : いちばん下のドライバ。自分のフォルダだけを読める
#   common/  : 共通部品。自分のフォルダと mcal_*/ を読める
#   それ以外 : ECUのフォルダ。自分のフォルダと common/・mcal_*/ を読める（ほかのECUのフォルダは読めない）
# src/ 直下（main.c・cmd.c・persist.c）・sim/・test/ は確かめない（ECUをまたぐこと、全部を読むことが分かっているため）。
# src/ 直下と sim/ は -I に入れないので、フォルダの中から cmd.h・persist.h や試験台のヘッダを読むと落ちる。
#
# エラーに「〜.h: No such file or directory」と出たら、そのフォルダからは読んではいけないヘッダを読んでいる。
# ECUではないフォルダ（たとえば層を1つ足すとき）を作ると、ECUとして扱われて落ちる。そのときはここの分け方を直す。
#
# -I を絞るだけではすり抜ける書き方も、合わせて止める
#   - 宣言の無い関数の呼び出し（ヘッダを読まずにほかのフォルダの関数を呼べてしまう）: -Werror=implicit-function-declaration
#   - .c の中の extern の宣言（自分で宣言を書けば、ヘッダを読まずに呼べてしまう）: grep
#   - #include "../〜"（-I に関係なく、ほかのフォルダのヘッダを読めてしまう）: grep
# 違反は全部表示してから、最後に失敗で終わる。確かめたファイルの数が0の種類があれば、それも失敗にする
# （探す場所を誤って、何も確かめずに OK を出すのを防ぐため）

cd "$(dirname "$0")/.." || exit 2

MCAL_DIRS=""
for d in src/mcal_*/; do
    [ -d "$d" ] && MCAL_DIRS="$MCAL_DIRS ${d%/}"
done

fail=0
n_mcal=0
n_common=0
n_ecu=0
n_ecu_dirs=0

# $1: 確かめるファイル、残り: 読んでよいフォルダ
check_file() {
    local file="$1"
    shift
    local inc=""
    for d in "$@"; do
        inc="$inc -I$d"
    done
    local lang=""
    case "$file" in
        *.h) lang="-x c" ;;
    esac
    # shellcheck disable=SC2086
    if ! gcc -std=c11 -fsyntax-only -Werror=implicit-function-declaration $inc $lang "$file"; then
        echo "check_deps: NG $file（読んでよいフォルダ:$*）"
        fail=1
    fi
}

for dir in src/*/; do
    dir="${dir%/}"
    case "$dir" in
        src/mcal_*)
            for f in "$dir"/*.c "$dir"/*.h; do
                [ -f "$f" ] || continue
                check_file "$f" "$dir"
                n_mcal=$((n_mcal + 1))
            done
            ;;
        src/common)
            for f in "$dir"/*.c "$dir"/*.h; do
                [ -f "$f" ] || continue
                # shellcheck disable=SC2086
                check_file "$f" "$dir" $MCAL_DIRS
                n_common=$((n_common + 1))
            done
            ;;
        *)
            n_ecu_dirs=$((n_ecu_dirs + 1))
            for f in "$dir"/*.c "$dir"/*.h; do
                [ -f "$f" ] || continue
                # shellcheck disable=SC2086
                check_file "$f" "$dir" src/common $MCAL_DIRS
                n_ecu=$((n_ecu + 1))
            done
            ;;
    esac
done

# -I を絞るだけではすり抜ける書き方（src/ 直下は対象外）
if grep -n '#include *"\.\./' src/*/*.c src/*/*.h; then
    echo "check_deps: NG 上の行は ../ でほかのフォルダのヘッダを読んでいる"
    fail=1
fi
if grep -n '^[[:space:]]*extern[[:space:]]' src/*/*.c; then
    echo "check_deps: NG 上の行は .c の中で extern の宣言を書いている（宣言はヘッダに書いて読む）"
    fail=1
fi

echo "check_deps: mcal ${n_mcal}個、common ${n_common}個、ECU ${n_ecu_dirs}フォルダ ${n_ecu}個のファイルを確かめた"
if [ "$n_mcal" -eq 0 ] || [ "$n_common" -eq 0 ] || [ "$n_ecu" -eq 0 ]; then
    echo "check_deps: NG 確かめたファイルが0個の種類がある（フォルダの探し方を確かめる）"
    fail=1
fi

if [ "$fail" -ne 0 ]; then
    echo "check_deps: 依存の向きが守られていない"
    exit 1
fi
echo "check_deps: OK"
