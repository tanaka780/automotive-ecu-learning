"""電源再投入（docs/scenarios.md）の結合テスト。

同じ一時ディレクトリでsensor_simを3回続けて実行し、1回目に保存されたdtc_data.txtが次の起動で
読み込まれることを、標準出力と照合して確認する。`make scenario`で実行する。約20秒×3回かかる。

- 1回目：config.txtだけを置き（dtc_data.txtは無い）、水温CRITICAL・イグニッションON固定で実行する
- 2回目：イグニッションOFF固定で実行する。OFFの間はDTCが更新されないため、最後に表示されるDTCが
  そのまま「起動直後に読み込まれた内容」になる（DTCの一覧は起動直後には表示されないため、この形で観測する）
- 3回目：1回目と違う値で水温CRITICAL・イグニッションON固定で実行する

別に、前の形（値11個）のdtc_data.txtを置いて1回起動し、壊れたファイルと同じく扱うことも確認する（Phase28）。
これは仮想時間版（sensor_sim_virtual）で実行する。確かめたいのは保存ファイルの読み込みと起動時自己診断の配線で、
本番版と同じコードのため。仮想時間版は出力が毎回同じで、数ミリ秒で終わる。

期待結果のうち「メーターECUの通信異常の記録は永続化していない」は、Phase25で対象外とした
通信故障に当たるため検証しない。
"""
import os
import sys
import tempfile
import unittest

# リポジトリ直下からモジュール名で実行した場合（python3 -m unittest scenario_test.test_xxx）も
# sensor_sim_runnerが見つかるよう、このフォルダをimportの検索先に加える
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sensor_sim_runner import SENSOR_SIM_VIRTUAL, run_in_dir

# 開ければ読み込めた扱いになるconfig.txt（POSTの結果をdtc_data.txtの有無だけで決めるために置く）
VALID_CONFIG = "LOG_LEVEL=0\n"

FIRST_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=115\nIGNITION=ON\n"
SECOND_FIXTURE = "IGNITION=OFF\n"
# フリーズフレームが上書きされていないことを見分けるため、1回目と違う値にする
THIRD_FIXTURE = "MODE=FIXED\nSPEED=60\nRPM=2000\nTEMP=119\nIGNITION=ON\n"

FIRST_FREEZE_FRAME = "[DTC] Freeze Frame (trigger: Overheat) Speed: 50 km/h RPM:2000 Temp:115 C"


