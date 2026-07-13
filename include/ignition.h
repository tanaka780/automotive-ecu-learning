/* インクルードガード */
#ifndef IGNITION_H
#define IGNITION_H

/* イグニッション状態：停止(OFF)か始動中(ON)かの2値 */
typedef enum {
    IGNITION_OFF = 0,
    IGNITION_ON
} IgnitionState;

/* 現在と前回のイグニッション状態を保持し、遷移(エッジ)検出に使う */
typedef struct {
    IgnitionState current;
    IgnitionState previous;
} Ignition;

/* イグニッション状態を初期値(OFF)に設定する */
void ignition_init(Ignition *ignition);
/* イグニッション状態をランダム値で更新する（ECUの電源状態の変化を模倣） */
void ignition_update(Ignition *ignition);
/* 前回と今回の状態を比較し、遷移した瞬間だけイベントとして表示する */
void ignition_check(const Ignition *ignition);
/* 現在のイグニッション状態を1行でコンソールに出力する */
void ignition_print(const Ignition *ignition);

#endif /* IGNITION_H */
