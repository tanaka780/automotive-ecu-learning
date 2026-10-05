"""電源再投入（docs/scenarios.md）の結合テスト。

同じ一時ディレクトリでsensor_simを3回続けて実行し、1回目に保存されたdtc_data.txtが次の起動で
読み込まれることを、標準出力と照合して確認する。`make scenario`で実行する。約20秒×3回かかる。

- 1回目：config.txtだけを置き（dtc_data.txtは無い）、水温CRITICAL・イグニッションON固定で実行する
- 2回目：イグニッションOFF固定で実行する。OFFの間はDTCが更新されないため、最後に表示されるDTCが
  そのまま「起動直後に読み込まれた内容」になる（DTCの一覧は起動直後には表示されないため、この形で観測する）
- 3回目：1回目と違う値で水温CRITICAL・イグニッションON固定で実行する

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
from sensor_sim_runner import run_in_dir

# 開ければ読み込めた扱いになるconfig.txt（POSTの結果をdtc_data.txtの有無だけで決めるために置く）
VALID_CONFIG = "LOG_LEVEL=0\n"

FIRST_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=95\nIGNITION=ON\n"
SECOND_FIXTURE = "IGNITION=OFF\n"
# フリーズフレームが上書きされていないことを見分けるため、1回目と違う値にする
THIRD_FIXTURE = "MODE=FIXED\nSPEED=60\nRPM=2000\nTEMP=99\nIGNITION=ON\n"

FIRST_FREEZE_FRAME = "[DTC] Freeze Frame (trigger: Temp) Speed: 50 km/h RPM:2000 Temp: 95 C"


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
        """保存ファイルが無い初回起動は、DTC0件の初期状態から開始する（記録されるのは水温の1件だけ）"""
        self.assertIn("[PERSIST] No saved data (first run)", self.first_lines)
        self.assertIn("[DTC] Speed NONE     CRITICAL occurrences: 0", self.first_lines)
        self.assertIn("[DTC] RPM   NONE     CRITICAL occurrences: 0", self.first_lines)
        self.assertIn("[DTC] Temp  ACTIVE   CRITICAL occurrences: 1", self.first_lines)

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
        self.assertIn("[DTC] Temp  ACTIVE   CRITICAL occurrences: 1", self.second_lines)
        self.assertIn("[DTC] Speed NONE     CRITICAL occurrences: 0", self.second_lines)
        self.assertIn("[DTC] RPM   NONE     CRITICAL occurrences: 0", self.second_lines)

    def test_freeze_frame_inherited(self):
        """フリーズフレームも引き継がれる"""
        self.assertIn(FIRST_FREEZE_FRAME, self.second_lines)

    def test_occurrence_counted_again_after_reboot(self):
        """前回状態がNORMALに戻っているため、再起動後もCRITICALなら発生回数がもう1つ増える"""
        self.assertIn("[STATUS] Speed: NORMAL   | RPM: NORMAL   | Temp: CRITICAL", self.third_lines)
        self.assertIn("[DTC] Temp  ACTIVE   CRITICAL occurrences: 2", self.third_lines)

    def test_freeze_frame_not_overwritten_after_reboot(self):
        """再起動後にCRITICALが発生しても、フリーズフレームは最初の値（Speed 50・Temp 95）のまま"""
        self.assertIn("Speed:  60 km/h | RPM: 2000 | Temp:  99 C", self.third_lines)
        self.assertIn(FIRST_FREEZE_FRAME, self.third_lines)


if __name__ == "__main__":
    unittest.main()