class PowerCycleTest(unittest.TestCase):
    """1回目の起動で記録したDTCが、2回目・3回目の起動に引き継がれる。"""

    @classmethod
    def setUpClass(cls):
        # 3回とも同じディレクトリで実行し、前回保存されたdtc_data.txtを次の起動で読み込ませる
        with tempfile.TemporaryDirectory() as workdir:
            cls.first = run_in_dir(workdir, {"config.txt": VALID_CONFIG, "fixture.txt": FIRST_FIXTURE})
            cls.second = run_in_dir(workdir, {"fixture.txt": SECOND_FIXTURE})
            cls.third = run_in_dir(workdir, {"fixture.txt": THIRD_FIXTURE})
        cls.first_lines = cls.first.stdout.splitlines()
        cls.second_lines = cls.second.stdout.splitlines()
        cls.third_lines = cls.third.stdout.splitlines()

    def test_all_runs_complete(self):
        """前提：3回とも異常終了せず、サンプルループが最後まで実行される"""
        for result in (self.first, self.second, self.third):
            self.assertEqual(0, result.returncode)
            self.assertIn("[Sample 20]", result.stdout.splitlines())

    def test_first_boot_starts_from_no_dtc(self):
        """保存ファイルが無い初回起動は、DTC0件の初期状態から開始する（記録されるのはオーバーヒートの1件だけ）"""
        self.assertIn("[PERSIST] No saved data (first run)", self.first_lines)
        self.assertIn("[DTC] Speed      NONE     occurrences: 0", self.first_lines)
        self.assertIn("[DTC] RPM        NONE     occurrences: 0", self.first_lines)
        self.assertIn("[DTC] TempSensor NONE     occurrences: 0", self.first_lines)
        self.assertIn("[DTC] Overheat   ACTIVE   occurrences: 1", self.first_lines)

    def test_dtc_saved_at_end_of_first_boot(self):
        """1回目の終了時にDTC記録がdtc_data.txtへ保存される"""
        self.assertIn("[PERSIST] Saved DTC data", self.first_lines)

    def test_post_depends_on_saved_dtc(self):
        """POSTは、dtc_data.txtが無い1回目はdid not pass、config.txtと両方読み込める2回目はpassed"""
        self.assertIn("[POST] Self-check did not pass, continuing with defaults", self.first_lines)
        self.assertIn("[POST] Self-check passed", self.second_lines)

    def test_dtc_inherited_right_after_reboot(self):
        """2回目の起動直後、1回目で記録されたDTCの発生回数・状態区分が引き継がれている"""
        # 前提：2回目はOFFのままで、DTCが更新される処理が一度も動いていない
        self.assertIn("[FIXTURE] Ignition fixed: OFF", self.second_lines)
        self.assertNotIn("[IGN] ON", self.second_lines)
        self.assertIn("[PERSIST] Loaded saved DTC data", self.second_lines)
        self.assertIn("[DTC] Overheat   ACTIVE   occurrences: 1", self.second_lines)
        self.assertIn("[DTC] TempSensor NONE     occurrences: 0", self.second_lines)
        self.assertIn("[DTC] Speed      NONE     occurrences: 0", self.second_lines)
        self.assertIn("[DTC] RPM        NONE     occurrences: 0", self.second_lines)

    def test_freeze_frame_inherited(self):
        """フリーズフレームも引き継がれる"""
        self.assertIn(FIRST_FREEZE_FRAME, self.second_lines)

    def test_occurrence_counted_again_after_reboot(self):
        """前回状態がNORMALに戻っているため、再起動後もCRITICALなら発生回数がもう1つ増える"""
        self.assertIn("[STATUS] Speed: NORMAL   | RPM: NORMAL   | Temp: CRITICAL", self.third_lines)
        self.assertIn("[DTC] Overheat   ACTIVE   occurrences: 2", self.third_lines)

    def test_freeze_frame_not_overwritten_after_reboot(self):
        """再起動後にCRITICALが発生しても、フリーズフレームは最初の値（Speed 50・Temp 115）のまま"""
        self.assertIn("Speed:  60 km/h | RPM: 2000 | Temp: 119 C", self.third_lines)
        self.assertIn(FIRST_FREEZE_FRAME, self.third_lines)


# 記録の種類を分ける前（Phase27まで）の形のdtc_data.txt。センサ3つ×（回数・状態区分）とフリーズフレームの5個で11個。
# 0件から始まったことを見分けるため、回数とフリーズフレームは0以外にする
OLD_FORMAT_DTC_DATA = "3\n2\n1\n2\n4\n1\n1\n0\n110\n2000\n70\n"


class OldFormatSaveFileTest(unittest.TestCase):
    """前の形（値11個）のdtc_data.txtを置いて起動する（Phase28）。

    イグニッションOFF固定で動かし、最後に表示されるDTCを「起動直後に読み込まれた内容」として見る
    （PowerCycleTestの2回目と同じ見方）。
    """

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as workdir:
            cls.result = run_in_dir(workdir, {"config.txt": VALID_CONFIG, "dtc_data.txt": OLD_FORMAT_DTC_DATA,
                                              "fixture.txt": SECOND_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.lines = cls.result.stdout.splitlines()

    def test_runs_to_the_end(self):
        """前提：異常終了せず、サンプルループが最後まで実行される。OFFのままでDTCが更新されない"""
        self.assertEqual(0, self.result.returncode)
        self.assertIn("[Sample 20]", self.lines)
        self.assertNotIn("[IGN] ON", self.lines)

    def test_treated_as_corrupted(self):
        """壊れたファイルと同じく扱い、POSTはdid not passになる（config.txtは読み込めている）"""
        self.assertIn("[PERSIST] Saved data is corrupted, ignoring", self.lines)
        self.assertIn("[POST] Self-check did not pass, continuing with defaults", self.lines)

    def test_starts_from_no_dtc(self):
        """前の形の記録は引き継がず、DTC0件・フリーズフレームなしの初期状態から開始する"""
        self.assertIn("[DTC] Speed      NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] RPM        NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] TempSensor NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] Overheat   NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] Freeze Frame: not captured (no DTC occurred)", self.lines)


if __name__ == "__main__":
    unittest.main()
