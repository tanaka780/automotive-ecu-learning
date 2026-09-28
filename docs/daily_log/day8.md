# Day8

## 実施内容

- diag.cの並行変数(speed_crit_count等)を、enum SensorId + struct DtcEntry + 配列(entries[SENSOR_COUNT])に修正。

---

## 確認・検証

| 確認観点 | 結果 |
| --- | --- |
| ビルド（-Wall -Wextra -std=c11） | 警告・エラーなし。ビルド成功。 |
| 実行確認 | DTC件数はSpeed:1, RPM:3, Temp:1。RPM・TempともCRITICALが連続したサンプル(RPM12-13、Temp06-07)では2重に数えられておらず、エッジ検出は変更前と同じ動作だった。 |

---

## 次回の設計方針

- CRITICALが連続したサンプルで「重複登録防止の動作確認」が実質的に満たされたため、active/history（DTCの状態区分）を追加する。
- 想定した挙動になるか確認するためにrand()によるランダム値の代わりに、固定値のテストデータを使うか検討。

---

## 次回やること

- DTCの状態区分（現在も継続中か、過去に発生して解消済みか）を追加する。
