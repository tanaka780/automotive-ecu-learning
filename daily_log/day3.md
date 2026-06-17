# Day3

## 実施内容

- float と整数除算の違いを説明練習で確認（learning_journal 更新）
- アラートモジュール（alert.c / alert.h）の実装
- 閾値設計（speed: 100 km/h、rpm: 5000、temp: 90℃）
- main.c に alert_check の呼び出しを追加

---

## 変更・追加したファイル

| ファイル | 変更内容 |
| --- | --- |
| `include/alert.h` | `ALERT_*` 閾値定数と `alert_check` 関数宣言を追加（新規） |
| `src/alert.c` | `alert_check` を実装（新規） |
| `src/main.c` | `alert.h` のインクルード、`alert_check` の呼び出しを追加 |
| `Makefile` | `SRCS` に `src/alert.c` を追加 |
| `README.md` | 実装一覧・モジュール構成に alert を追記 |
| `project_context.md` | モジュール構成に alert を追記 |
| `study_plan.md` | アラートモジュール追加タスクを完了に更新 |
| `learning_journal.md` | 整数除算・#define・alert.c 責務を追記、「まだ理解が浅いこと」を解消 |

---

## 確認・検証

| 確認観点 | 結果 |
| --- | --- |
| ビルド（-Wall -Wextra -std=c11） | 成功（警告・エラーなし） |
| 実行（20サンプル＋アラート＋統計表示） | 成功 |
| 複数項目同時アラート（Sample 15） | Speed と Temp が同時に警告表示された |

```
[Sample 01]
Speed:  20 km/h | RPM: 5076 | Temp:  75 C
[ALERT] RPM   : 5076       (limit: 5000)
[Sample 02]
Speed:  44 km/h | RPM: 4471 | Temp:  94 C
[ALERT] Temp  :  94 C      (limit: 90 C)
[Sample 03]
Speed:  57 km/h | RPM: 3883 | Temp:  36 C
[Sample 04]
Speed:  62 km/h | RPM: 5894 | Temp:  49 C
[ALERT] RPM   : 5894       (limit: 5000)
[Sample 05]
Speed:  81 km/h | RPM: 5981 | Temp:  90 C
[ALERT] RPM   : 5981       (limit: 5000)
[Sample 06]
Speed:  30 km/h | RPM: 4266 | Temp:  71 C
[Sample 07]
Speed:  40 km/h | RPM: 5881 | Temp:  69 C
[ALERT] RPM   : 5881       (limit: 5000)
[Sample 08]
Speed:  84 km/h | RPM:  819 | Temp:  54 C
[Sample 09]
Speed: 118 km/h | RPM: 2680 | Temp:  79 C
[ALERT] Speed : 118 km/h  (limit: 100 km/h)
[Sample 10]
Speed:   5 km/h | RPM:  818 | Temp:  73 C
[Sample 11]
Speed:  70 km/h | RPM: 4872 | Temp:  67 C
[Sample 12]
Speed:  24 km/h | RPM: 1453 | Temp:  41 C
[Sample 13]
Speed:  41 km/h | RPM: 5337 | Temp:  67 C
[ALERT] RPM   : 5337       (limit: 5000)
[Sample 14]
Speed:  80 km/h | RPM: 2007 | Temp:  82 C
[Sample 15]
Speed: 112 km/h | RPM: 1039 | Temp:  96 C
[ALERT] Speed : 112 km/h  (limit: 100 km/h)
[ALERT] Temp  :  96 C      (limit: 90 C)
[Sample 16]
Speed:  44 km/h | RPM: 2940 | Temp:  57 C
[Sample 17]
Speed: 112 km/h | RPM: 5405 | Temp:  66 C
[ALERT] Speed : 112 km/h  (limit: 100 km/h)
[ALERT] RPM   : 5405       (limit: 5000)
[Sample 18]
Speed:  46 km/h | RPM: 3814 | Temp:  79 C
[Sample 19]
Speed:  89 km/h | RPM: 3982 | Temp:  48 C
[Sample 20]
Speed: 117 km/h | RPM: 5284 | Temp:  33 C
[ALERT] Speed : 117 km/h  (limit: 100 km/h)
[ALERT] RPM   : 5284       (limit: 5000)

--- Stats (20 samples) ---
Speed: min=  5  max=118  avg= 63 km/h
RPM  : min= 818  max=5981  avg=3795
Temp : min= 33  max= 96  avg= 66 C
```

## 次回の設計方針

- センサ状態判定は sensor.c・stats.c・alert.c とは別モジュール（status.c）にする
- status.c の責務を「センサ値の状態分類と表示」に限定するため
- 走行距離（speed × 経過時間）は概念として単純なため今回のフェーズでは省略する

---

## 次回やること

- センサ状態モジュールの追加（status.c / status.h）
- enum の導入（NORMAL / WARNING / CRITICAL の3段階）
- 各センサに2段階の閾値（warning / critical）を設定する
