# Day2

## 実施内容

- `&` とポインタの概念を説明練習で確認
- 統計モジュール（stats.c / stats.h）の実装
- stats の処理フローを確認

---

## 変更・追加したファイル

| ファイル | 変更内容 |
| --- | --- |
| `include/stats.h` | `VehicleStats` struct と関数宣言を追加（新規） |
| `src/stats.c` | `stats_init` / `stats_update` / `stats_print` を実装（新規） |
| `src/main.c` | `stats.h` のインクルード、stats の初期化・更新・表示を追加 |
| `Makefile` | `SRCS` に `src/stats.c` を追加 |

---

## 確認・検証

| 確認観点 | 結果 |
| --- | --- |
| ビルド（-Wall -Wextra -std=c11） | 成功（警告・エラーなし） |
| 実行（20サンプル＋統計表示） | 成功 |

```
[Sample 01]
Speed:  73 km/h | RPM: 2753 | Temp:  73 C
[Sample 02]
Speed:  68 km/h | RPM: 4501 | Temp:  95 C
[Sample 03]
Speed:  57 km/h | RPM: 2883 | Temp:  51 C
[Sample 04]
Speed: 113 km/h | RPM: 2236 | Temp:  94 C
[Sample 05]
Speed:  12 km/h | RPM: 5717 | Temp:  56 C
[Sample 06]
Speed: 102 km/h | RPM: 1009 | Temp:  50 C
[Sample 07]
Speed:  25 km/h | RPM: 3761 | Temp:  32 C
[Sample 08]
Speed:  60 km/h | RPM: 2442 | Temp:  97 C
[Sample 09]
Speed:  22 km/h | RPM: 2963 | Temp:  98 C
[Sample 10]
Speed:  76 km/h | RPM: 4910 | Temp:  52 C
[Sample 11]
Speed:  76 km/h | RPM: 5584 | Temp:  66 C
[Sample 12]
Speed:  60 km/h | RPM: 2437 | Temp:  50 C
[Sample 13]
Speed:  97 km/h | RPM: 5922 | Temp:  95 C
[Sample 14]
Speed:  62 km/h | RPM: 4623 | Temp:  52 C
[Sample 15]
Speed:  28 km/h | RPM: 5818 | Temp:  68 C
[Sample 16]
Speed:  96 km/h | RPM: 5862 | Temp:  84 C
[Sample 17]
Speed:  74 km/h | RPM: 5239 | Temp:  37 C
[Sample 18]
Speed: 109 km/h | RPM: 2444 | Temp:  89 C
[Sample 19]
Speed: 113 km/h | RPM: 3152 | Temp: 100 C
[Sample 20]
Speed:  92 km/h | RPM: 2585 | Temp:  37 C

--- Stats (20 samples) ---
Speed: min= 12  max=113  avg= 70 km/h
RPM  : min=1009  max=5922  avg=3842
Temp : min= 32  max=100  avg= 68 C
```

---

## 気づき・メモ

- `&` は変数に自動で割り当てられた住所（番地）を調べる演算子
- `*data` は住所を受け取るための変数（ポインタ）。値ではなく住所そのものが入っている
- 値をそのまま渡すとデータのコピーが作られる。コピーを書き換えても元データは変わらない。住所を渡すと元データを直接書き換えられる
- `stats.c` を別モジュールにした理由: `sensor.c` は「今の値」、`stats.c` は「過去の記録」で責務が違うため
- `stats_update` の第2引数に `const` をつけた理由: センサ値を読むだけで書き換えないことを型で明示するため
- `INT_MAX` で最小値を初期化する理由: 最初のサンプルが何の値でも必ず「今の値 < INT_MAX」が成立し、確実に更新されるため

---

## 次回の設計方針

- アラート機能は sensor.c・stats.c とは別モジュール（alert.c）にする
- alert.c の責務を「閾値チェックと警告出力」に限定するため

---

## 次回やること

- アラートモジュールの追加（alert.c / alert.h）
- 閾値設計（speed・rpm・temp それぞれの上限値を決める）

