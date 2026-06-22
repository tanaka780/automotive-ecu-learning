# Day7

## 実施内容

- day6.mdで決めたDTCモジュール追加に進む前に、「CRITICAL検出時の記録」にはprevious_status / current_statusの比較が必要だと気づいた。
- 最初はstatus.cにprevious/currentを持たせる設計で実装したが、day6.mdの方針（status.cの分類結果を読むだけで使う、既存の責務分割を崩さない）と矛盾すると気づき、status.cは元の設計に戻した。
- 前回状態の保持はdiag.c側に移し、day6.mdで決めていたDTCモジュール（diag.h / diag.c）を実装した。

---

## 確認・検証

| 確認観点 | 結果 |
| --- | --- |
| ビルド（-Wall -Wextra -std=c11） | 成功（警告・エラーなし） |
| alert.c / status.c への影響 | 変更不要（uint8_t / uint16_t は比較時に int へ自動昇格） |

---

## 次回の設計方針

- 今のDTC管理（speed_crit_count / rpm_crit_count / temp_crit_count）は、センサごとに同じ処理を繰り返す「並行変数」になっている。
- enum（センサ種別）+ struct（1件分のDTC情報）+ 配列（複数件の管理）に直す。これは動的確保を使わない組み込みCで、同種のデータを複数件扱う際の標準的なパターン。
- study_plan.mdのPhase2拡張バックログの優先順位1番として、次回はこの保存方法の変更から着手する。

---

## 次回やること

- 同じロジックを複数回手書きする並行変数を配列管理に直す。
