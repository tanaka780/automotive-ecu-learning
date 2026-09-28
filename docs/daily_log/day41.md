# Day41

## 実施内容

- 前回（Day40）の「次回やること」に沿ってSchedulerを検討したが、実装・Phase化はせず、着手要否の判断のみを行った
- 判断に先立ち、直近の実装（Phase15 faultmgr・Phase18 timer）を理解確認した：faultmgr.cのDebounce（CRITICAL連続回数で確定させる理由）、timer_is_dueとfaultmgrのDebounceカウントの役割の違い（時間ベース vs サンプル回数ベース）、main.cの処理順序（status_check→diag_check→faultmgr_check→faultmgr_apply_safe_values→alert_check/stats_update）がなぜその順でなければならないか

## 今回の設計方針

- Scheduler着手の要否を、①今の単一周期（1000ms）構造で実際に困っている点があるか、②Schedulerが必要になる具体的な場面（CAN通信の複数メッセージ周期、Watchdog）が今あるかの2点で判断した。どちらも今日の時点では該当しないため、今日この場でのScheduler実装・Phase化はしないことにした
- study_plan.mdの複数ECU化の着手順序（Fail-safe→DTO整理→Timer→Scheduler→CAN通信→フォルダ分割）自体は変更しない。「Timerの次だから機械的にSchedulerに進む」のではなく、CAN通信に着手する回の最初にSchedulerを挟む（CANが要求する周期を踏まえて設計判断する）という位置づけで進める。これはPhase17（DTO整理）をCAN Phase着手直前に決定だけ済ませたのと同じパターンで、「今具体的な必要性が無いままPhase化しない」というCLAUDE.mdの判断順序（①ストーリー上の必要性）に従った結果
- 次回のテーマをCAN通信に着手することと決めたため、Schedulerの具体的な必要性（②）は次回時点で満たされることが確定した。よって次回冒頭でSchedulerの検討・実装を行ってからCAN本体に進む

## 次回やること

- CAN通信に着手する。着手にあたり、まずScheduler（Phase番号は次回採番）を検討・実装し、CANが要求する周期（メッセージごとの送受信レート等）をタスクとして扱える形にしてからCAN本体の実装に進む
