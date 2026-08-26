# Day31

## 実施内容

- study_plan.mdの「今後の候補テーマ」に、複数ECU化（エンジン監視ECU→メーターECU→ファン制御ECU、CAN接続）の構想をメモとして追加した
- `include/validate.h`・`src/validate.c`を新規作成。`SensorId`（sensor.h）をインデックスにした値域テーブル（speed 0〜120、rpm 0〜6000、temp 25〜100）を持たせ、汎用関数`validate_in_range`とLOG_LEVEL用の`validate_log_level`を実装した
- `src/config.c`の`apply_line`に、各キーの値を`ConfigData`へ代入する直前で`validate_in_range`/`validate_log_level`を挟んだ。値域外の値は既存の「未知のキーは無視する」パターンに合わせ、そのキーだけ無視する
- 全テストターゲット（test_diag/test_persist/test_stats/test_alert/test_ignition/test_cmd/test_config/test_fixture）に`src/validate.c`を追加した

## 確認内容

- `validate.c`追加後、`make clean && make`で警告・エラーが出ないこと
- `make test`で既存の全テストがPASSし、値域チェックの追加による既存動作への影響が無いこと

## 実行結果

- `make`：成功、警告・エラーなし
- `make test`：既存153件全てPASS（内訳はtest_diag 33／test_persist 17／test_stats 28／test_alert 16／test_ignition 4／test_cmd 25／test_config 16／test_fixture 14）

## 判定

成功。ビルド・既存テストへの影響が無いことを想定通り確認できた。ただしPhase12のタスクはタスク5までで、タスク6（validate.c自体の自動テスト追加、test_config.c/test_fixture.cへの値域外ケース追加）とタスク7（`make run`での実行確認）はまだ未着手。

## 今回の設計方針

- 入力妥当性チェックの置き場所として、(A)新規モジュール（validate.c/validate.h）に一元化、(B)config.c/fixture.cがそれぞれ個別に持つ、(C)使う側（alert.c/status.c等）でGuard Clauseとして防御する、の3案を比較した。persist.cを汎用化するか検討したときの判断基準（「2つ目の対象が実際に出てきたときに初めて汎用化する」）に照らすと、config.c・fixture.cという2つの具体的な呼び出し元が今の時点で既に存在するため、(A)の一元化を採用した
- 値域の基準（speed 0〜120等）をどこに置くかは、既存のsensor.hのコメントのままにするか、validate.hに定数として持たせるかで迷った。将来メーターECU・ファン制御ECUをCANで追加する構想（study_plan.mdの「今後の候補テーマ」参照）を踏まえ、validate.cを「config.cの付属品」ではなく「ECUドメイン共通の検証層」として位置付け、将来のフォルダ再編成（src/common）時にそのまま移動・再利用できるようにする必要があると判断し、validate.h側を正本とした。sensor.hのコメントは値域を重複させず「詳細はvalidate.h参照」の一言に留めた
- センサ種別（speed/rpm/temp）の値域チェックには、既存の`SensorId`をインデックスにしたテーブル（Lookup Table）を採用した。Phase1〜2で学んだ「並行変数をenum+配列にまとめる」パターンの延長で、センサ別に個別関数を3つ書く設計を避けられた
- 集約関数（min/max/sum等）やビットマスクによる複数センサの異常パターン列挙は、今回のテーマ（1つの値が物理的な範囲内かどうか）には合わないと判断し、採用しなかった
- LOG_LEVELは`SensorId`ファミリーに属さない（enumの離散値チェックであり、連続値の範囲チェックとは性質が異なる）ため、同じテーブルに混ぜず`validate_log_level`として別関数に分離した
- 値域チェックは`uint8_t`/`uint16_t`へキャストする前のint値に対して行うことにした。キャスト後にチェックすると、既にオーバーフロー（例：999が231に化ける等）した後の値を検証することになり意味が無くなるため
- 検証に失敗した場合の扱いは、既存の「未知のキー・値欠落・数値以外の行は無視する」というconfig.c/fixture.cのパースパターンと一貫させ、値域外の値もそのキーだけ無視し、ファイル全体は無効にしないことにした

## 次回やること

- Phase12のタスク6（`test/test_validate.c`の新規作成、`test_config.c`/`test_fixture.c`への値域外の異常系ケース追加）に着手する
- タスク7として、`config.txt`/`fixture.txt`に範囲外の値を書いた場合に、そのキーだけ無視され他のキーは反映されることを`make run`で実行確認する
